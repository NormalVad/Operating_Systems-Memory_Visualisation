#include "mmu.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

// byte addressable memory
unsigned char RAM[RAM_SIZE];  


// OS's memory starts at the beginning of RAM.
// Store the process related info, page tables or other data structures here.
// do not use more than (OS_MEM_SIZE: 72 MB).
unsigned char* OS_MEM = RAM;  

// memory that can be used by processes.   
// 128 MB size (RAM_SIZE - OS_MEM_SIZE)
unsigned char* PS_MEM = RAM + OS_MEM_SIZE; 


// This first frame has frame number 0 and is located at start of RAM(NOT PS_MEM).
// We also include the OS_MEM even though it is not paged. This is 
// because the RAM can only be accessed through physical RAM addresses.  
// The OS should ensure that it does not map any of the frames, that correspond
// to its memory, to any process's page. 
int NUM_FRAMES = ((RAM_SIZE) / PAGE_SIZE);

// Actual number of usable frames by the processes.
int NUM_USABLE_FRAMES = ((RAM_SIZE - OS_MEM_SIZE) / PAGE_SIZE);

// To be set in case of errors. 
int error_no; 

// Given 128 MB free space and page size is 4KB, hence number of pages
// are (128/4)*1024 = 32*1024 pages.
// Thus the indexes for the binary free list. 
int start_free_list = 0;
int end_free_list = ((RAM_SIZE - OS_MEM_SIZE)/PAGE_SIZE) - 1;

int start_page_tables = ((RAM_SIZE - OS_MEM_SIZE)/PAGE_SIZE);
int end_page_tables = ((RAM_SIZE - OS_MEM_SIZE)/ PAGE_SIZE) + 4108*100 - 1;


// ---------------------- Helper functions for Page table entries ------------------ // 

int free_PF(){
    int i = start_free_list;
    while(i<=end_free_list){
        if((int) RAM[i] == 0){
            return i;
        }
        i++;
    }
    return -1;
}

int free_pcb_index(){
    struct PCB* iter = (struct PCB*) (&OS_MEM[start_page_tables]);
    int i = 0;
    while(i<100){
        if(iter->is_free){
            return i;
        }
        iter++;
        i++;
    }
    return -1;
}

// return the frame number from the pte
int pte_to_frame_num(page_table_entry pte) 
{
    // TODO: student
    return (pte & (65535<<4))>>4;
}

// return 1 if read bit is set in the pte
// 0 otherwise
int is_readable(page_table_entry pte) {
    // TODO: student
    return ((pte & O_READ) == 1);
}

// return 1 if write bit is set in the pte
// 0 otherwise
int is_writeable(page_table_entry pte) {
    // TODO: student
    return (((pte & O_WRITE)>>1) == 1);
}

// return 1 if executable bit is set in the pte
// 0 otherwise
int is_executable(page_table_entry pte) {
    // TODO: student
    return (((pte & O_EX)>>2) == 1);
}

// return 1 if present bit is set in the pte
// 0 otherwise
int is_present(page_table_entry pte) {
    // TODO: student
    return ((pte & 8) == 8);
}

int get_flags(page_table_entry pte){
    return (pte & 7);
}


///


page_table_entry pte_construct(int frame_num, int present, int flags){
    if(flags > 7 || flags < 0){
        printf("ERROR: flags can be in [0,7] \n");
    }
    if(present != 0 && present != 1){
        printf("ERROR: present can be either be 0 or 1 \n");
    }
    return (page_table_entry) ((1<<31) + (frame_num<<4) + (present<<3) + flags); 
}

void os_init() {
    // TODO student 
    // initialize your data structures.
    for(int i = start_free_list;i<=end_free_list;i++){
        RAM[i] = (unsigned char) 0;
    }
    for(int i = 0;i<100;i++){
        struct PCB* tmp = (struct PCB*) (&OS_MEM[start_page_tables + 4108*i]);
        tmp->count_page_table = 0;
        tmp->is_free = 1;
        tmp->pid = i;
        int j = 0;
        while(j<1024){
            tmp->page_table[j] = pte_construct(0,0,0);
            j++;
        }
    }
}


// ----------------------------------- Functions for managing memory --------------------------------- //

