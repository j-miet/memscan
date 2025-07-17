// Memory scanner in C++ //

#include "memscan.hpp"

#include <handleapi.h>
#include <memoryapi.h>
#include <processthreadsapi.h>
#include <winerror.h>
#include <winnt.h>
#include <psapi.h>

#include <cstdint>
#include <iostream>


/// @brief Check if BASIC_MEMORY_INFORMATION protect condition allows for reading and writing to memory location
/// @param protectCond 
/// @return A boolean
bool mem_scan::checkPage(uint32_t protectCond)
{
    return protectCond & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY);
}

/// @brief Check if offset byte is in mask
/// @return A boolean
bool mem_scan::isInSearch(mem_scan::MemBlock* mb, 
                          int offset)
{
    return (mb->searchMask[(offset)/8] & (1<<(offset)%8));
}

/// @brief Update mask to exclude offset value
/// @param mb 
/// @param offset 
void mem_scan::removeFromSearch(mem_scan::MemBlock* mb, 
                                int offset)
{
    mb->searchMask[(offset)/8] &= ~(1<<(offset)%8); // ~ is bitwise NOT/complement
}

/// @brief Create a local memory block for a process
/// @param hProc Process handle
/// @param memInfo Internal information of base block
/// @return Pointer to memory block
mem_scan::MemBlock* mem_scan::createMemBlock(HANDLE pHandle, 
                                             MEMORY_BASIC_INFORMATION* memInfo, 
                                             int dataSize) 
{
    mem_scan::MemBlock* mb = reinterpret_cast<mem_scan::MemBlock*>(malloc (sizeof(mem_scan::MemBlock)));

    if (mb) 
    {
        mb->pHandle = pHandle;
        mb->addr = reinterpret_cast<uint8_t*>(memInfo->BaseAddress);
        mb->size = memInfo->RegionSize;
        mb->buffer = reinterpret_cast<uint8_t*>(malloc(memInfo->RegionSize));
        mb->searchMask = reinterpret_cast<uint8_t*>(malloc(memInfo->RegionSize/8)); // bitmask requires 1/8th of size
        memset(mb->searchMask, 0xff, memInfo->RegionSize/8); // 0xff is equal to 1111 1111 in binary
        mb->matches = memInfo->RegionSize; // initially search for every byte i.e. match to entire buffer
        mb->dataSize = dataSize;
        mb->next = nullptr;
    }

    return mb;
}

/// @brief Free allocated memory block
/// @param mb Block address
void mem_scan::freeMemBlock(mem_scan::MemBlock* mb) 
{
    if (mb) 
    {
        if (mb->buffer) 
        {
            free(mb->buffer);
        }
        if (mb->searchMask) 
        {
            free(mb->searchMask);
        }

        free(mb);
    }
}

/// @brief Read information into memory block buffer by applying a search condition
/// @param mb Block address 
/// @param condition Search condition
/// @param val Comparison value when condition is COND_EQUALS
void mem_scan::updateMemBlock(mem_scan::MemBlock* mb, 
                              mem_scan::Cond condition, 
                              uint32_t val) 
{
    uint8_t tempBuf[128*1024];
    uint32_t bytesLeft;
    uint32_t totalRead;
    uint32_t bytesToRead;
    SIZE_T bytesRead;

    if (mb->matches > 0) 
    {
        bytesLeft = mb->size;
        totalRead = 0;
        mb->matches = 0;

        while (bytesLeft) 
        {
            bytesToRead = (bytesLeft > sizeof(tempBuf)) ? sizeof(tempBuf) : bytesLeft;
            ReadProcessMemory(mb->pHandle, mb->addr + totalRead, tempBuf, bytesToRead, &bytesRead);
            if (bytesRead != bytesToRead)
            {
                break;
            }
            if (condition == COND_UNCONDITIONAL) 
            {
                // no comparison required: set start index based on totalRead, write 1s by amount of bytesRead
                memset(mb->searchMask + (totalRead/8), 0xff, bytesRead/8);
                mb->matches += bytesRead;
            } 
            else 
            {
                uint32_t offset; // offset of tempBuf i.e. local offset
                for (offset = 0; offset < bytesRead; offset += mb->dataSize) 
                {
                    if (isInSearch(mb, (totalRead+offset))) // totalRead+offset is the overall offset
                    {
                        bool isMatch = false; 
                        uint32_t tempVal;
                        uint32_t prevVal = 0; // previous tempVal

                        switch (mb->dataSize)
                        {
                            case 1: // a byte (unsigned char)
                                tempVal = tempBuf[offset];
                                prevVal = *(reinterpret_cast<uint8_t*>(&mb->buffer[totalRead+offset]));
                                break;
                            case 2: // 2 bytes (unsigned short)
                                tempVal = *(reinterpret_cast<uint16_t*>(&tempBuf[offset]));
                                prevVal = *(reinterpret_cast<uint16_t*>(&mb->buffer[totalRead+offset]));
                                break;
                            case 4: // 4 bytes (unsigned int)
                            default:
                                tempVal = *(reinterpret_cast<uint32_t*>(&tempBuf[offset]));
                                prevVal = *(reinterpret_cast<uint32_t*>(&mb->buffer[totalRead+offset]));
                                break;
                            case 8: // 8 bytes (unsigned long)
                                tempVal = *(reinterpret_cast<uint64_t*>(&tempBuf[offset]));
                                prevVal = *(reinterpret_cast<uint64_t*>(&mb->buffer[totalRead+offset]));
                        }
                        
                        switch (condition) 
                        {
                            case COND_EQUALS: // match to passed argument 'val'
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
                            removeFromSearch(mb, (totalRead+offset)); // otherwise set overall offset bit in mask to 0
                        }
                    }
                }
            }

            memcpy(mb->buffer + totalRead, tempBuf, bytesRead);
            bytesLeft -= bytesRead;
            totalRead += bytesRead;
        }

        mb->size = totalRead;
    }
}

