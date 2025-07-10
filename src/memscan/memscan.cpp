/* Memory scanner in C++ (in progress)
Based on a youtube tutorial written in C (https://www.youtube.com/watch?v=YRPMdb1YMS8&list=WL; has total of 8 parts)

How to run:
- compile code with g++: 
    g++ -o memscan memscan.cpp
- get a process id with 'tasklist' command, then run application with
    mescan process_id OR ./memscan process_id

Currently:
- can access process memory and read information:
  - creates a scanner which takes process id and collects all of its memory data into MemBlock structs
  - blocks form a linked list structure which allows easy iteration; they include both a reference to base
  address (process memory) and to local address (temporary buffer)
  - prints addresses and byte sizes of each accessed block
  - then frees all memory allocated from calling malloc inside MemBlocks

TODO:
- write process data into local buffer
- write data back into process memory base address
- possibly add a Gui (library will be decided later)

Lots and lots of comments to help learn and recall stuff. Most of these will be removed later.
*/

#include <handleapi.h>
#include <iostream>
#include <memoryapi.h>
#include <processthreadsapi.h>
#include <winerror.h>
#include <winnt.h>

/// @brief A single block of memory
typedef struct MemBlock {
    HANDLE pHandle; // process handle; which process uses this piece of memory
    unsigned char* addr; // base address
    int size; // memory size
    unsigned char* buffer; // local address to copy and store data
    struct MemBlock* next; // pointer to next block memory; thus MemBlocks form a linked list
} MemBlock; 

/// @brief Create a local memory block for a process
/// @param hProc Process handle
/// @param memInfo Stored memory information
/// @return Pointer to memory block
MemBlock* createMemBlock(HANDLE pHandle, MEMORY_BASIC_INFORMATION* memInfo) {
    MemBlock* mb = (MemBlock*) malloc (sizeof(MemBlock));

    if (mb) {
        mb->pHandle = pHandle;
        mb->addr = (unsigned char*) memInfo->BaseAddress;
        mb->size = memInfo->RegionSize;
        mb->buffer = (unsigned char*) malloc(memInfo->RegionSize); // allocate space for memInfo
        mb->next = NULL;
    }
    return mb;
}

/// @brief Free allocated memory block
/// @param mb Block address
void freeMemBlock(MemBlock* mb) {
    if (mb) {
        if (mb->buffer) {
            free(mb->buffer);
        }
        free(mb);
    }
}

/// @brief Scan and access the linked list of external memory blocks
/// @param processId process id
/// @return pointer to head of linked list 
MemBlock* createScan(unsigned int processId) {
    MemBlock* mbLinked = nullptr;
    MEMORY_BASIC_INFORMATION memInfo; // info of current memory chunk
    unsigned char *addr = 0; // virtual query tracking address; starting from lowest value 0 is important because

    // get process handle: need full access to main process, but not to child processes
    HANDLE pHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);

    if (pHandle) {
        while (1) {
            // VirtualQueryEx performs a query to return memory information of passed process
            // it requires 
            //  - process handle which memory is queried
            //  - pointer to base address where query begins
            //  - memory information address where query information is written
            //  - size of buffer where memory information is temporarily stored before writing it to meminfo. This is 
            //    therefore equal to size of meminfo
            // Returns 0 if failed or base address value gets too high, otherwise a non-zero value which is equal to
            // the number of bytes in the meminfo buffer
            SIZE_T byteCount = VirtualQueryEx(pHandle, addr, &memInfo, sizeof(memInfo));
            if (byteCount == 0 || byteCount == ERROR_INVALID_PARAMETER) {
                break;
            }

            // discards unreadable/unwritable blocks i.e. if meminfo.State gets values MEM_FREE or MEM_RESERVE
            // similar logic is applied to meminfo.Protect to check if block allows for writing data except
            // it can result to 0 from other values. To limit scope only to writable checks, a #define directive is used
            // Finally, bitwise AND operator is used to apply a masking in order to match to desired values
            #define WRITABLE (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)
            if ((memInfo.State & MEM_COMMIT) && (memInfo.Protect & WRITABLE)) {
                MemBlock* mb = createMemBlock(pHandle, &memInfo);
                if (mb) {
                    // add a node to linked list
                    mb->next = mbLinked;
                    mbLinked = mb;
                }
            }

            // move base address pointer to start of next block (range of pages)
            addr = (unsigned char*) memInfo.BaseAddress + memInfo.RegionSize;
        }
    }
    return mbLinked;
};

/// @brief Free allocated memory of scan list
/// @param mbLinked Linked list i.e. pointer to head node
void freeScan (MemBlock* mbLinked) {
    CloseHandle(mbLinked->pHandle); // close the process

    while (mbLinked) {
        MemBlock* mb = mbLinked; // point to current head node
        mbLinked = mbLinked->next; // move list head to next node
        freeMemBlock(mb); // free discarded memory node
    }
}

/// @brief Print info of accessed memory blocks
/// @param mbLinked Linked list
void dumpScanInfo(MemBlock *mbLinked) {
    MemBlock* mb = mbLinked;
    
    while (mb) {
        //printf("0x%08x %d\r\n", mb->addr, mb->size); // C printf to test and compare
        std::cout << "0x" << std::hex << (long long)(mb->addr) << std::dec << " " << mb->size << "\r\n";
        mb = mb->next;
    }
}

int main(int argc, char *argv[]) {
    MemBlock* scan = createScan(atoi(argv[1]));
    if (scan) {
        dumpScanInfo(scan);
        freeScan(scan);
    }

    return 0;
}