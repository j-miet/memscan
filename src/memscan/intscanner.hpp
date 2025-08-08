#pragma once
#include "memblock.hpp"

#include <string>

/**
 * \brief Integer scanner for 8/16/32/64 bit signed integers
 * \param memblocks Vector of MemBlocks
 */
class IntScanner
{
    public:
        IntScanner(std::vector<MemBlock> memblocks);
        ~IntScanner();

        void updateScan(Condition condition, int64_t val);
        void writeInt8(uintptr_t addr, char val);
        void writeInt16(uintptr_t addr, int16_t val);
        void writeInt32(uintptr_t addr, int32_t val);
        void writeInt64(uintptr_t addr, int64_t val);
        char readInt8(uintptr_t addr);
        int16_t readInt16(uintptr_t addr);
        int32_t readInt32(uintptr_t addr);
        int64_t readInt64(uintptr_t addr);

              HANDLE&                pHandle()         { return m_pHandle; }
        const HANDLE&                pHandle()   const { return m_pHandle; }
              std::vector<MemBlock>& memblocks()       { return m_memblocks; }
        const std::vector<MemBlock>& memblocks() const { return m_memblocks; }

    private:
        HANDLE m_pHandle;
        std::vector<MemBlock> m_memblocks;
        
        void updateSearch(MemBlock& mb, int bytesRead, std::string& tempBuf, Condition condition, int64_t val);
        void updateMemBlock(MemBlock& mb, Condition condition, int64_t val);
};