/// @brief Scan and access the linked list of external memory blocks
/// @param processId Process id
/// @param dataSize Memory block reading size
/// @return Pointer to linked list 
mem_scan::MemBlock* mem_scan::createScan(int64_t processId, int dataSize) 
{
    mem_scan::MemBlock* mbLinked = nullptr;
    MEMORY_BASIC_INFORMATION memInfo;
    uint8_t* addr = 0;

    // get process handle: need full access to main process, but not to child processes
    HANDLE pHandle = OpenProcess(PROCESS_ALL_ACCESS, false, processId);

    if (pHandle) 
    {
        while (1) 
        {
            // VirtualQueryEx performs a query to return memory information
            // Returns 0 if failed or base address value gets too high, otherwise a non-zero value which is equal to
            // the number of bytes in the meminfo buffer
            SIZE_T byteCount = VirtualQueryEx(pHandle, addr, &memInfo, sizeof(memInfo));
            if (byteCount == 0 || byteCount == ERROR_INVALID_PARAMETER) 
            {
                break;
            }
            // discards unreadable/unwritable blocks i.e. if meminfo.State gets values MEM_FREE or MEM_RESERVE
            // similar logic is applied to meminfo.Protect to check if block allows for writing data except
            if ((memInfo.State & MEM_COMMIT) && (checkPage(memInfo.Protect))) 
            {
                mem_scan::MemBlock* mb = createMemBlock(pHandle, &memInfo, dataSize);
                if (mb) 
                {
                    mb->next = mbLinked;
                    mbLinked = mb;
                }
            }

            addr = (uint8_t*)memInfo.BaseAddress + memInfo.RegionSize;
        }
    }
    
    return mbLinked;
};

/// @brief Free allocated memory of scan list
/// @param mbLinked Linked list
void mem_scan::freeScan (mem_scan::MemBlock* mbLinked) 
{
    CloseHandle(mbLinked->pHandle);

    while (mbLinked) 
    {
        mem_scan::MemBlock* mb = mbLinked;
        mbLinked = mbLinked->next;
        freeMemBlock(mb);
    }
}

/// @brief Update all memory blocks
/// @param mbLinked Linked list
/// @param condition Search condition
/// @param int Value to compare to if condition has value COND_EQUALS
void mem_scan::updateScan(mem_scan::MemBlock* mbLinked, 
                          mem_scan::Cond condition, 
                          uint32_t val) 
{
    mem_scan::MemBlock* mb = mbLinked;

    while (mb) 
    {
        updateMemBlock(mb, condition, val);
        mb = mb->next;
    }
}

/// @brief Write into a memory location
/// @param pHandle Process handle
/// @param dataSize Data unit size
/// @param addr Base address
/// @param val New value
void mem_scan::poke(HANDLE pHandle, int dataSize, uintptr_t addr, uint32_t val)
{
    if (WriteProcessMemory(pHandle, reinterpret_cast<void*>(addr), &val, dataSize, nullptr) == 0)
    {
        std::cout << "poke failed\r\n";
    }
}

/// @brief Read a memory location
/// @param pHandle Process handle
/// @param dataSize Data unit size
/// @param addr Base address
/// @return Value stored in base address
int64_t mem_scan::peek(HANDLE pHandle, int dataSize, uintptr_t addr)
{
    int64_t val = 0;

    if (ReadProcessMemory(pHandle, reinterpret_cast<void*>(addr), &val, dataSize, nullptr) == 0)
    {
        std::cout << "peek failed\r\n";
    }

    return val;
}

