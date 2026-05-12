#ifndef MEMORY_H
#define MEMORY_H

#include "kernel.h"

enum 
{
  // Physical page facts
  PAGE_SIZE = 4096,
  PAGE_N_ENTRIES = (PAGE_SIZE / sizeof(uint32_t)),
  SECTORS_PER_PAGE = (PAGE_SIZE / SECTOR_SIZE),

  PTABLE_SPAN = (PAGE_SIZE * PAGE_N_ENTRIES),

  // Page directory/table entry bits
  PE_P = 1 << 0,                    // present bit
  PE_RW = 1 << 1,                   // read/write bit
  PE_US = 1 << 2,                   // user/supervisor bit
  PE_PWT = 1 << 3,                  // page write-through bit
  PE_PCD = 1 << 4,                  // page cache disable bit
  PE_A = 1 << 5,                    // accessed bit
  PE_D = 1 << 6,                    // dirty bit
  PE_BASE_ADDR_BITS = 12,           // position of base address
  PE_BASE_ADDR_MASK = 0xfffff000,   // extracts the base address

  // Constants to simulate a very small physical memory
  MEM_START = 0x100000,             // 1MB
  PAGEABLE_PAGES = 33,
  MAX_PHYSICAL_MEMORY = (MEM_START + PAGEABLE_PAGES * PAGE_SIZE),

  // Number of kernel page tables
  N_KERNEL_PTS = 1,
  N_PROCESS_STACK_PAGES = 2,

  PAGE_DIRECTORY_BITS = 22,          // position of page dir index
  PAGE_TABLE_BITS = 12,              // position of page table index
  PAGE_DIRECTORY_MASK = 0xffc00000,  // page directory mask
  PAGE_TABLE_MASK = 0x003ff000,      // page table mask
  PAGE_MASK = 0x00000fff,            // page offset mask
  MODE_MASK = 0x000003ff,  

  PAGE_TABLE_SIZE = (1024 * 4096 - 1)   // size of a page table in bytes
};

// Structure of an entry in the page map
typedef struct 
{
	uint32_t swap_loc;
	uint32_t vaddr;
  uint32_t swap_size;
  uint32_t *pdir;
	bool_t free;
  bool_t pinned;
} page_map_entry_t;

// Prototypes
uint32_t get_dir_idx(uint32_t vaddr);     // Use virtual address to get index in page directory
uint32_t get_tab_idx(uint32_t vaddr);     // Use virtual address to get index in a page table
uint32_t* page_addr(int i);               // Return the physical address of the i-th page
int page_alloc(int pinned);               // Allocate a page
void init_memory(void);                   // Set up kernel memory
void setup_page_table(pcb_t * p);
void page_fault_handler(void);
void page_swap_in(int pageno);            // Swap the i-th page in from disk
void page_swap_out(int pageno);           // Swap the i-th page out
int page_replacement_policy(void);        // Decide which page to replace and return the page number

#endif