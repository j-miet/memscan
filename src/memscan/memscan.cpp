// Memory scanner in C++ //

#include "memscan.hpp"

#include <handleapi.h>
#include <memoryapi.h>
#include <processthreadsapi.h>
#include <winerror.h>
#include <winnt.h>
#include <psapi.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>


/// @brief Check if BASIC_MEMORY_INFORMATION protect condition allows for reading and writing to memory location
/// @param protectCond Value of BASIC_MEMORY_INFORMATION.protect (DWORD i.e. int32)
/// @return Boolean
bool mem_scan::checkPage(int32_t protectCond)
{
    auto condIter = std::find(mem_scan::writable.begin(), mem_scan::writable.end(), protectCond);
    return condIter != mem_scan::writable.end() ? true : false;
}

/// @brief Check if offset byte is in mask
/// @return Boolean
bool mem_scan::isInSearch(mem_scan::MemBlock* mb, 
                          int offset)
{
    return (mb->searchMask[(offset)/8] & (1<<(offset)%8));
}

/// @brief Update mask to exclude offset value
/// @param mb Memory block
/// @param offset Offset byte
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
    mem_scan::MemBlock* mb = new mem_scan::MemBlock;

    if (mb) 
    {
        mb->pHandle = pHandle;
        mb->addr = static_cast<int8_t*>(memInfo->BaseAddress);
        mb->size = memInfo->RegionSize;
        mb->buffer.reserve(memInfo->RegionSize);
        mb->searchMask.reserve(memInfo->RegionSize/8); // bitmask requires 1/8th of byte size
        // fill mask with 1's by inserting 0xff (= 1111 1111 in binary) into each byte
        std::fill(mb->searchMask.begin(), mb->searchMask.begin()+memInfo->RegionSize/8, 0xff);
        mb->matches = memInfo->RegionSize; // initially search for every address in region
        mb->dataSize = dataSize;
        mb->next = nullptr;
    }

    return mb;
}

