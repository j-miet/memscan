#pragma once
#include <handleapi.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

// this is closely tied to MemBlock, but is still required elsewhere. Thus it cannot be part of MemBlock.
enum Condition
{
    COND_UNCONDITIONAL, 
    COND_EQUALS,
    COND_INCREASED,
    COND_DECREASED
};

/** 
 * \brief Single memory block
 * \param pHandle Process handle
 * \param memInfo Process memory info
 * \param dataSize Data size for stored data in bytes. String values don't care about this parameter.
 */
class MemBlock
{
    public:
        MemBlock(HANDLE pHandle, MEMORY_BASIC_INFORMATION* memInfo, int dataSize);

              HANDLE&            pHandle()          { return m_pHandle; }
        const HANDLE&            pHandle()    const { return m_pHandle; }
              char*              addr()             { return m_addr; }
        const char*              addr()       const { return m_addr; }
              int&               size()             { return m_size; }
        const int&               size()       const { return m_size; }
              std::vector<char>& buffer()           { return m_buffer; }
        const std::vector<char>& buffer()     const { return m_buffer; }
              std::vector<char>& searchMask()       { return m_searchMask; }
        const std::vector<char>& searchMask() const { return m_searchMask; }
              int&               matches()          { return m_matches; }
        const int&               matches()    const { return m_matches; }
              int&               dataSize()         { return m_dataSize; }
        const int&               dataSize()   const { return m_dataSize; }

        bool isInSearch(int offset);
        bool static checkPage(int32_t protectCond);
        void removeFromSearch(int offset);

        const static inline std::vector<int> writable {PAGE_READWRITE, PAGE_WRITECOPY, PAGE_EXECUTE_READWRITE,        
                                                       PAGE_EXECUTE_WRITECOPY};

    private:
        HANDLE m_pHandle;
        char* m_addr;
        std::vector<char> m_buffer;
        std::vector<char> m_searchMask;
        int m_size;
        int m_matches;
        int m_dataSize;
};