/**
 *  Process Virtual Memory layout: 
 *  ---------------------- (virt. memory start 0x00)
 *        code
 *  ----------------------  
 *     read only data 
 *  ----------------------
 *     read / write data
 *  ----------------------
 *        heap
 *  ----------------------
 *        stack  
 *  ----------------------  (virt. memory end 0x3fffff)
 * 
 * 
 *  code            : read + execute only
 *  ro_data         : read only
 *  rw_data         : read + write only
 *  stack           : read + write only
 *  heap            : (protection bits can be different for each heap page)
 * 
 *  assume:
 *  code_size, ro_data_size, rw_data_size, max_stack_size, are all in bytes
 *  code_size, ro_data_size, rw_data_size, max_stack_size, are all multiples of PAGE_SIZE
 *  code_size + ro_data_size + rw_data_size + max_stack_size < PS_VIRTUAL_MEM_SIZE
 *  
 * 
 *  The rest of memory will be used dynamically for the heap.
 * 
 *  This function should create a new process, 
 *  allocate code_size + ro_data_size + rw_data_size + max_stack_size amount of physical memory in PS_MEM,
 *  and create the page table for this process. Then it should copy the code and read only data from the
 *  given `unsigned char* code_and_ro_data` into processes' memory.
 *   
 *  It should return the pid of the new process.  
 *  
 */
int create_ps(int code_size, int ro_data_size, int rw_data_size,
                 int max_stack_size, unsigned char* code_and_ro_data) 
{   
    // TODO student
    int pcb_index_allocate = free_pcb_index();
    if(pcb_index_allocate == -1){
        printf("ERROR: no free space available \n");
    }
    struct PCB* atm = (struct PCB*) (&OS_MEM[start_page_tables + 4108*pcb_index_allocate]);
    atm->is_free = 0;
    int pid_allocate = atm->pid;
    int i = 0;
    int no_pages_code = code_size/PAGE_SIZE;
    while(i < no_pages_code){
        int PF_allocate = free_PF();
        RAM[PF_allocate] = (unsigned char) 1;
        atm->page_table[i] = pte_construct((PF_allocate + OS_MEM_SIZE/PAGE_SIZE), 1, 5);
        memcpy(OS_MEM + (PF_allocate + OS_MEM_SIZE/PAGE_SIZE)*4*1024, code_and_ro_data, 4*1024);
        code_and_ro_data += 4*1024;
        atm->count_page_table++;
        i++;
    }
    int no_pages_ro_data = ro_data_size/PAGE_SIZE;
    i = 0;
    while(i < no_pages_ro_data){
        int page_allocate = no_pages_code + i;
        if(page_allocate == -1){
            printf("ERROR: no page available to be allocated in virtual memory");
        }
        int PF_allocate = free_PF();
        RAM[PF_allocate] = (unsigned char) 1;
        atm->page_table[page_allocate] = pte_construct((PF_allocate + OS_MEM_SIZE/PAGE_SIZE), 1, 1);
        memcpy(OS_MEM + (PF_allocate + OS_MEM_SIZE/PAGE_SIZE)*4*1024, code_and_ro_data, 4*1024);
        code_and_ro_data += 4*1024;
        atm->count_page_table++;
        i++;
    }
    int no_pages_rw_data = rw_data_size/PAGE_SIZE;
    i = 0;
    while(i < no_pages_rw_data){
        int page_allocate = no_pages_code + no_pages_ro_data + i;
        int PF_allocate = free_PF();
        RAM[PF_allocate] = (unsigned char) 1;
        atm->page_table[page_allocate] = pte_construct((PF_allocate + OS_MEM_SIZE/PAGE_SIZE), 1, 3);
        atm->count_page_table++;
        i++;
    }
    int no_pages_stack = max_stack_size/PAGE_SIZE;
    i = 0;
    while(i < no_pages_stack){
        int page_allocate = 1023 + i + 1 - no_pages_stack;
        int PF_allocate = free_PF();
        RAM[PF_allocate] = (unsigned char) 1;
        atm->page_table[page_allocate] = pte_construct((PF_allocate + OS_MEM_SIZE/PAGE_SIZE), 1, 3);
        atm->count_page_table++;
        i++;
    }
    return pid_allocate;
}

/**
 * This function should deallocate all the resources for this process. 
 * 
 */
void exit_ps(int pid) 
{
   // TODO student
   struct PCB* atm = (struct PCB*) (&OS_MEM[start_page_tables + 4108*pid]);
   atm->is_free = 1;
   int i = 0;
   while(i < 1024){
    if(is_present(atm->page_table[i])){
        int FN_drop = pte_to_frame_num(atm->page_table[i]);
        RAM[(FN_drop - OS_MEM_SIZE/PAGE_SIZE)] = (unsigned char) 0;
        atm->page_table[i] = pte_construct(0,0,0);
    }
    i++;
   }
   atm->count_page_table = 0;
}



/**
 * Create a new process that is identical to the process with given pid. 
 * 
 */
