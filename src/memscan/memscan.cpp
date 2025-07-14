/* Memory scanner in C++ (in progress)
Based on a youtube tutorial written in C (https://www.youtube.com/watch?v=YRPMdb1YMS8&list=WL; has total of 8 parts)

How to run:
- compile code with g++: 
    g++ -o memscan memscan.cpp
- get a process id with 'tasklist' command, then run application with
    mescan process_id OR ./memscan process_id

Implemented:
- can access process memory address:
  - creates a scanner which takes process id and collects all of its memory data into MemBlock structs
  - blocks form a linked list structure which allows easy iteration; they include both a reference to base
  address (process memory) and to local address (temporary buffer)
  - prints addresses and byte sizes of each accessed block
  - then frees all memory allocated from calling malloc inside MemBlocks

- read process memory from an address by applying a search mask
- write process data into local buffer

TODO:
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
//#include <bitset>

// macros for searching memory; first checks if offset value is in mask, second updates mask to exclude offset value
#define IS_IN_SEARCH(mb,offset) (mb->searchMask[(offset)/8] & (1<<(offset)%8))
#define REMOVE_FROM_SEARCH(mb,offset) (mb->searchMask[(offset)/8] &= ~(1<<(offset)%8)); // ~ is bitwise NOT/complement

/// @brief A single block of memory
typedef struct MemBlock 
{
    HANDLE pHandle; // process handle; which process uses this piece of memory
    unsigned char* addr; // base address: where process stores memory
    int size; // memory size
    unsigned char* buffer; // local address: read data from base addr, manipulate it, then write it back to base addr
    unsigned char* searchMask; // mask where each bit is 1 for matching byte in buffer and 0 otherwise
    int matches; // number of matched bytes
    int dataSize; // on what basis are search comparisons done (1 for uchar, 2 for ushort, 4 for uint)
    struct MemBlock* next; // pointer to next block memory i.e. next node of linked list
} MemBlock; 

typedef enum Cond
{
    COND_UNCONDITIONAL, // everything is accepted, default setting
    COND_EQUALS, // search matches particular value

    COND_INCREASED, // include byte if its value has increase from last search
    COND_DECREASED, // same as above, but value has decreased
} SEARCH_CONDITION;

/// @brief Create a local memory block for a process
/// @param hProc Process handle
/// @param memInfo Internal information of base block
/// @return Pointer to memory block
MemBlock* createMemBlock(HANDLE pHandle, 
                         MEMORY_BASIC_INFORMATION* memInfo, 
                         int dataSize) 
{
    MemBlock* mb = (MemBlock*) malloc (sizeof(MemBlock));

    if (mb) 
    {
        mb->pHandle = pHandle;
        mb->addr = (unsigned char*) memInfo->BaseAddress;
        mb->size = memInfo->RegionSize;
        mb->buffer = (unsigned char*) malloc(memInfo->RegionSize); // allocate space for memInfo
        mb->searchMask = (unsigned char*) malloc(memInfo->RegionSize/8);
        memset(mb->searchMask, 0xff, memInfo->RegionSize/8); // 0xff is equal to 1111 1111 in binary
        mb->matches = memInfo->RegionSize; // initially search for every byte i.e. match to entire buffer
        mb->dataSize = dataSize;
        mb->next = NULL;
    }

    return mb;
}

/// @brief Free allocated memory block
/// @param mb Block address
void freeMemBlock(MemBlock* mb) 
{
    if (mb) 
    {
        if (mb->buffer) 
        {
            free(mb->buffer);
        }
        if (mb->searchMask) 
        {
            free(mb->searchMask);
        }

        free(mb);
    }
}

/// @brief Read information into memory block buffer by applying a search condition
/// @param mb Block address 
/// @param condition Search condition
/// @param val Comparison value when condition is COND_EQUALS
void updateMemBlock(MemBlock* mb, 
                    SEARCH_CONDITION condition, 
                    unsigned int val) 
{
    static unsigned char tempBuf[128*1024]; // byte buffer; 1024 bits = 128 bytes
    unsigned int bytesLeft;  // how many bytes left to read in total
    unsigned int totalRead; // how many read so far in total
    unsigned int bytesToRead; // number of bytes to be read
    unsigned int bytesRead; // actual bytes read

    if (mb->matches > 0) 
    {
        bytesLeft = mb->size; // how many bytes left to read, initially full size
        totalRead = 0;
        mb->matches = 0; // reset matches for current block

        while (bytesLeft) 
        {
            bytesToRead = (bytesLeft > std::size(tempBuf)) ? std::size(tempBuf) : bytesLeft;
            // I have no idea what happens here, but still managed to fix the issue:
            // - after running ReadProcessMemory, bytesToRead resets to 0 for some reason: only tempBuf and &bytesRead 
            // contents should be modified so what's going on here.
            // - In order to fix this, I would assign a variable 'a' as placeholder, insert bytesToRead value inside it then
            // use it afterwards. Turns out, 'a' is not needed directly, only declaring it fixed the issue. 
            // Yeah... at least it works now, even if it doesn't make sense.
            unsigned int a = bytesToRead;
            ReadProcessMemory(mb->pHandle, mb->addr + totalRead, tempBuf, bytesToRead, (SIZE_T*) &bytesRead);
            if (bytesRead != bytesToRead) 
            {
                // if byte count differs from buffer size
                break;
            }
            if (condition == COND_UNCONDITIONAL) 
            {
                // no comparison required
                // set start index based on totalRead (byte-normalized), write 1s by amount of bytesRead (again normalized)
                memset(mb->searchMask + (totalRead/8), 0xff, bytesRead/8);
                mb->matches += bytesRead;
            } 
            else 
            {
                unsigned int offset; // offset of tempBuf i.e. local offset

                for (offset = 0; offset < bytesRead; offset += mb->dataSize) 
                {
                    if (IS_IN_SEARCH(mb, (totalRead+offset))) 
                    {   // totalRead+offset is the overall offset
                        BOOL isMatch = FALSE;
                        unsigned int tempVal;
                        unsigned int prevVal = 0; // previous tempVal

                        switch (mb->dataSize)
                        {
                            case 1: // byte (unsigned char)
                                tempVal = tempBuf[offset];
                                // buffer offset before copying new value into it
                                prevVal = *((unsigned char*)&mb->buffer[totalRead+offset]);
                                break;
                            case 2: // 2 bytes (unsigned short)
                                tempVal = *((unsigned short*)&tempBuf[offset]);
                                prevVal = *((unsigned short*)&mb->buffer[totalRead+offset]);
                                break;
                            case 4: // 4 bytes (unsigned int)
                            default:
                                tempVal = *((unsigned int*)&tempBuf[offset]);
                                prevVal = *((unsigned int*)&mb->buffer[totalRead+offset]);
                                break;
                        }
                        
                        switch (condition) 
                        {
                            case COND_EQUALS: // match to passed argument 'val'
                                isMatch = (tempVal == val);
                                break;
                            case COND_INCREASED:
                                isMatch = (tempVal > prevVal);
                                break;
                            case COND_DECREASED:
                                isMatch = (tempVal < prevVal);
                                break;
                            default:
                                break;
                        }

                        if (isMatch) 
                        {
                            mb->matches++; // increase match counter
                        } 
                        else 
                        {
                            REMOVE_FROM_SEARCH(mb, (totalRead+offset)); // otherwise set overall offset byte to 0
                        }
                    }
                }
            }
            // copy tempBuf data of size bytesRead into MemBlock buffer
            //
            // why read memory into tempBuf and immediately copy it to MemBlock buffer, instead of just copy it to MemBlock
            // straight away?
            // It creates an intermediary state which allows comparing current buffer data with previously stored buffer
            memcpy(mb->buffer + totalRead, tempBuf, bytesRead);
            bytesLeft -= bytesRead;
            totalRead += bytesRead;
        }
        mb->size = totalRead; // update size to match total amount of bytes read and to flag rest of buffer as invalid
    }
}

/// @brief Scan and access the linked list of external memory blocks
/// @param processId Process id
/// @param dataSize Memory block reading size
/// @return Pointer to head of linked list 
MemBlock* createScan(unsigned int processId, int dataSize) 
{
    MemBlock* mbLinked = nullptr;
    MEMORY_BASIC_INFORMATION memInfo; // info of current memory chunk
    unsigned char* addr = 0; // virtual query tracking address; starting from lowest value 0 is important because

    // get process handle: need full access to main process, but not to child processes
    HANDLE pHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);

    if (pHandle) 
    {
        while (1) 
        {
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
            if (byteCount == 0 || byteCount == ERROR_INVALID_PARAMETER) 
            {
                break;
            }

            // discards unreadable/unwritable blocks i.e. if meminfo.State gets values MEM_FREE or MEM_RESERVE
            // similar logic is applied to meminfo.Protect to check if block allows for writing data except
            // it can result to 0 from other values. To limit scope only to writable checks, a #define directive is used
            // Finally, bitwise AND operator is used to apply a masking in order to match to desired values
            #define WRITABLE (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)
            if ((memInfo.State & MEM_COMMIT) && (memInfo.Protect & WRITABLE)) 
            {
                MemBlock* mb = createMemBlock(pHandle, &memInfo, dataSize);
                if (mb) 
                {
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
/// @param mbLinked Linked list head
void freeScan (MemBlock* mbLinked) 
{
    CloseHandle(mbLinked->pHandle); // close the process

    while (mbLinked) 
    {
        MemBlock* mb = mbLinked; // point to current head node
        mbLinked = mbLinked->next; // move list head to next node
        freeMemBlock(mb); // free discarded memory node
    }
}

/// @brief Updates all memory blocks
/// @param mbLinked Linked list head
/// @param condition Search condition
/// @param int Value to compare to if condition has value COND_EQUALS
void updateScan(MemBlock* mbLinked, 
                SEARCH_CONDITION condition, 
                unsigned int val) 
{
    MemBlock* mb = mbLinked;

    while (mb) 
    {
        updateMemBlock(mb, condition, val);
        mb = mb->next;
    }
}

/// @brief Print info of accessed memory blocks
/// @param mbLinked Linked list head
void dumpScanInfo(MemBlock* mbLinked) 
{
    MemBlock* mb = mbLinked;
    
    while (mb) 
    {
        //printf("0x%08x %d\r\n", mb->addr, mb->size); // C printf to test and compare
        // print memory addresses and their sizes
        std::cout << "0x" << std::hex << (intptr_t)(mb->addr) << std::dec << " " << mb->size << "\r\n";

        // uncomment this to test output (remember to also uncomment #include <bitset>)
        // it will print buffer contents in binary, and might take a while to finish!
        /*
        int i;
        for (i = 0; i< mb->size; i++) {
            // print buffer contents
            std::cout << std::bitset<32>(mb->buffer[i]);
        }
        std::cout << "\r\n";
        */

        mb = mb->next;
    }
}

