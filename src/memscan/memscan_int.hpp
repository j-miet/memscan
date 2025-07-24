#ifndef MEMSCAN_INT_HPP
#define MEMSCAN_INT_HPP

#include "memscan.hpp"

namespace mem_scan
{
    void updateScanForInt(MemBlock* mbLinked, Condition condition, int64_t val);
    void updateSearchForInt(MemBlock* mb, 
                            int64_t bytesRead, 
                            int64_t totalRead, 
                            int dataSize, 
                            std::array<int8_t, 1024*128> tempBuf, 
                            int64_t val, 
                            Condition condition);
    void updateMemBlockInt(MemBlock* mb, Condition condition, int64_t val);
    void pokeInt8(HANDLE pHandle, intptr_t addr, int8_t val);
    void pokeInt16(HANDLE pHandle, intptr_t addr, int16_t val);
    void pokeInt32(HANDLE pHandle, intptr_t addr, int32_t val);
    void pokeInt64(HANDLE pHandle, intptr_t addr, int64_t val);
    int64_t peekInt(HANDLE pHandle, int dataSize, intptr_t addr);


} // namespace mem_scan
#endif /* MEMSCAN_INT.HPP */