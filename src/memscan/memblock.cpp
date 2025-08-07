#include "memblock.hpp"

#include <algorithm>

MemBlock::MemBlock(HANDLE pHandle, MEMORY_BASIC_INFORMATION* memInfo, int dataSize)
    : m_pHandle(pHandle)
    , m_addr(static_cast<char*>(memInfo->BaseAddress))
    , m_searchMask(memInfo->RegionSize/8, 0xff)
    , m_size(memInfo->RegionSize)
    , m_matches(memInfo->RegionSize)
    , m_dataSize(dataSize)
{
    m_buffer.resize(memInfo->RegionSize);
}

/**
 * \brief Check if BASIC_MEMORY_INFORMATION protect condition allows for reading and writing to memory location
 * \param protectCond Value of BASIC_MEMORY_INFORMATION.protect (DWORD i.e. int32)
 * \return Boolean
 */
bool MemBlock::checkPage(int32_t protectCond)
{
    auto condIter = std::find(writable.begin(), writable.end(), protectCond);
    return condIter != writable.end() ? true : false;
}

/**
 * \brief Check if offset byte is in mask
 * 
 * \return Boolean
 */
bool MemBlock::isInSearch(int offset)
{
    return (m_searchMask[(offset)/8] & (1<<(offset)%8));
}

/**
 * \brief Update mask to exclude offset value
 * 
 * \param mb Memory block
 * \param offset Offset byte
 */
void MemBlock::removeFromSearch(int offset)
{
    m_searchMask[(offset)/8] &= ~(1<<(offset)%8);
}