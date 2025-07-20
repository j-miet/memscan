#ifndef MEMSCAN_HPP
#define MEMSCAN_HPP

#include <handleapi.h>

#include <array>
#include <memory>
#include <vector>

namespace mem_scan
{
    // A single block of memory. Memory blocks are sequential, forming a linked list.
    struct MemBlock
    {
        HANDLE pHandle;
        //int8_t* addr;
        std::unique_ptr<int8_t> addr;
        int size;
        std::vector<int8_t> buffer;
        std::vector<uint8_t> searchMask;
        int matches;
        int dataSize;
        MemBlock* next;
    };
    // Memory search conditions
    enum Cond
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
    MemBlock* createMemBlock(HANDLE pHandle, MEMORY_BASIC_INFORMATION* memInfo, int dataSize);
    void updateMemBlock(MemBlock* mb, Cond condition, int64_t val); 
    MemBlock* createScan(int64_t processId, int dataSize);
    void freeScan (MemBlock* mbLinked);
    void updateScan(MemBlock* mbLinked, Cond condition, int64_t val); 
    void poke(HANDLE pHandle, int dataSize, int64_t addr, int64_t val);
    int64_t peek(HANDLE pHandle, int dataSize, int64_t addr);
    void printMatches(MemBlock* mbLinked);
    int getMatchesCount(MemBlock* mbLinked);
    int stringToInt(std::string s);
    MemBlock* uiNewScan();
    void uiPoke(HANDLE pHandle, int dataSize);
    void uiRunScan();
    
} // namespace mem_scan

#endif /* MEMSCAN.HPP */