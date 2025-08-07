#include "intscanner.hpp"
#include "memblock.hpp"
#include "scanner.hpp"
#include "stringscanner.hpp"

#include <memoryapi.h>
#include <processthreadsapi.h>
#include <winerror.h>

#include <iostream>

std::vector<MemBlock> Scanner::createScan(int processId, int dataSize) 
{
    std::vector<MemBlock> mbScan;
    MEMORY_BASIC_INFORMATION memInfo;
    char* addr = 0;

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
            if ((memInfo.State & MEM_COMMIT) && (MemBlock::checkPage(memInfo.Protect))) 
            {
                MemBlock mb(pHandle, &memInfo, dataSize);
                mbScan.insert(mbScan.end(), mb);
            }

            addr = static_cast<char*>(memInfo.BaseAddress) + memInfo.RegionSize;
        }
    }

    return mbScan;
};

int Scanner::getMatchesCount(std::vector<MemBlock> mbScan) 
{
    int count = 0;

    for (auto& mb : mbScan) 
    {
        count += mb.matches();
    }

    return count;
}

int Scanner::stringToInt(std::string s)
{
    int base = 10;
    bool negative = false;

    if (s[0] == '0' && s[1] == 'x')
    {
        base = 16;
        s.erase(0, 2);
    }
    else if (s.size() == 0)
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
        if ((isdigit(ch) == false) ? true : false)
        { 
            return -1; // processId and dataSize are always non-negative so -1 can be used for discarding bad inputs
        }
    }
    if (negative)
    {
        return -stoi(s, 0, base);
    }

    return stoi(s, 0, base);
}

StringScanner Scanner::createStringScanner(Condition startCondition)
{
    StringScanner strScanner(m_scan);
    strScanner.updateScan(startCondition, m_strVal);
    m_scan = strScanner.memblocks();

    std::cout << "\r\n" << getMatchesCount(m_scan) << " matches found\n";
    return strScanner;
}

IntScanner Scanner::createIntScanner(Condition startCondition)
{
    IntScanner intScan(m_scan);
    //intScan.updateScan(startCondition, m_intVal);
    //m_scan = intScan.memblocks();

    std::cout << "\r\n" << getMatchesCount(m_scan) << " matches found\n";
    return intScan;
}

// UI
void Scanner::uiNewScan()
{
    DWORD pId;
    int dataSize;
    std::string input;

    while(1)
    {
        std::cout << "\r\nEnter the process id: ";
        std::getline(std::cin, input);
        pId = stringToInt(input);

        std::cout << "\r\nEnter the data size (1, 2, 4 or 8 bytes. Empty input means 8): ";
        std::getline(std::cin, input);
        if (input[0] == 's')
        {
            m_isString = true;
        }
        else
        {
            m_isString = false;
            dataSize = stringToInt(input);
            if (dataSize == -1)
            {   
                dataSize = 8;
            }
        }

        std::cout << "\r\nEnter the start value, or empty input for unconditional search: ";
        std::getline(std::cin, input);
        if (input.size() == 0)
        {
            m_startCondition = COND_UNCONDITIONAL;
            m_intVal = 0;
            m_strVal = "";
        }
        else
        {
            m_startCondition = COND_EQUALS;
            if (m_isString)
            {
                m_strVal = input.data();
            }
            else
            {
                m_intVal = stringToInt(input);
            }
        }
        
        m_scan = createScan(pId, dataSize);
        if (!m_scan.empty())
        {
            break;
        }

        std::cout << "\r\nInvalid scan";
    }
}

// String UI

void Scanner::uiPrintStringMatches(StringScanner strScan, int size) 
{
    for (auto mb : strScan.memblocks())
    {
        for (int offset = 0; offset < mb.size(); offset++) 
        {
            if (mb.isInSearch(offset)) 
            {
                std::string sVal = strScan.readString(reinterpret_cast<uintptr_t>(mb.addr()) + offset, size);
                std::cout << "0x";
                std::cout << std::hex << reinterpret_cast<intptr_t>(mb.addr()) + offset << std::dec;
                std::cout << "-> value: " << std::flush << sVal << std::flush;
                std::cout << " | size: " << mb.size() << "\r" << std::endl;
            }
        }
    }
}

void Scanner::uiWriteString(StringScanner scanner)
{
    uintptr_t addr;
    std::string input;

    std::cout << "Enter the address: ";
    std::cin >> input;
    addr = stringToInt(input);

    std::cout << "\nEnter the value: ";
    std::cin >> input;
    std::cout << "\r\n";

    scanner.writeString(addr, input);
}

int Scanner::openStringUi(StringScanner strScanner)
{
    std::string input;
    std::string sVal = m_strVal;

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
                strScanner.updateScan(COND_INCREASED, sVal);
                std::cout << getMatchesCount(strScanner.memblocks()) << " matches found\r\n";
                break;
            case 'd':
                strScanner.updateScan(COND_DECREASED, sVal);
                std::cout << getMatchesCount(strScanner.memblocks()) << " matches found\r\n"; 
                break;
            case 'm':
                uiPrintStringMatches(strScanner, sVal.size());
                break;
            case 'p':
                uiWriteString(strScanner);
                break;
            case 'n':
                return 1;
            case 'q':
                return 0;
            default:
                sVal = input.data();
                strScanner.updateScan(COND_EQUALS, sVal);

                std::cout << getMatchesCount(strScanner.memblocks()) << " matches left";
                break;
        }
    }
}

// Int UI

/*
int Scanner::openIntUi(IntScanner intScanner)
{
    std::string input;
    int64_t iVal = m_intVal;

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
                intScanner.updateScan(COND_INCREASED, iVal);
                std::cout << getMatchesCount(intScanner.memblocks()) << " matches found\r\n";
                break;
            case 'd':
                intScanner.updateScan(COND_DECREASED, iVal);
                std::cout << getMatchesCount(intScanner.memblocks()) << " matches found\r\n"; 
                break;
            case 'm':
                uiPrintStringMatches(intScanner, iVal.size());
                break;
            case 'p':
                uiWriteString(intScanner);
                break;
            case 'n':
                return 1;
            case 'q':
                return 0;
            default:
                iVal = stringToInt(input);
                intScanner.updateScan(COND_EQUALS, iVal);

                std::cout << getMatchesCount(intScanner.memblocks()) << " matches left";
                break;
        }
    }
}
*/
