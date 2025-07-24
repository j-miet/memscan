#ifndef MEMSCAN_STR_HPP
#define MEMSCAN_STR_HPP

#include "memscan.hpp"

#include <handleapi.h>

#include <cstdint>


namespace mem_scan
{
    void updateSearchForString(MemBlock* mb, 
                               int bytesRead, 
                               int totalRead, 
                               int dataSize, 
                               char* tempBuf, 
                               char* val, 
                               Condition condition);
    void updateScanForString(MemBlock* mbLinked, Condition condition, char* val);
    void updateMemBlockString(MemBlock* mb, Condition condition, char* val);
    void pokeString(HANDLE pHandle, uintptr_t addr, char* val, int size);
    char* peekString(HANDLE pHandle, uintptr_t addr, int size);
} // namespace mem_scan
#endif /* MEMSCAN_STR.HPP */