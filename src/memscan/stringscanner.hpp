#pragma once
#include "memblock.hpp"

#include <handleapi.h>

#include <cstdint>
#include <string>

/**
 * \brief String value scanner
 * \param memblocks Vector of MemBlocks
 */
class StringScanner
{
    public:
        StringScanner(std::vector<MemBlock> memblocks);
        ~StringScanner();

              HANDLE&                pHandle()         { return m_pHandle; }
        const HANDLE&                pHandle()   const { return m_pHandle; }
              std::vector<MemBlock>& memblocks()       { return m_memblocks; }
        const std::vector<MemBlock>& memblocks() const { return m_memblocks; }

        void updateScan(Condition condition, std::string val);
        void writeString(uintptr_t addr, std::string val);
        std::string readString(uintptr_t addr, int size);

    private:
        HANDLE m_pHandle;
        std::vector<MemBlock> m_memblocks;
        
        void updateSearch(MemBlock& mb, int bytesRead, std::string& tempBuf, Condition condition, std::string val);
        void updateMemBlock(MemBlock& mb, Condition condition, std::string val);
};