// Memory scanner in C //

#include <handleapi.h>
#include <memoryapi.h>
#include <processthreadsapi.h>
#include <stdio.h>
#include <winerror.h>
#include <winnt.h>

#define IS_IN_SEARCH(mb,offset) (mb->search_mask[(offset)/8] & (1<<(offset)%8))
#define REMOVE_FROM_SEARCH(mb,offset) (mb->search_mask[(offset)/8] &= ~(1<<(offset)%8));

typedef struct _MEMBLOCK 
{
    HANDLE pHandle;
    unsigned char* addr;
    int size;
    unsigned char* buffer;
    unsigned char* search_mask;
    int matches;
    int data_size;
    struct _MEMBLOCK* next;
} MEMBLOCK; 

typedef enum Cond
{
    COND_UNCONDITIONAL,
    COND_EQUALS,
    COND_INCREASED,
    COND_DECREASED
} SEARCH_CONDITION;

MEMBLOCK* create_memblock(HANDLE pHandle, MEMORY_BASIC_INFORMATION* mem_info, int data_size) 
{
    MEMBLOCK* mb = (MEMBLOCK*) malloc (sizeof(MEMBLOCK));

    if (mb) 
    {
        mb->pHandle = pHandle;
        mb->addr = (unsigned char*) mem_info->BaseAddress;
        mb->size = mem_info->RegionSize;
        mb->buffer = (unsigned char*) malloc(mem_info->RegionSize);
        mb->search_mask = (unsigned char*) malloc(mem_info->RegionSize/8);
        memset(mb->search_mask, 0xff, mem_info->RegionSize/8);
        mb->matches = mem_info->RegionSize;
        mb->data_size = data_size;
        mb->next = NULL;
    }

    return mb;
}

void free_memblock(MEMBLOCK* mb) 
{
    if (mb) 
    {
        if (mb->buffer) 
        {
            free(mb->buffer);
        }
        if (mb->search_mask) 
        {
            free(mb->search_mask);
        }

        free(mb);
    }
}

void update_memblock(MEMBLOCK* mb, SEARCH_CONDITION condition, unsigned int val) 
{
    static unsigned char temp_buf[128*1024];
    unsigned int bytes_left;
    unsigned int total_read;
    unsigned int bytes_to_read;
    SIZE_T bytes_read;

    if (mb->matches > 0) 
    {
        bytes_left = mb->size;
        total_read = 0;
        mb->matches = 0;

        while (bytes_left) 
        {
            bytes_to_read = (bytes_left > sizeof(temp_buf)) ? sizeof(temp_buf) : bytes_left;
            ReadProcessMemory(mb->pHandle, mb->addr + total_read, temp_buf, bytes_to_read, &bytes_read);
            if (bytes_read != bytes_to_read) 
            {
                break;
            }
            if (condition == COND_UNCONDITIONAL) 
            {
                memset(mb->search_mask + (total_read/8), 0xff, bytes_read/8);
                mb->matches += bytes_read;
            } 
            else 
            {
                unsigned int offset;

                for (offset = 0; offset < bytes_read; offset += mb->data_size) 
                {
                    if (IS_IN_SEARCH(mb, (total_read+offset))) 
                    { 
                        BOOL isMatch = FALSE;
                        unsigned int temp_val;
                        unsigned int prev_val = 0;

                        switch (mb->data_size)
                        {
                            case 1:
                                temp_val = temp_buf[offset];
                                prev_val = *((unsigned char*)&mb->buffer[total_read+offset]);
                                break;
                            case 2:
                                temp_val = *((unsigned short*)&temp_buf[offset]);
                                prev_val = *((unsigned short*)&mb->buffer[total_read+offset]);
                                break;
                            case 4:
                            default:
                                temp_val = *((unsigned int*)&temp_buf[offset]);
                                prev_val = *((unsigned int*)&mb->buffer[total_read+offset]);
                                break;
                        }
                        
                        switch (condition) 
                        {
                            case COND_EQUALS:
                                isMatch = (temp_val == val);
                                break;
                            case COND_INCREASED:
                                isMatch = (temp_val > prev_val);
                                break;
                            case COND_DECREASED:
                                isMatch = (temp_val < prev_val);
                                break;
                            default:
                                break;
                        }

                        if (isMatch) 
                        {
                            mb->matches++;
                        } 
                        else 
                        {
                            REMOVE_FROM_SEARCH(mb, (total_read+offset));
                        }
                    }
                }
            }
            memcpy(mb->buffer + total_read, temp_buf, bytes_read);
            bytes_left -= bytes_read;
            total_read += bytes_read;
        }
        mb->size = total_read;
    }
}

MEMBLOCK* create_scan(unsigned int process_id, int data_size) 
{
    MEMBLOCK* mb_linked = NULL;
    MEMORY_BASIC_INFORMATION mem_info;
    unsigned char* addr = 0;

    HANDLE pHandle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, process_id);

    if (pHandle) 
    {
        while (1) 
        {
            SIZE_T byte_count = VirtualQueryEx(pHandle, addr, &mem_info, sizeof(mem_info));
            if (byte_count == 0 || byte_count == ERROR_INVALID_PARAMETER) 
            {
                break;
            }

            #define WRITABLE (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)
            if ((mem_info.State & MEM_COMMIT) && (mem_info.Protect & WRITABLE)) 
            {
                MEMBLOCK* mb = create_memblock(pHandle, &mem_info, data_size);
                if (mb) 
                {
                    mb->next = mb_linked;
                    mb_linked = mb;
                }
            }

            addr = (unsigned char*) mem_info.BaseAddress + mem_info.RegionSize;
        }
    }
    
    return mb_linked;
};

