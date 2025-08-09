#include "intscanner.hpp"

#include <cstring>
#include <iostream>
#include <memoryapi.h>

IntScanner::IntScanner(std::vector<MemBlock> memblocks): m_pHandle(memblocks[0].pHandle()), m_memblocks(memblocks)
{}

IntScanner::~IntScanner()
{
    CloseHandle(m_pHandle);
}

void IntScanner::updateSearch(MemBlock& mb, 
                              int bytesRead, 
                              std::string& tempBuf, 
                              Condition condition, 
                              int64_t val)
{
    for (int offset = 0; offset < bytesRead; offset += mb.dataSize()) 
    {
        if (mb.isInSearch(offset))
        {
            bool isMatch = false; 
            int64_t tempVal;
            int64_t prevVal = 0;
            char* tempBuffer;
            char* prevBuffer;

            switch (mb.dataSize())
            {
                case 1:
                    tempVal = tempBuf[offset];
                    prevVal = mb.buffer()[offset];
                    break;
                case 2:
                    tempVal = *reinterpret_cast<int16_t*>(&tempBuf[offset]);
                    prevVal = *reinterpret_cast<int16_t*>(&mb.buffer()[offset]);
                    break;
                case 4:
                default:
                    tempVal = *reinterpret_cast<int32_t*>(&tempBuf[offset]);
                    prevVal = *reinterpret_cast<int32_t*>(&mb.buffer()[offset]);
                    break;
                case 8:
                    tempVal = *reinterpret_cast<int64_t*>(&tempBuf[offset]);
                    prevVal = *reinterpret_cast<int64_t*>(&mb.buffer()[offset]);
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
                mb.matches() = mb.matches()+1;
            } 
            else
            {
                mb.removeFromSearch(offset);
            }
        }
    }
}

void IntScanner::updateMemBlock(MemBlock& mb, Condition condition, int64_t val)
{
    std::string tempBuf;
    int bytesToRead;
    SIZE_T bytesRead;

    tempBuf.resize(mb.size());

    if (mb.size() > 0) 
    {
        mb.matches() = 0;

        bytesToRead = mb.size();
        ReadProcessMemory(m_pHandle, mb.addr(), &tempBuf[0], bytesToRead, &bytesRead);
        if (condition == COND_UNCONDITIONAL) 
        {
            std::fill(mb.searchMask().begin(), mb.searchMask().begin()+bytesRead/8, 0xff);
            mb.matches() += bytesRead;
        } 
        else
        {
            updateSearch(mb, bytesRead, tempBuf, condition, val);
        }

        std::memcpy(mb.buffer().data(), &tempBuf[0], bytesRead);
        mb.size() = bytesRead;
    }
}

void IntScanner::updateScan(Condition condition, int64_t val) 
{
    for (auto& mb : m_memblocks)
    {
        updateMemBlock(mb, condition, val);
    }
}

void IntScanner::writeInt8(uintptr_t addr, char val)
{
    if (!WriteProcessMemory(m_pHandle, reinterpret_cast<uintptr_t*>(addr), &val, 1, nullptr))
    {
        std::cout << "writing failed\r\n";
    }
}

void IntScanner::writeInt16(uintptr_t addr, int16_t val)
{
    if (!WriteProcessMemory(m_pHandle, reinterpret_cast<uintptr_t*>(addr), &val, 2, nullptr))
    {
        std::cout << "writing failed\r\n";
    }
}

void IntScanner::writeInt32(uintptr_t addr, int32_t val)
{
    if (!WriteProcessMemory(m_pHandle, reinterpret_cast<uintptr_t*>(addr), &val, 4, NULL))
    {
        std::cout << "writing failed\r\n";
    }
}

void IntScanner::writeInt64(uintptr_t addr, int64_t val)
{
    if (!WriteProcessMemory(m_pHandle, reinterpret_cast<uintptr_t*>(addr), &val, 8, nullptr))
    {
        std::cout << "writing failed\r\n";
    }
}

char IntScanner::readInt8(uintptr_t addr)
{
    char val = 0;

    if (!ReadProcessMemory(m_pHandle, reinterpret_cast<char*>(addr), &val, 1, nullptr))
    {
        std::cout << "reading failed\r\n";
    }

    return val;
}

int16_t IntScanner::readInt16(uintptr_t addr)
{
    int16_t val = 0;

    if (!ReadProcessMemory(m_pHandle, reinterpret_cast<int16_t*>(addr), &val, 2, nullptr))
    {
        std::cout << "reading failed\r\n";
    }

    return val;
}

int32_t IntScanner::readInt32(uintptr_t addr)
{
    int32_t val = 0;

    if (!ReadProcessMemory(m_pHandle, reinterpret_cast<int32_t*>(addr), &val, 4, nullptr))
    {
        std::cout << "reading failed\r\n";
    }

    return val;
}

int64_t IntScanner::readInt64(uintptr_t addr)
{
    int64_t val = 0;

    if (!ReadProcessMemory(m_pHandle, reinterpret_cast<int64_t*>(addr), &val, 8, nullptr))
    {
        std::cout << "reading failed\r\n";
    }

    return val;
}