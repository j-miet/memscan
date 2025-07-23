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
                               char tempBuf[], 
                               char* val, 
                               Condition condition)
    {
        uint32_t offset;
        
        for (offset = 0; offset < bytesRead; offset += dataSize) 
        {
            bool isMatch = false;
            if (mem_scan::isInSearch(mb, (totalRead+offset)))
            {
                switch (condition) 
                {
                    case COND_EQUALS:
                        isMatch = (memcmp(&tempBuf[offset], val, mb->dataSize) == 0);
                        break;
                    case COND_INCREASED:
                        isMatch = (memcmp(&tempBuf[offset], val, mb->dataSize) > 0);
                        break;
                    case COND_DECREASED:
                        isMatch = (memcmp(&tempBuf[offset], val, mb->dataSize) < 0);
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

    void updateMemBlockString(mem_scan::MemBlock* mb, mem_scan::Condition condition, char* val)
    {
        char tempBuf[1024*128];
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

    void pokeString(HANDLE pHandle, intptr_t addr, const char* val)
    {
        if (WriteProcessMemory(pHandle, reinterpret_cast<intptr_t*>(addr), val, sizeof(val), nullptr) == 0)
        {
            std::cout << "poke failed\r\n";
        }
    }

    char* peekString(HANDLE pHandle, intptr_t addr)
    {
        static char val[32];

	    if (ReadProcessMemory(pHandle, reinterpret_cast<intptr_t*>(addr), val, sizeof(val)+1, nullptr) == 0)
        {
            std::cout << "peek failed\r\n";
        }

        return val;
    }
}