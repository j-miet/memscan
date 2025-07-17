#ifndef MEMSCAN_HPP
#define MEMSCAN_HPP

#include <cstdint>

#include <handleapi.h>


namespace mem_scan
{
    // A single block of memory
    struct MemBlock
    {
        HANDLE pHandle;
        uint8_t* addr;
        int size;
        uint8_t* buffer;
        uint8_t* searchMask;
        int matches;
        int dataSize;
        struct MemBlock* next;
    };
    // Memory search conditions
    enum Cond
    {
        COND_UNCONDITIONAL, 
        COND_EQUALS,
        COND_INCREASED,
        COND_DECREASED
    };

    bool checkPage(uint32_t protectCond);
    bool isInSearch(MemBlock* mb, int offset);
    void removeFromSearch(MemBlock* mb, int offset);
    MemBlock* createMemBlock(HANDLE pHandle, MEMORY_BASIC_INFORMATION* memInfo, int dataSize);
    void freeMemBlock(MemBlock* mb);
    void updateMemBlock(MemBlock* mb, Cond condition, uint32_t val); 
    MemBlock* createScan(int64_t processId, int dataSize);
    void freeScan (MemBlock* mbLinked);
    void updateScan(MemBlock* mbLinked, Cond condition, uint32_t val); 
    void poke(HANDLE pHandle, int dataSize, uintptr_t addr, uint32_t val);
    int64_t peek(HANDLE pHandle, int dataSize, uintptr_t addr);
    void printMatches(MemBlock* mbLinked);
    int getMatchesCount(MemBlock* mbLinked);
    int64_t stringToInt(char* s);
    MemBlock* uiNewScan();
    void uiPoke(HANDLE pHandle, int dataSize);
    void uiRunScan();
    
} // namespace mem_scan
#endif /* MEMSCAN.HPP */