int fork_ps(int pid) {

    // TODO student:
    int pcb_allocate = free_pcb_index();
    struct PCB* atm = (struct PCB*) (&OS_MEM[start_page_tables + 4108*pcb_allocate]);
    struct PCB* make_copy = (struct PCB*) (&OS_MEM[start_page_tables + 4108*pid]);
    if(pcb_allocate == -1){
        printf("ERROR: no free space available \n");
    }
    atm->is_free = 0;
    int pid_allocate = atm->pid;
    int i = 0;
    while(i<1024){
        page_table_entry pte = make_copy->page_table[i];
        if(is_present(pte)){
            int PF_allocate = free_PF();
            RAM[PF_allocate] = (unsigned char) 1;
            int fl = get_flags(make_copy->page_table[i]);
            atm->page_table[i] = pte_construct((PF_allocate + OS_MEM_SIZE/PAGE_SIZE), 1, fl);
            int pte_to_FN = pte_to_frame_num(make_copy->page_table[i]);
            memcpy(OS_MEM + (PF_allocate + OS_MEM_SIZE/PAGE_SIZE)*4*1024, OS_MEM + (pte_to_FN)*4*1024, 4*1024);
            atm->count_page_table++;
        }
        i++;
    }
    return pid_allocate;
}



// dynamic heap allocation
//
// Allocate num_pages amount of pages for process pid, starting at vmem_addr.
// Assume vmem_addr points to a page boundary.  
// Assume 0 <= vmem_addr < PS_VIRTUAL_MEM_SIZE
//
//
// Use flags to set the protection bits of the pages.
// Ex: flags = O_READ | O_WRITE => page should be read & writeable.
//
// If any of the pages was already allocated then kill the process, deallocate all its resources(ps_exit) 
// and set error_no to ERR_SEG_FAULT.
void allocate_pages(int pid, int vmem_addr, int num_pages, int flags) 
{
   // TODO student
    struct PCB* atm = (struct PCB*) (&OS_MEM[start_page_tables + 4108*pid]);
    if(atm->is_free){
        error_no = ERR_SEG_FAULT;
    }
    int i = vmem_addr/PAGE_SIZE;
    while(i < (vmem_addr/PAGE_SIZE + num_pages)){
        if(is_present(atm->page_table[i] == 1)){
            error_no = ERR_SEG_FAULT;
            exit_ps(pid);
            return;
        }
        int PF_allocate = free_PF();
        RAM[PF_allocate] = (unsigned char) 1;
        atm->page_table[i] = pte_construct((PF_allocate + OS_MEM_SIZE/PAGE_SIZE), 1, flags);
        i++;
    }
    atm->count_page_table += num_pages;
}



// dynamic heap deallocation
//
// Deallocate num_pages amount of pages for process pid, starting at vmem_addr.
// Assume vmem_addr points to a page boundary
// Assume 0 <= vmem_addr < PS_VIRTUAL_MEM_SIZE

// If any of the pages was not already allocated then kill the process, deallocate all its resources(ps_exit) 
// and set error_no to ERR_SEG_FAULT.
void deallocate_pages(int pid, int vmem_addr, int num_pages) 
{
   // TODO student
    struct PCB* atm = (struct PCB*) (&OS_MEM[start_page_tables + 4108*pid]);
    if(atm->is_free){
        error_no = ERR_SEG_FAULT;
    }
    int i = vmem_addr/PAGE_SIZE;
    while(i < (vmem_addr/PAGE_SIZE + num_pages)){
        if(is_present(atm->page_table[i]) == 0){
            error_no = ERR_SEG_FAULT;
            exit_ps(pid);
            return;
        }
        int FN_drop = pte_to_frame_num(atm->page_table[i]);
        RAM[(FN_drop - OS_MEM_SIZE/PAGE_SIZE)] = (unsigned char) 0;
        atm->page_table[i] = pte_construct(0,0,0);
        i++;
    }
    atm->count_page_table -= num_pages;
}

// Read the byte at `vmem_addr` virtual address of the process
// In case of illegal memory access kill the process, deallocate all its resources(ps_exit) 
// and set error_no to ERR_SEG_FAULT.
// 
// assume 0 <= vmem_addr < PS_VIRTUAL_MEM_SIZE
unsigned char read_mem(int pid, int vmem_addr) 
{
    // TODO: student
    struct PCB* atm = (struct PCB*) (&OS_MEM[start_page_tables + 4108*pid]);
    if(atm->is_free){
        error_no = ERR_SEG_FAULT;
    }
    int page_num = (int) vmem_addr/PAGE_SIZE;
    int byte_offset = vmem_addr%PAGE_SIZE;
    if(is_readable(atm->page_table[page_num]) == 0){
        error_no = ERR_SEG_FAULT;
        exit_ps(pid);
        printf("ERROR\n");
        return -1;
    }
    int FN = pte_to_frame_num(atm->page_table[page_num]);
    unsigned char ans = (unsigned char) RAM[(FN*4*1024 + byte_offset)];
    return ans; 
}

