#include "memscan.hpp"

#include <handleapi.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memoryapi.h>

namespace mem_scan
{
    void updateSearchForString(MemBlock* mb, 
                               int bytesRead, 
                               int totalRead, 
                               int dataSize, 
                               char* tempBuf, 
                               char* val, 
                               Condition condition)
    {
        uint32_t offset;
        
        for (offset = 0; offset < bytesRead; offset += 1) 
        {
            bool isMatch;
            if (mem_scan::isInSearch(mb, (totalRead+offset)))
            {
                switch (condition) 
                {
                    case COND_EQUALS:
                        isMatch = (std::memcmp(&tempBuf[offset], val, mb->dataSize) == 0);
                        break;
                    case COND_INCREASED:
                        isMatch = (std::memcmp(&tempBuf[offset], val, mb->dataSize) > 0);
                        break;
                    case COND_DECREASED:
                        isMatch = (std::memcmp(&tempBuf[offset], val, mb->dataSize) < 0);
                        break;
                    default:
                        break;
                }

                if (isMatch) 
                {
                    mb->matches++;
                    offset = offset + mb->dataSize;
                } 
                else
                {
                    mem_scan::removeFromSearch(mb, (totalRead+offset));
                }
            }
        }
    }

    void updateMemBlockString(mem_scan::MemBlock* mb, mem_scan::Condition condition, char* val)
    {
        char tempBuf[1000*mb->dataSize];
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
                ReadProcessMemory(mb->pHandle, mb->addr.get() + totalRead, tempBuf, bytesToRead, &bytesRead);
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
                    updateSearchForString(mb, bytesRead, totalRead, mb->dataSize, tempBuf, val, condition);
                }

                std::memcpy(mb->buffer.data() + totalRead, tempBuf, bytesRead);
                bytesLeft -= bytesRead;
                totalRead += bytesRead;
            }

            mb->size = totalRead;
        }
    }

    void updateScanForString(mem_scan::MemBlock* mbLinked, mem_scan::Condition condition, char* val) 
    {
        mem_scan::MemBlock* mb = mbLinked;

        while (mb) 
        {
            updateMemBlockString(mb, condition, val);
            mb = mb->next;
        }
    }

    void pokeString(HANDLE pHandle, uintptr_t addr, char* val, int size)
    {
        if (WriteProcessMemory(pHandle, reinterpret_cast<uintptr_t*>(addr), val, size, nullptr) == 0)
        {
            std::cout << "poke failed\r\n";
        }
    }

    char* peekString(HANDLE pHandle, uintptr_t addr, int size)
    {
        static char val[32];

	    if (ReadProcessMemory(pHandle, reinterpret_cast<uintptr_t*>(addr), val, size, nullptr) == 0)
        {
            std::cout << "peek failed\r\n";
        }

        return val;
    }
}