// Memory scanner in C++ //

#include <handleapi.h>
#include <memoryapi.h>
#include <processthreadsapi.h>
#include <winerror.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <vector>

#include "memscan.hpp"
#include "memscan_int.hpp"
#include "memscan_str.hpp"


/**
 * \brief Check if BASIC_MEMORY_INFORMATION protect condition allows for reading and writing to memory location
 * \param protectCond Value of BASIC_MEMORY_INFORMATION.protect (DWORD i.e. int32)
 * \return Boolean
 */
bool mem_scan::checkPage(int32_t protectCond)
{
    auto condIter = std::find(mem_scan::writable.begin(), mem_scan::writable.end(), protectCond);
    return condIter != mem_scan::writable.end() ? true : false;
}

/**
 * \brief Check if offset byte is in mask
 * 
 * \return Boolean
 */
bool mem_scan::isInSearch(mem_scan::MemBlock* mb, 
                          int offset)
{
    return (mb->searchMask[(offset)/8] & (1<<(offset)%8));
}

/**
 * \brief Update mask to exclude offset value
 * 
 * \param mb Memory block
 * \param offset Offset byte
 */
void mem_scan::removeFromSearch(mem_scan::MemBlock* mb, 
                                int offset)
{
    mb->searchMask[(offset)/8] &= ~(1<<(offset)%8);
}

/**
 * \brief Convert decimal/hex string values into integers
 * 
 * \param s String object
 * 
 * \return Decimal integer
 */
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
            return -1; // processId and dataSize are always non-negative so -1 can be used for discarding bad inputs
        }
    }
    if (negative == true)
    {
        return -stoi(s, 0, base);
    }

    return stoi(s, 0, base);
}

/**
 * \brief Create a local memory block for a process
 * 
 * \param hProc Process handle
 * \param memInfo Internal information of base block
 * 
 * \return Pointer to memory block
 */
mem_scan::MemBlock* mem_scan::createMemBlock(HANDLE pHandle, 
                                             MEMORY_BASIC_INFORMATION* memInfo, 
                                             int dataSize) 
{
    mem_scan::MemBlock* mb = new mem_scan::MemBlock;

    if (mb) 
    {
        mb->pHandle = pHandle;
        mb->addr = std::unique_ptr<int8_t>(static_cast<int8_t*>(memInfo->BaseAddress));
        mb->isStr = false;
        mb->size = memInfo->RegionSize;
        mb->buffer.reserve(memInfo->RegionSize);
        mb->searchMask.reserve(memInfo->RegionSize/8);
        std::fill(mb->searchMask.begin(), mb->searchMask.begin()+memInfo->RegionSize/8, 0xff);
        mb->matches = memInfo->RegionSize;
        mb->dataSize = dataSize;
        mb->next = nullptr;
    }

    return mb;
}

/**
 * \brief Scan and access the linked list of external memory blocks
 * 
 * \param Process id
 * \param dataSize Memory block reading size
 * 
 * \return Pointer to linked list 
 */
mem_scan::MemBlock* mem_scan::createScan(int64_t processId, int dataSize, bool isStr) 
{
    mem_scan::MemBlock* mbLinked = nullptr;
    MEMORY_BASIC_INFORMATION memInfo;
    int8_t* addr = 0;

    HANDLE pHandle = OpenProcess(PROCESS_ALL_ACCESS, false, processId);

    if (pHandle) 
    {
        while (1) 
        {
            SIZE_T byteCount = VirtualQueryEx(pHandle, addr, &memInfo, sizeof(memInfo));
            if (byteCount == 0 || byteCount == ERROR_INVALID_PARAMETER) 
            {
                break;
            }
            if ((memInfo.State & MEM_COMMIT) && (mem_scan::checkPage(memInfo.Protect))) 
            {
            
                mem_scan::MemBlock* mb = mem_scan::createMemBlock(pHandle, &memInfo, dataSize);
                if (mb) 
                {
                    if (isStr)
                    {
                        mb->isStr = true;
                    }
                    mb->next = mbLinked;
                    mbLinked = mb;
                }
            }

            addr = static_cast<int8_t*>(memInfo.BaseAddress) + memInfo.RegionSize;
        }
    }

    return mbLinked;
};

/**
 * \brief Free allocated memory in scan list
 * 
 * \param mbLinked Linked list
 */
void mem_scan::freeScan(mem_scan::MemBlock* mbLinked) 
{
    CloseHandle(mbLinked->pHandle);

    while (mbLinked)
    {
        mem_scan::MemBlock* mb = mbLinked;
        mbLinked = mbLinked->next;
        mb->addr.release();
        delete(mb);
    }
}

/**
 * \brief Return match count
 * 
 * \param mbLinked Linked list
 * 
 * \return Match count
 */
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

/**
 * \brief Print search matches
 * 
 * \param mbLinked Linked list
 */
