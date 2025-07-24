#include "memscan.hpp"

#include <handleapi.h>

#include <cstdint>
#include <cstring>
#include <iostream>
#include <memoryapi.h>


namespace mem_scan
{
    void updateSearchForInt(mem_scan::MemBlock* mb, 
                            int bytesRead, 
                            int totalRead, 
                            int dataSize, 
                            std::array<int8_t, 1024*128> tempBuf, 
                            int64_t val, 
                            mem_scan::Condition condition)
    {
        int64_t offset;

        for (offset = 0; offset < bytesRead; offset += dataSize) 
        {
            if (mem_scan::isInSearch(mb, (totalRead+offset)))
            {
                bool isMatch = false; 
                int64_t tempVal;
                int64_t prevVal = 0;
                char* tempBuffer;
                char* prevBuffer;

                switch (mb->dataSize)
                {
                    case 1:
                        tempVal = tempBuf[offset];
                        prevVal = mb->buffer[totalRead+offset];
                        break;
                    case 2:
                        tempVal = *reinterpret_cast<int16_t*>(&tempBuf[offset]);
                        prevVal = *reinterpret_cast<int16_t*>(&mb->buffer[totalRead+offset]);
                        break;
                    case 4:
                        tempVal = *reinterpret_cast<int32_t*>(&tempBuf[offset]);
                        prevVal = *reinterpret_cast<int32_t*>(&mb->buffer[totalRead+offset]);
                        break;
                    case 8:
                    default:
                        tempVal = *reinterpret_cast<int64_t*>(&tempBuf[offset]);
                        prevVal = *reinterpret_cast<int64_t*>(&mb->buffer[totalRead+offset]);
                        break;
                }
                
                switch (condition) 
                {
                    case COND_EQUALS:
                        isMatch = (tempVal == val);
                        break;
                    case COND_INCREASED:
                        isMatch = (tempVal > prevVal);
                        break;
                    case COND_DECREASED:
                        isMatch = (tempVal < prevVal);
                        break;
                    default:
                        break;
                }

                if (isMatch) 
                {
                    mb->matches++;
                } 
                else
                {
                    mem_scan::removeFromSearch(mb, (totalRead+offset));
                }
            }
        }
    }

    /**
     * \brief Read information into memory block buffer by applying a search condition
     * 
     * \param mb Block address 
     * \param condition Search condition
     * \param val Comparison value when condition is COND_EQUALS
     */
    void updateMemBlockInt(mem_scan::MemBlock* mb, 
                           mem_scan::Condition condition, 
                           int64_t val) 
    {
        /* using std::vector<int8_t> tempBuf(1024*128) slows the update process enormously. 
        No idea why it happens and how to fix it. Instead, std::array is used and it operates as expected. */

        std::array<int8_t, 1024*128> tempBuf;
        int bytesLeft;
        int totalRead;
        int bytesToRead;
        size_t bytesRead;

        if (mb->matches > 0) 
        {
            bytesLeft = mb->size;
            totalRead = 0;
            mb->matches = 0;

            while (bytesLeft) 
            {
                bytesToRead = (bytesLeft > sizeof(tempBuf)) ? sizeof(tempBuf) : bytesLeft;
                ReadProcessMemory(mb->pHandle, mb->addr.get() + totalRead, tempBuf.data(), bytesToRead, &bytesRead);
                if (bytesRead != bytesToRead)
                {
                    break;
                }
                if (condition == COND_UNCONDITIONAL) 
                {
                    std::fill(mb->searchMask.begin()+totalRead/8, mb->searchMask.begin()+totalRead/8+bytesRead/8, 0xff);
                    mb->matches += bytesRead;
                } 
                else 
                {
                    updateSearchForInt(mb, bytesRead, totalRead, mb->dataSize, tempBuf, val, condition);
                }

                // memcpy instead of copy as it copies raw byte data (also copy just crashes entire program, no idea why)
                // i.e. this doesn't work -> std::copy(tempBuf.begin(), tempBuf.end(), mb->buffer.begin() + totalRead);
                std::memcpy(mb->buffer.data() + totalRead, tempBuf.data(), bytesRead);
                bytesLeft -= bytesRead;
                totalRead += bytesRead;
            }

            mb->size = totalRead;
        }
    }

    /**
     * \brief Update all memory blocks
     * 
     * \param mbLinked Linked list
     * \param condition Search condition
     * \param int Value to compare to if condition has value COND_EQUALS
     */
    void updateScanForInt(mem_scan::MemBlock* mbLinked, mem_scan::Condition condition, int64_t val) 
    {
        MemBlock* mb = mbLinked;

        while (mb) 
        {
            updateMemBlockInt(mb, condition, val);
            mb = mb->next;
        }
    }
    
    void pokeInt8(HANDLE pHandle, intptr_t addr, int8_t val)
    {
        if (WriteProcessMemory(pHandle, reinterpret_cast<int8_t*>(addr), &val, 1, nullptr) == 0)
        {
            std::cout << "poke failed\r\n";
        }
    }

    void pokeInt16(HANDLE pHandle, intptr_t addr, int16_t val)
    {
        if (WriteProcessMemory(pHandle, reinterpret_cast<int16_t*>(addr), &val, 2, nullptr) == 0)
        {
            std::cout << "poke failed\r\n";
        }
    }

    void pokeInt32(HANDLE pHandle, intptr_t addr, int32_t val)
    {
        if (WriteProcessMemory(pHandle, reinterpret_cast<int32_t*>(addr), &val, 4, nullptr) == 0)
        {
            std::cout << "poke failed\r\n";
        }
    }

    /**
     * \brief Write into a memory location
     * 
     * \param pHandle Process handle
     * \param dataSize Data unit size
     * \param addr Base address
     * \param val New value
     */
    void pokeInt64(HANDLE pHandle, intptr_t addr, int64_t val)
    {
        if (WriteProcessMemory(pHandle, reinterpret_cast<int64_t*>(addr), &val, 8, nullptr) == 0)
        {
            std::cout << "poke failed\r\n";
        }
    }

    /**
     * \brief Read a memory location
     * 
     * \param pHandle Process handle
     * \param dataSize Data unit size
     * \param addr Base address
     * 
     * \return Value stored in base address
     */
    int64_t peekInt(HANDLE pHandle, int dataSize, intptr_t addr)
    {
        int64_t val = 0;

        if (ReadProcessMemory(pHandle, reinterpret_cast<intptr_t*>(addr), &val, dataSize, nullptr) == 0)
        {
            std::cout << "peek failed\r\n";
        }

        return val;
    }
}