void free_scan (MEMBLOCK* mb_linked) 
{
    CloseHandle(mb_linked->pHandle);

    while (mb_linked) 
    {
        MEMBLOCK* mb = mb_linked;
        mb_linked = mb_linked->next;
        free_memblock(mb);
    }
}

void update_scan(MEMBLOCK* mb_linked, SEARCH_CONDITION condition, unsigned int val) 
{
    MEMBLOCK* mb = mb_linked;

    while (mb) 
    {
        update_memblock(mb, condition, val);
        mb = mb->next;
    }
}

void dump_scan_info(MEMBLOCK* mb_linked) 
{
    MEMBLOCK* mb = mb_linked;
    
    while (mb) 
    {
        printf("0x%d %d\r\n", (unsigned char*) mb->addr, mb->size);
        mb = mb->next;
    }
}

void poke(HANDLE pHandle, int data_size, uintptr_t addr, unsigned int val)
{
    if (WriteProcessMemory(pHandle, (void*) addr, &val, data_size, NULL) == 0)
    {
        printf("poke failed\r\n");
    }
}

unsigned int peek(HANDLE pHandle, int data_size, uintptr_t addr)
{
    unsigned int val = 0;

    if (ReadProcessMemory(pHandle, (void*) addr, &val, data_size, NULL) == 0)
    {
        printf("peek failed\r\n");
    }

    return val;
}

void print_matches(MEMBLOCK* mb_linked) 
{
    unsigned int offset;
    MEMBLOCK* mb = mb_linked;

    while (mb) 
    {
        for (offset = 0; offset < mb->size; offset += mb->data_size) 
        {
            if (IS_IN_SEARCH(mb, offset)) 
            {
                unsigned int val = peek(mb->pHandle, mb->data_size, (uintptr_t) mb->addr + offset);
                printf("0x%d-> value: 0x%d | size: %d\r\n", (uintptr_t)(mb->addr) + offset, val, mb->size);
            }
        }

        mb = mb->next;
    }
}

int get_matches_count(MEMBLOCK* mb_linked) 
{
    MEMBLOCK* mb = mb_linked;
    int count = 0;

    while (mb) 
    {
        count += mb->matches;
        mb = mb->next;
    }

    return count;
}

unsigned int string_to_int(char* s)
{
    int base = 10;

    if (s[0] == '0' && s[1] == 'x')
    {
        base = 16;
        s += 2;
    }

    return strtoul(s, NULL, base);
}

MEMBLOCK* ui_new_scan()
{
    MEMBLOCK* scan = NULL;
    DWORD pid;
    int data_size;
    unsigned int start_value;
    SEARCH_CONDITION start_condition;
    char s[20];

    while (1)
    {
        printf("\r\nEnter the process id: ");
        fgets(s, sizeof(s), stdin);
        pid = string_to_int(s);

        printf("\r\nEnter the data size: ");
        fgets(s, sizeof(s), stdin);
        data_size = string_to_int(s);

        printf("\r\nEnter the start value, or 'u' for unknown: ");
        fgets(s, sizeof(s), stdin);
        if (s[0] == 'u')
        {
            start_condition = COND_UNCONDITIONAL;
            start_value = 0;
        }
        else
        {
            start_condition = COND_EQUALS;
            start_value = string_to_int(s);
        }

        scan = create_scan(pid, data_size);
        if (scan)
        {
            break;
        }
        
        printf("\r\nInvalid scan");
    }

    update_scan(scan, start_condition, start_value);
    printf("\r\n%d matches found\n\n", get_matches_count(scan));

    return scan;
}

void ui_poke(HANDLE pHandle, int data_size)
{
    unsigned int addr;
    unsigned int val;
    char s[20];

    printf("Enter the address :" );
    fgets(s, sizeof(s), stdin);
    addr = string_to_int(s);

    printf("\n\nEnter the value: ");
    fgets(s, sizeof(s), stdin);
    val = string_to_int(s);
    printf("\r\n");

    poke(pHandle, data_size, addr, val);
}

void ui_run_scan()
{
    unsigned int val;
    char s[20];
    MEMBLOCK* scan;

    scan = ui_new_scan();

    while (1)
    {
        printf("\r\nEnter the next value or, ");
        printf("\r\n[i] increased");
        printf("\r\n[d] decreased");
        printf("\r\n[m] print matches");
        printf("\r\n[p] poke address");
        printf("\r\n[n] new scan");
        printf("\r\n[q] quit\r\n");

        fgets(s, sizeof(s), stdin);
        printf("\r\n");

        switch (s[0])
        {
            case 'i':
                update_scan(scan, COND_INCREASED, 0);
                printf("%d matches found\r\n", get_matches_count(scan));
                break;
            case 'd':
                update_scan(scan, COND_DECREASED, 0);
                printf("%d matches found\r\n", get_matches_count(scan)); 
                break;
            case 'm':
                print_matches(scan);
                break;
            case 'p':
                ui_poke(scan->pHandle, scan->data_size);
                break;
            case 'n':
                free_scan(scan);
                scan = ui_new_scan();
                break;
            case 'q':
                free_scan(scan);
                return;
            default:
                val = string_to_int(s);
                update_scan(scan, COND_EQUALS, val);
                printf("%d matches found", get_matches_count(scan));
                break;
        }
    }
}

int main(int argc, char* argv[]) 
{
    ui_run_scan();
    return 0;
}