// Write the given `byte` at `vmem_addr` virtual address of the process
// In case of illegal memory access kill the process, deallocate all its resources(ps_exit) 
// and set error_no to ERR_SEG_FAULT.
// 
// assume 0 <= vmem_addr < PS_VIRTUAL_MEM_SIZE
void write_mem(int pid, int vmem_addr, unsigned char byte) 
{
    // TODO: student
    struct PCB* atm = (struct PCB*) (&OS_MEM[start_page_tables + 4108*pid]);
    if(atm->is_free){
        error_no = ERR_SEG_FAULT;
    }
    int page_num = (int) (vmem_addr/PAGE_SIZE);
    int byte_offset = vmem_addr%PAGE_SIZE;
    if(is_writeable(atm->page_table[page_num]) == 0 || !is_present(atm->page_table[page_num])){
        error_no = ERR_SEG_FAULT;
        exit_ps(pid);
    }
    else{
        int FN = pte_to_frame_num(atm->page_table[page_num]);
        RAM[(FN*4*1024 + byte_offset)] = byte;
    }
}



// -------------------  functions to print the state  --------------------------------------------- //

void print_page_table(int pid) 
{
    struct PCB* tmp = (struct PCB*) (&OS_MEM[start_page_tables + 4108*pid]);
    page_table_entry* page_table_start = tmp->page_table; // TODO student: start of page table of process pid
    int num_page_table_entries = 1024;           // TODO student: num of page table entries

    // Do not change anything below
    puts("------ Printing page table-------");
    for (int i = 0; i < num_page_table_entries; i++) 
    {
        page_table_entry pte = page_table_start[i];
        printf("Page num: %d, frame num: %d, R:%d, W:%d, X:%d, P%d\n", 
                i, 
                pte_to_frame_num(pte),
                is_readable(pte),
                is_writeable(pte),
                is_executable(pte),
                is_present(pte)
                );
    }

}

// For testing the code....


// #define MB (1024*1024)
// #define KB (1024)

// unsigned char code_ro_data[10*MB];

// int main(){
//     os_init();

//     code_ro_data[10*PAGE_SIZE] = 'a';
//     code_ro_data[10*PAGE_SIZE + 1] = 'b';

//     int ps1 = create_ps(10*PAGE_SIZE, 1*PAGE_SIZE, 2*PAGE_SIZE, 1*MB, code_ro_data);
//     error_no = -1;
//     print_page_table(ps1);

//     unsigned char a = read_mem(ps1, 10*PAGE_SIZE);
//     assert(a == 'a');
//     unsigned char b = read_mem(ps1, 10*PAGE_SIZE+1);
//     assert(b == 'b');

//     assert(error_no == -1);

//     write_mem(ps1, 10*PAGE_SIZE, 'b');
//     assert(error_no == ERR_SEG_FAULT);

//     int ps2 = create_ps(1*MB, 0, 0, 1*MB, code_ro_data);
//     error_no = -1;

//     int heap_begin = 1*MB;
//     allocate_pages(ps2, heap_begin, 250, O_READ | O_WRITE);
//     print_page_table(ps2);

//     write_mem(ps2, heap_begin + 1, 'a');
//     write_mem(ps2, heap_begin + 2, 'b');
//     assert(read_mem(ps2, heap_begin + 1) == 'a');
//     assert(read_mem(ps2, heap_begin + 2) == 'b');

//     deallocate_pages(ps2,heap_begin, 10);
//     print_page_table(ps2);
//     write_mem(ps2, heap_begin + 1, 'b');
//     assert(error_no == ERR_SEG_FAULT);

//     int process_pid[100];
//     int i = 0;
//     while(i < 64){
//         process_pid[i] = create_ps(1*MB, 0, 0, 1*MB, code_ro_data);
//         i++;
//     }
//     exit_ps(process_pid[0]);
//     process_pid[0] = create_ps(1*MB, 0,0,500*KB, code_ro_data);

//     print_page_table(process_pid[0]);

//     allocate_pages(process_pid[0], 1*MB, 125, O_READ | O_READ | O_EX);

//     i = 0;
//     while(i < 64){
//         print_page_table(process_pid[i]);
//         i++;
//     }
// }