void mem_scan::uiPrintMatches(mem_scan::MemBlock* mbLinked) 
{
    int64_t offset;
    mem_scan::MemBlock* mb = mbLinked;

    while (mb)
    {
        for (offset = 0; offset < mb->size; offset += mb->dataSize) 
        {
            if (mem_scan::isInSearch(mb, offset)) 
            {
                if (mb->isStr)
                {
                    char* sVal = mem_scan::peekString(mb->pHandle, 
                                                      reinterpret_cast<intptr_t>(mb->addr.get()) + offset);
                    std::cout << "0x";
                    std::cout << std::hex <<reinterpret_cast<intptr_t>(mb->addr.get()) + offset<< std::dec;
                    std::cout << "-> value: " << std::flush << sVal << std::flush;
                    std::cout << " | size: " << mb->size << "\r" << std::endl; 
                }
                else
                {
                    int64_t val = mem_scan::peekInt(mb->pHandle, 
                                                    mb->dataSize, 
                                                    reinterpret_cast<intptr_t>(mb->addr.get()) + offset);  
                    std::cout << "0x" << std::hex << reinterpret_cast<intptr_t>(mb->addr.get()) + offset << std::dec;
                    std::cout << "-> value: 0x" << std::hex << val << std::dec << " (";
                    std::cout << val << ") | size: " << mb->size << "\r" << std::endl;
                }
            }
        }

        mb = mb->next;
    }
}

/**
 * \brief Access and modify the value of an memory address through UI
 * 
 * \param pHandle Process handle
 * \param dataSize Data block size
 */
void mem_scan::uiPoke(HANDLE pHandle, int dataSize, bool isStr)
{
    int64_t addr;
    int64_t val;
    char* sVal;
    std::string input;

    std::cout << "Enter the address: ";
    std::cin >> input;
    addr = mem_scan::stringToInt(input);

    std::cout << "\nEnter the value: ";
    std::cin >> input;
    if (isStr)
    {
        sVal = input.data(); 
    }
    val = mem_scan::stringToInt(input);
    std::cout << "\r\n";

    if (isStr)
    {
        mem_scan::pokeString(pHandle, addr, sVal);
    }
    else
    {
        switch (dataSize)
        {
            case 1:
                mem_scan::pokeInt8(pHandle, dataSize, addr, val);
            case 2:
                mem_scan::pokeInt16(pHandle, dataSize, addr, val);
            case 4:
                mem_scan::pokeInt32(pHandle, dataSize, addr, val);
            case 8:
            default:
                mem_scan::pokeInt64(pHandle, dataSize, addr, val);
        }
    }
}

/**
 * \brief Initialize a new scanner through UI
 * 
 * \return Linked list
 */
mem_scan::MemBlock* mem_scan::uiNewScan()
{
    mem_scan::MemBlock* scan = nullptr;
    DWORD pid;
    int dataSize;
    int64_t startIntValue;
    char* startStringValue;
    Condition startCondition;
    std::string input;

    while(1)
    {
        std::cout << "\r\nEnter the process id: ";
        std::getline(std::cin, input);
        pid = mem_scan::stringToInt(input);

        std::cout << "\r\nEnter the data size (1, 2, 4 or 8 bytes. Empty input means 8): ";
        std::getline(std::cin, input);
        bool isString;
        if (input[0] == 's')
        {
            isString = true;
        }
        else
        {
            dataSize = mem_scan::stringToInt(input);
            if (dataSize == -1)
            {   
                dataSize = 8;
            }
        }

        std::cout << "\r\nEnter the start value, or empty input for unconditional search: ";
        std::getline(std::cin, input);
        if (input.length() == 0)
        {
            startCondition = COND_UNCONDITIONAL;
            startIntValue = 0;
            startStringValue = 0;
        }
        else
        {
            startCondition = COND_EQUALS;
            if (isString)
            {
                dataSize = input.length();
                startStringValue = input.data();
            }
            else
            {
                startIntValue = mem_scan::stringToInt(input);
            }
        }
        scan = mem_scan::createScan(pid, dataSize, isString);
        if (scan)
        {
            break;
        }

        std::cout << "\r\nInvalid scan";
    }

    if (scan->isStr)
    {
        mem_scan::updateScanForString(scan, startCondition, startStringValue);
    }
    else
    {
        mem_scan::updateScanForInt(scan, startCondition, startIntValue);
    }
    std::cout << "\r\n" << mem_scan::getMatchesCount(scan) << " matches found\n\n";

    return scan;
}

/**
 * @brief Open the memory scanner UI
 */
void mem_scan::uiRunScan()
{
    int64_t val;
    char* sVal;
    std::string input;
    mem_scan::MemBlock* scan = nullptr;

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
                mem_scan::updateScanForInt(scan, COND_INCREASED, 0);
                std::cout << mem_scan::getMatchesCount(scan) << " matches found\r\n";
                break;
            case 'd':
                mem_scan::updateScanForInt(scan, COND_DECREASED, 0);
                std::cout << mem_scan::getMatchesCount(scan) << " matches found\r\n"; 
                break;
            case 'm':
                mem_scan::uiPrintMatches(scan);
                break;
            case 'p':
                mem_scan::uiPoke(scan->pHandle, scan->dataSize, scan->isStr);
                break;
            case 'n':
                mem_scan::freeScan(scan);
                scan = mem_scan::uiNewScan();
                break;
            case 'q':
                mem_scan::freeScan(scan);
                return;
            default:
                if (scan->isStr)
                {
                    sVal = input.data();
                    mem_scan::updateScanForString(scan, COND_EQUALS, sVal);
                }
                else
                {
                    val = mem_scan::stringToInt(input);
                    mem_scan::updateScanForInt(scan, COND_EQUALS, val);
                }
                std::cout << mem_scan::getMatchesCount(scan) << " matches left";
                break;
        }
    }
}