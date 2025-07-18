#ifndef MEMSCAN_HPP
#define MEMSCAN_HPP

#include <handleapi.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace mem_scan
{
    // A single block of memory. Memory blocks are sequential in form of a linked list.
    struct MemBlock
    {
        HANDLE pHandle;
        //std::unique_ptr<uint8_t> addr;
        uint8_t* addr;
        int size;
        std::vector<uint8_t> buffer;
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
    void updateMemBlock(MemBlock* mb, Cond condition, uint32_t val); 
    MemBlock* createScan(uint32_t processId, int dataSize);
    void freeScan (MemBlock* mbLinked);
    void updateScan(MemBlock* mbLinked, Cond condition, uint32_t val); 
    void poke(HANDLE pHandle, int dataSize, uintptr_t addr, uint32_t val);
    uint32_t peek(HANDLE pHandle, int dataSize, uintptr_t addr);
    void printMatches(MemBlock* mbLinked);
    int getMatchesCount(MemBlock* mbLinked);
    int stringToInt(std::string s);
    MemBlock* uiNewScan();
    void uiPoke(HANDLE pHandle, int dataSize);
    void uiRunScan();
    
} // namespace mem_scan

#endif /* MEMSCAN.HPP */