#include "memblock.hpp"
#include "stringscanner.hpp"

#include <memoryapi.h>

#include <cstring>
#include <iostream>

StringScanner::StringScanner(std::vector<MemBlock> memblocks): m_pHandle(memblocks[0].pHandle()), m_memblocks(memblocks)
{}

StringScanner::~StringScanner()
{
    CloseHandle(m_pHandle);
}

void StringScanner::updateSearch(MemBlock& mb, 
                                 int bytesRead, 
                                 std::string& tempBuf, 
                                 Condition condition, 
                                 std::string val)
{
    for (int offset = 0; offset < bytesRead; offset++) 
    {
        if (mb.isInSearch(offset))
        {
            std::string strBuffer;
            std::string prevStr;
            bool isMatch = false;

            strBuffer.resize(val.size());

            switch (condition) 
            {
                case COND_EQUALS:
                    if (tempBuf[offset] == val[0])
                    {
                        for (int i = 0; i < val.size(); i++)
                        {
                            strBuffer[i] = tempBuf[offset+i];
                        }
                        isMatch = (strBuffer.compare(val) == 0) ? true : false;
                    }
                    break;
                case COND_INCREASED:
                    if (tempBuf[offset] == mb.buffer()[offset])
                    {
                        prevStr.resize(val.size());
                        for (int i = 0; i < strBuffer.size(); i++)
                        {
                            prevStr[i] = mb.buffer()[offset+i];
                            strBuffer[i] = tempBuf[offset+i];
                        }
                        isMatch = (prevStr.compare(strBuffer) > 0) ? true : false;
                    }
                    break;
                case COND_DECREASED:
                    if (tempBuf[offset] == mb.buffer()[offset])
                    {
                        prevStr.resize(val.size());
                        for (int i = 0; i < strBuffer.size(); i++)
                        {
                            prevStr[i] = mb.buffer()[offset+i];
                            strBuffer[i] = tempBuf[offset+i];
                        }
                        isMatch = (prevStr.compare(strBuffer) < 0) ? true : false;
                    }
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

void StringScanner::updateMemBlock(MemBlock& mb, Condition condition, std::string val)
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

void StringScanner::updateScan(Condition condition, std::string val) 
{
    for (auto& mb : m_memblocks)
    {
        updateMemBlock(mb, condition, val);
    }
}

void StringScanner::writeString(uintptr_t addr, std::string val)
{
    int size = val.size();
    if (!WriteProcessMemory(m_pHandle, reinterpret_cast<uintptr_t*>(addr), &val[0], size, nullptr))
    {
        std::cout << "writing failed\r\n";
    }
}

std::string StringScanner::readString(uintptr_t addr, int size)
{
    std::string strBuffer;
    strBuffer.resize(size);

    if (!ReadProcessMemory(m_pHandle, reinterpret_cast<uintptr_t*>(addr), &strBuffer[0], size, nullptr))
    {
        std::cout << "reading failed\r\n";
    }

    return strBuffer;
}