/// @brief Print search matches
/// @param mbLinked Linked list
void mem_scan::printMatches(mem_scan::MemBlock* mbLinked) 
{
    int64_t offset;
    mem_scan::MemBlock* mb = mbLinked;

    while (mb) 
    {
        for (offset = 0; offset < mb->size; offset += mb->dataSize) 
        {
            if (isInSearch(mb, offset)) 
            {
                int64_t val = peek(mb->pHandle, mb->dataSize, reinterpret_cast<uint64_t>(mb->addr) + offset);

                std::cout << "0x" << std::hex << reinterpret_cast<uint64_t>(mb->addr) + offset << std::dec << ": ";
                std::cout << "0x" << std::hex << val << std::dec << " ";
                std::cout << val << " " << mb->size << "\r\n"; 
            }
        }

        mb = mb->next;
    }
}

/// @brief Return match count
/// @param mbLinked Linked list
/// @return Match count
int mem_scan::getMatchesCount(mem_scan::MemBlock* mbLinked) 
{
    mem_scan::MemBlock* mb = mbLinked;
    int count = 0;

    while (mb) 
    {
        count += mb->matches;
        mb = mb->next;
    }

    return count;
}

/// @brief Convert decimal/hex string values into integers
/// @param s String i.e. char pointer
/// @return Decimal integer
int64_t mem_scan::stringToInt(char* s)
{
    int base = 10;

    if (s[0] == '0' && s[1] == 'x')
    {
        base = 16;
        s += 2;
    }

    return strtoul(s, nullptr, base);
}

/// @brief Access and modify the value of an memory address
/// @param pHandle Process handle
/// @param dataSize Data block size
void mem_scan::uiPoke(HANDLE pHandle, int dataSize)
{
    uint32_t addr;
    uint32_t val;
    char input[20];

    std::cout << "Enter the address: ";
    std::cin >> input;
    addr = stringToInt(input);

    std::cout << "\n\nEnter the value: ";
    val = stringToInt(input);
    std::cout << "\r\n";

    mem_scan::poke(pHandle, dataSize, addr, val);
}

/// @brief Initialize a new scanner
/// @return Linked list
mem_scan::MemBlock* mem_scan::uiNewScan()
{
    mem_scan::MemBlock* scan = nullptr;
    DWORD pid;
    int dataSize;
    int64_t startValue;
    Cond startCondition;
    char input[20];

    while(1)
    {
        std::cout << "\r\nEnter the process id: ";
        std::cin >> input; 
        pid = stringToInt(input);

        std::cout << "\r\nEnter the data size: ";
        std::cin >> input;
        dataSize = stringToInt(input);

        std::cout << "\r\nEnter the start value, or 'u' for unknown: ";
        std::cin >> input;
        if (input[0] == 'u')
        {
            startCondition = COND_UNCONDITIONAL;
            startValue = 0;
        }
        else
        {
            startCondition = COND_EQUALS;
            startValue = stringToInt(input);
        }

        scan = createScan(pid, dataSize);
        if (scan)
        {
            break;
        }
        std::cout << "\r\nInvalid scan";
    }

    updateScan(scan, startCondition, startValue);
    std::cout << "\r\n" << getMatchesCount(scan) << " matches found\n\n";

    return scan;
}

/// @brief Start a memory scanner
void mem_scan::uiRunScan()
{
    uint32_t val;
    char input[20];
    mem_scan::MemBlock* scan;

    scan = uiNewScan();

    while (1)
    {
        std::cout << "\r\nEnter the next value or, "
            "\r\n[i] increased"
            "\r\n[d] decreased"
            "\r\n[m] print matches"
            "\r\n[p] poke address"
            "\r\n[n] new scan"
            "\r\n[q] quit\r\n";

        std::cin >> input;
        std::cout << "\r\n";

        switch (input[0])
        {
            case 'i':
                updateScan(scan, COND_INCREASED, 0);
                std::cout << getMatchesCount(scan) << " matches found\r\n";
                break;
            case 'd':
                updateScan(scan, COND_DECREASED, 0);
                std::cout << getMatchesCount(scan) << " matches found\r\n"; 
                break;
            case 'm':
                printMatches(scan);
                break;
            case 'p':
                uiPoke(scan->pHandle, scan->dataSize);
                break;
            case 'n':
                freeScan(scan);
                scan = uiNewScan();
                break;
            case 'q':
                freeScan(scan);
                return;
            default:
                val = stringToInt(input);
                updateScan(scan, COND_EQUALS, val);
                std::cout << getMatchesCount(scan) << " matches left";
                break;
        }
    }
}