/// @brief Print search matches
/// @param mbLinked Linked list head
void printMatches(MemBlock* mbLinked) 
{
    unsigned int offset;
    MemBlock* mb = mbLinked;

    while (mb) 
    {
        for (offset = 0; offset < mb->size; offset += mb->dataSize) 
        {
            if (IS_IN_SEARCH(mb, offset)) 
            {
                std::cout << "0x" << std::hex << (long long)(mb->addr) + offset << std::dec;
                std::cout << " " << mb->size << "\r\n"; 
            }
        }

        mb = mb->next;
    }
}

/// @brief Return match count
/// @param mbLinked Linked list head
/// @return Match count
int getMatchesCount(MemBlock* mbLinked) 
{
    MemBlock* mb = mbLinked;
    int count = 0;

    while (mb) 
    {
        count += mb->matches;
        mb = mb->next;
    }

    return count;
}

int main(int argc, char* argv[]) 
{
    MemBlock* scan = createScan(atoi(argv[1]), 4); // data size 4 bytes; other possible values are 1 or 2
    if (scan) 
    {

        // temporary test sequence
         // applies search to addresses where value 1000 is included
        std::cout << "searching for 1000\r\n";
        updateScan(scan, COND_EQUALS, 1000);
        printMatches(scan);

        {
            char s[10];
            fgets(s, sizeof s, stdin);
        }

        // then apply a new search to previously found addresses; this might return nothing because it's unlikely that
        // the process suddenly changed existing register values between reads 
        // e.g. open notepad.exe -> read memory - do nothing -> read again
        std::cout << "searching for decreassed\r\n";
        updateScan(scan, COND_DECREASED, 0);
        printMatches(scan);
        //dumpScanInfo(scan);

        freeScan(scan);
    }

    return 0;
}