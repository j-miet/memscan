#ifndef MEMSCAN_HPP
#define MEMSCAN_HPP

#include <handleapi.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace mem_scan
{
    struct MemBlock
    {
        HANDLE pHandle;
        std::unique_ptr<int8_t> addr;
        bool isStr;
        int size;
        std::vector<int8_t> buffer;
        std::vector<uint8_t> searchMask;
        int matches;
        int dataSize;
        MemBlock* next;
    };
    enum Condition
    {
        COND_UNCONDITIONAL, 
        COND_EQUALS,
        COND_INCREASED,
        COND_DECREASED
    };

    const std::array<int, 4> writable {PAGE_READWRITE, PAGE_WRITECOPY, PAGE_EXECUTE_READWRITE, PAGE_EXECUTE_WRITECOPY};

    bool checkPage(int32_t protectCond);
    bool isInSearch(MemBlock* mb, int offset);
    void removeFromSearch(MemBlock* mb, int offset);
    int stringToInt(std::string s);
    MemBlock* createMemBlock(HANDLE pHandle, MEMORY_BASIC_INFORMATION* memInfo, int dataSize);
    MemBlock* createScan(int64_t processId, int dataSize, bool isStr);
    void freeScan (MemBlock* mbLinked);
    int getMatchesCount(MemBlock* mbLinked);
    void uiPrintMatches(MemBlock* mbLinked);
    MemBlock* uiNewScan();
    void uiPoke(HANDLE pHandle, int dataSize, bool isStr);
    void uiRunScan();   
} // namespace mem_scan
#endif /* MEMSCAN.HPP */