/// @brief Read information into memory block buffer by applying a search condition
/// @param mb Block address 
/// @param condition Search condition
/// @param val Comparison value when condition is COND_EQUALS
void mem_scan::updateMemBlock(mem_scan::MemBlock* mb, 
                              mem_scan::Cond condition, 
                              int64_t val) 
{
    // using std::vector<int8_t> tempBuf(1024*128) slows the update process enormously. 
    // No idea why it happens and how to fix it. Instead, std::array is used and it operates as expected

    std::array<int8_t, 1024*128> tempBuf;
    int64_t bytesLeft;
    int64_t totalRead;
    int64_t bytesToRead;
    size_t bytesRead;

    if (mb->matches > 0) 
    {
        bytesLeft = mb->size;
        totalRead = 0;
        mb->matches = 0;

        while (bytesLeft) 
        {
            bytesToRead = (bytesLeft > sizeof(tempBuf)) ? sizeof(tempBuf) : bytesLeft;
            ReadProcessMemory(mb->pHandle, mb->addr + totalRead, tempBuf.data(), bytesToRead, &bytesRead);
            if (bytesRead != bytesToRead)
            {
                break;
            }
            if (condition == COND_UNCONDITIONAL) 
            {
                // no comparison required: set start index based on totalRead/8, write 1s by amount of bytesRead/8
                std::fill(mb->searchMask.begin()+totalRead/8, mb->searchMask.begin()+totalRead/8+bytesRead/8, 0xff);
                mb->matches += bytesRead;
            } 
            else 
            {
                int64_t offset; // offset of tempBuf i.e. local offset
                for (offset = 0; offset < bytesRead; offset += mb->dataSize) 
                {
                    if (mem_scan::isInSearch(mb, (totalRead+offset))) // totalRead+offset is the overall offset
                    {
                        bool isMatch = false; 
                        int64_t tempVal;
                        int64_t prevVal = 0; // previous tempVal

                        switch (mb->dataSize)
                        {
                            case 1: // a byte (char)
                                tempVal = tempBuf[offset];
                                prevVal = mb->buffer[totalRead+offset];
                                break;
                            case 2: // 2 bytes (short)
                                tempVal = *reinterpret_cast<int16_t*>(&tempBuf[offset]);
                                prevVal = *reinterpret_cast<int16_t*>(&mb->buffer[totalRead+offset]);
                                break;
                            case 4: // 4 bytes (int)
                            default:
                                tempVal = *reinterpret_cast<int32_t*>(&tempBuf[offset]);
                                prevVal = *reinterpret_cast<int32_t*>(&mb->buffer[totalRead+offset]);
                                break;
                            case 8: // 8 bytes (long long)
                                tempVal = *reinterpret_cast<int64_t*>(&tempBuf[offset]);
                                prevVal = *reinterpret_cast<int64_t*>(&mb->buffer[totalRead+offset]);
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
                        {   // otherwise set overall offset bit in mask to 0
                            mem_scan::removeFromSearch(mb, (totalRead+offset));
                        }
                    }
                }
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

/// @brief Scan and access the linked list of external memory blocks
/// @param processId Process id
/// @param dataSize Memory block reading size
/// @return Pointer to linked list 
mem_scan::MemBlock* mem_scan::createScan(int64_t processId, int dataSize) 
{
    mem_scan::MemBlock* mbLinked = nullptr;
    MEMORY_BASIC_INFORMATION memInfo;
    int8_t* addr = 0;

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
            // discards unreadable/unwritable blocks
            if ((memInfo.State & MEM_COMMIT) && (mem_scan::checkPage(memInfo.Protect))) 
            {
                mem_scan::MemBlock* mb = mem_scan::createMemBlock(pHandle, &memInfo, dataSize);
                if (mb) 
                {
                    mb->next = mbLinked;
                    mbLinked = mb;
                }
            }

            addr = (int8_t*)memInfo.BaseAddress + memInfo.RegionSize;
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
        delete(mb);
    }
}

/// @brief Update all memory blocks
/// @param mbLinked Linked list
/// @param condition Search condition
/// @param int Value to compare to if condition has value COND_EQUALS
void mem_scan::updateScan(mem_scan::MemBlock* mbLinked, 
                          mem_scan::Cond condition, 
                          int64_t val) 
{
    mem_scan::MemBlock* mb = mbLinked;

    while (mb) 
    {
        mem_scan::updateMemBlock(mb, condition, val);
        mb = mb->next;
    }
}

/// @brief Write into a memory location
/// @param pHandle Process handle
/// @param dataSize Data unit size
/// @param addr Base address
/// @param val New value
void mem_scan::poke(HANDLE pHandle, int dataSize, int64_t addr, int64_t val)
{
    if (WriteProcessMemory(pHandle, reinterpret_cast<int64_t*>(addr), &val, dataSize, nullptr) == 0)
    {
        std::cout << "poke failed\r\n";
    }
}

/// @brief Read a memory location
/// @param pHandle Process handle
/// @param dataSize Data unit size
/// @param addr Base address
/// @return Value stored in base address
int64_t mem_scan::peek(HANDLE pHandle, int dataSize, int64_t addr)
{
    int64_t val = 0;

    if (ReadProcessMemory(pHandle, reinterpret_cast<int64_t*>(addr), &val, dataSize, nullptr) == 0)
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
            if (mem_scan::isInSearch(mb, offset)) 
            {
                int64_t val = peek(mb->pHandle, mb->dataSize, reinterpret_cast<intptr_t>(mb->addr) + offset);
                std::cout << "0x" << std::hex << reinterpret_cast<intptr_t>(mb->addr) + offset << std::dec;
                std::cout << "-> value: 0x" << std::hex << val << std::dec << " (";
                std::cout << val << ") | size: " << mb->size << "\r\n";
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
/// @param s String object
/// @return Decimal integer
int mem_scan::stringToInt(std::string s)
{
    int base = 10;
    bool negative = false;

    if (s[0] == '0' && s[1] == 'x')
    {
        base = 16;
        s.erase(0, 2);
    }
    else if (s.length() == 0)
    {
        return -1;
    }
    if (s[0] == '-')
    {
        negative = true;
        s.erase(0, 1);
    }
    
    for (auto ch : s)
    {   

        if ((std::isdigit(ch) == false) ? true : false)
        { 
            return -1; // processId and dataSize are always non-negative so -1 can be used to discard bad inputs
        }
    }
    if (negative == true)
    {
        return -stoi(s, 0, base);
    }

    return stoi(s, 0, base);
}

/// @brief Access and modify the value of an memory address
/// @param pHandle Process handle
/// @param dataSize Data block size
void mem_scan::uiPoke(HANDLE pHandle, int dataSize)
{
    int64_t addr;
    int64_t val;
    std::string input;

    std::cout << "Enter the address: ";
    std::cin >> input;
    addr = mem_scan::stringToInt(input);

    std::cout << "\nEnter the value: ";
    std::cin >> input;
    val = mem_scan::stringToInt(input);
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
    std::string input;

    while(1)
    {
        std::cout << "\r\nEnter the process id: ";
        std::getline(std::cin, input);
        pid = mem_scan::stringToInt(input);

        std::cout << "\r\nEnter the data size (1, 2, 4 or 8 bytes. Default is 4. " 
            "Use 4 (32bit) or 8 (64bit) to display negative values): ";
        std::getline(std::cin, input);
        dataSize = mem_scan::stringToInt(input);
        if (dataSize == -1)
        {
            dataSize = 4;
        }

        std::cout << "\r\nEnter the start value, or empty input for unconditional search: ";
        std::getline(std::cin, input);
        if (input.length() == 0)
        {
            startCondition = COND_UNCONDITIONAL;
            startValue = 0;
        }
        else
        {
            startCondition = COND_EQUALS;
            startValue = mem_scan::stringToInt(input);
        }

        scan = mem_scan::createScan(pid, dataSize);
        if (scan)
        {
            break;
        }
        std::cout << "\r\nInvalid scan";
    }

    mem_scan::updateScan(scan, startCondition, startValue);
    std::cout << "\r\n" << mem_scan::getMatchesCount(scan) << " matches found\n\n";

    return scan;
}

/// @brief Start a memory scanner
void mem_scan::uiRunScan()
{
    int64_t val;
    std::string input;
    mem_scan::MemBlock* scan;

    scan = mem_scan::uiNewScan();

    while (1)
    {
        std::cout << "\r\nEnter the next value or, "
            "\r\n[i] increased"
            "\r\n[d] decreased"
            "\r\n[m] print matches"
            "\r\n[p] poke address"
            "\r\n[n] new scan"
            "\r\n[q] quit"
            "\r\n=>";

        std::cin >> input;
        std::cin.ignore(1024, '\n'); // flush newline from buffer for std::getline
        std::cout << "\r\n";

        switch (input[0])
        {
            case 'i':
                mem_scan::updateScan(scan, COND_INCREASED, 0);
                std::cout << mem_scan::getMatchesCount(scan) << " matches found\r\n";
                break;
            case 'd':
                mem_scan::updateScan(scan, COND_DECREASED, 0);
                std::cout << mem_scan::getMatchesCount(scan) << " matches found\r\n"; 
                break;
            case 'm':
                mem_scan::printMatches(scan);
                break;
            case 'p':
                mem_scan::uiPoke(scan->pHandle, scan->dataSize);
                break;
            case 'n':
                mem_scan::freeScan(scan);
                scan = mem_scan::uiNewScan();
                break;
            case 'q':
                mem_scan::freeScan(scan);
                return;
            default:
                val = mem_scan::stringToInt(input);
                mem_scan::updateScan(scan, COND_EQUALS, val);
                std::cout << mem_scan::getMatchesCount(scan) << " matches left";
                break;
        }
    }
}