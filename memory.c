#include "common.h"
#include "kernel.h"
#include "scheduler.h"
#include "memory.h"
#include "thread.h"
#include "util.h"
#include "interrupt.h"
#include "tlb.h"
#include "usb/scsi.h"

// ========================== Static global variables ==========================

// Keep track of all pages: their vaddr, status, and other properties
static page_map_entry_t page_map[PAGEABLE_PAGES];

// Address of the kernel page directory (shared by all kernel threads)
static uint32_t *kernel_pdir;

// Kernel page tables
static uint32_t *kernel_ptabs[N_KERNEL_PTS];

// Lock to ensure mutual exclusion
static lock_t lock;

// Queue for FIFO page replacement policy
struct queue 
{
    int pages[PAGEABLE_PAGES];
    int head;
    int tail;
    int size;
};

// Queue of pages in FIFO order to be replaced
static struct queue q;

// =============================================================================

// Main API

// Use virtual address to get index in page directory
uint32_t get_dir_idx(uint32_t vaddr)
{
    return (vaddr & PAGE_DIRECTORY_MASK) >> PAGE_DIRECTORY_BITS;
}

// Use virtual address to get index in a page table
uint32_t get_tab_idx(uint32_t vaddr)
{
    return (vaddr & PAGE_TABLE_MASK) >> PAGE_TABLE_BITS;
}

// Returns physical address of page number i
uint32_t* page_addr(int i)
{
    return (uint32_t*)(MEM_START + i * PAGE_SIZE);
}

// Set flags in a page table entry to 'mode'
void set_ptab_entry_flags(uint32_t * pdir, uint32_t vaddr, uint32_t mode)
{
    uint32_t dir_idx = get_dir_idx((uint32_t) vaddr);
    uint32_t tab_idx = get_tab_idx((uint32_t) vaddr);
    uint32_t dir_entry;
    uint32_t *tab;
    uint32_t entry;

    dir_entry = pdir[dir_idx];
    ASSERT(dir_entry & PE_P);       // dir entry present
    tab = (uint32_t *) (dir_entry & PE_BASE_ADDR_MASK);

    entry = tab[tab_idx] & PE_BASE_ADDR_MASK;
    entry |= mode & ~PE_BASE_ADDR_MASK;
    tab[tab_idx] = entry;

    // Flush TLB because we just changed a page table entry in memory
    flush_tlb_entry(vaddr);
}

// Initialize a page table entry
void init_ptab_entry(uint32_t * table, uint32_t vaddr,uint32_t paddr, uint32_t mode)
{
    int index = get_tab_idx(vaddr);
    table[index] = (paddr & PE_BASE_ADDR_MASK) | (mode & ~PE_BASE_ADDR_MASK);
    flush_tlb_entry(vaddr);
}

// Insert a page table entry into the page directory
void insert_ptab_dir(uint32_t * dir, uint32_t *tab, uint32_t vaddr, uint32_t mode)
{
    uint32_t access = mode & MODE_MASK;
    int idx = get_dir_idx(vaddr);
    dir[idx] = ((uint32_t)tab & PE_BASE_ADDR_MASK) | access;
}

// Allocate a page and return page index in the page_map directory
int page_alloc(int pinned)
{
    for(int i = 0; i < PAGEABLE_PAGES; i++)
    {
        // find a free page_map entry to allocate
        if(page_map[i].free)
        {
            page_map[i].pinned = pinned;
            page_map[i].free = FALSE;

            // zero out the memory at physical addr 
            bzero((char*)page_addr(i), PAGE_SIZE);

            // page replacement policy queue
            if(!pinned)
            {
                q.pages[q.head] = i;
                q.size++;
                q.head = (q.head + 1) % PAGEABLE_PAGES;
            }

            return i;
        }
    }

    // nothing is available in page_map so swap out
    int page_swap_index = page_replacement_policy();
    page_swap_out(page_swap_index);

    page_map[page_swap_index].pinned = pinned;
    page_map[page_swap_index].free = FALSE;

    // zero out here too at physical addr
    bzero((char*)page_addr(page_swap_index), PAGE_SIZE);

    // page replacement policy queue
    if(!pinned)
    {
        q.pages[q.head] = page_swap_index;
        q.size++;
        q.head = (q.head + 1) % PAGEABLE_PAGES;
    }

    return page_swap_index;
}

// Set up kernel memory for kernel threads to run
void init_memory(void)
{
    // inititalize lock
    lock_init(&lock);
    // initialize queue
    q.head = 0;
    q.tail = 0;
    q.size = 0;

    // do for all page_maps
    for(int i = 0; i < PAGEABLE_PAGES; i++)
    {
        page_map[i].free = TRUE;
        page_map[i].pinned = FALSE;
    }

    // allocate a page
    int page_dir_index = page_alloc(TRUE);
    // then physical memory address is pdir
    kernel_pdir = page_addr(page_dir_index);
    page_map[page_dir_index].vaddr = (uint32_t)kernel_pdir; 
    page_map[page_dir_index].pdir = kernel_pdir;

    // setup kernel page tables
    for(int i = 0; i < N_KERNEL_PTS; i++)
    {
        // allocate a page
        int page_table_index = page_alloc(TRUE);
        // kernel_ptabs[i] = physical addr
        kernel_ptabs[i] = page_addr(page_table_index);

        // set up entry in pdir for each table
        uint32_t mode = 0;
        mode |= PE_P;
        // identity map for physical and virtual for kernel
        insert_ptab_dir(kernel_pdir, kernel_ptabs[i], (uint32_t)kernel_ptabs[i], mode);

        page_map[page_table_index].vaddr = (uint32_t)kernel_ptabs[i];
        page_map[page_table_index].pdir = kernel_pdir;
    }

    // map in all of 0-MAX PHYSICAL MEMORY
    for(uint32_t temp_mem = 0; temp_mem < MAX_PHYSICAL_MEMORY; temp_mem += PAGE_SIZE)
    {
        uint32_t mode = 0;
        mode |= PE_P;
        init_ptab_entry(kernel_ptabs[0], temp_mem, temp_mem, mode);
    }

    // screen address page
    uint32_t mode = 0;
    mode |= PE_P | PE_RW | PE_US;
    set_ptab_entry_flags(kernel_pdir, SCREEN_ADDR, mode);
}

// Set up a page directory and page table for a new process
void setup_page_table(pcb_t * p)
{
    lock_acquire(&lock);

    // Kernel thread : share kernel space
    if(p->is_thread)
    {
        p->page_directory = kernel_pdir;
    }
    else
    {
        uint32_t mode = PE_P | PE_RW | PE_US;
        int page_index;

        // allocate page directory
        page_index = page_alloc(TRUE);
        p->page_directory = page_addr(page_index);

        page_map[page_index].vaddr = (uint32_t)p->page_directory;
        page_map[page_index].pdir = p->page_directory;

        // create code/data page table
        page_index = page_alloc(TRUE);
        uint32_t* code_ptab = page_addr(page_index);

        insert_ptab_dir(p->page_directory, code_ptab, PROCESS_START, mode);

        page_map[page_index].vaddr = PROCESS_START;
        page_map[page_index].pdir = p->page_directory;

        // create stack page table
        page_index = page_alloc(TRUE);
        uint32_t* stack_ptab = page_addr(page_index);

        insert_ptab_dir(p->page_directory, stack_ptab, PROCESS_STACK, mode);

        page_map[page_index].vaddr = PROCESS_STACK;
        page_map[page_index].pdir = p->page_directory;

        // allocate 1 code/data page
        page_index = page_alloc(FALSE);         // not pinned (can be swapped)

        init_ptab_entry(code_ptab,PROCESS_START,(uint32_t)page_addr(page_index),mode);

        page_map[page_index].vaddr = PROCESS_START;
        page_map[page_index].pdir = p->page_directory;


        // allocate stack pages
        for(int i = 0; i < N_PROCESS_STACK_PAGES; i++)
        {
            page_index = page_alloc(TRUE);      // stack pinned

            uint32_t vaddr = PROCESS_STACK - (PAGE_SIZE * i);

            init_ptab_entry(stack_ptab,vaddr,(uint32_t)page_addr(page_index),mode);

            page_map[page_index].vaddr = vaddr;
            page_map[page_index].pdir = p->page_directory;
        }

        // map kernel memory
        insert_ptab_dir(p->page_directory,kernel_ptabs[0],(uint32_t)kernel_ptabs[0],mode);
    }

    lock_release(&lock);
}

// Swap into a free page upon a page fault
void page_fault_handler(void)
{
    lock_acquire(&lock);
    // increment page fault count
    current_running->page_fault_count++;

    uint32_t* page_dir = current_running->page_directory;
    uint32_t vaddr = current_running->fault_addr;

    // get directory index from fault addr
    uint32_t dir_idx = get_dir_idx((uint32_t) vaddr);
    uint32_t dir_entry = page_dir[dir_idx];
    ASSERT(dir_entry & PE_P);       // dir entry present
    uint32_t *tab = (uint32_t *) (dir_entry & PE_BASE_ADDR_MASK);

    // allocate a page
    int page_index = page_alloc(FALSE);

    // set page map entry
    page_map[page_index].vaddr = vaddr;
    page_map[page_index].swap_loc = current_running->swap_loc;
    page_map[page_index].swap_size = current_running->swap_size;
    page_map[page_index].pdir = page_dir;

    // load contents from disk
    page_swap_in(page_index);

    // update page table
    uint32_t mode = 0;
    mode |= PE_P | PE_RW | PE_US;
    init_ptab_entry(tab, vaddr, (uint32_t)page_addr(page_index), mode);
    lock_release(&lock);
}

// Get the sector number on disk of a process image
int get_disk_sector(page_map_entry_t * page)
{
    return page->swap_loc + ((page->vaddr - PROCESS_START) / PAGE_SIZE) * SECTORS_PER_PAGE;
}

// Swap i-th page in from disk
void page_swap_in(int i)
{
    // min(swap_size + swap_loc - get_disk_sector, 8)
    int block_count = SECTORS_PER_PAGE;
    int disk_sector = get_disk_sector(&page_map[i]);
    int sectors = current_running->swap_size + current_running->swap_loc - disk_sector;
    if(sectors < block_count)
    {
        block_count = sectors;
    }
    // read page from disk into respective page at physical memory 
    int check  = scsi_read(disk_sector, block_count, (char *) page_addr(i));
    ASSERT(check == 0);
}

// Swap i-th page out to disk
void page_swap_out(int i)
{
    // find in table
    uint32_t vaddr = page_map[i].vaddr;
    uint32_t* page_dir = page_map[i].pdir;

    // get directory index and entry
    uint32_t dir_idx = get_dir_idx((uint32_t) vaddr);
    uint32_t dir_entry = page_dir[dir_idx];
    ASSERT(dir_entry & PE_P); // dir entry present

    // get table index and table entry
    uint32_t *tab = (uint32_t *) (dir_entry & PE_BASE_ADDR_MASK);
    uint32_t tab_idx = get_tab_idx((uint32_t) vaddr);
    uint32_t entry = tab[tab_idx];

    // set to not present and not dirty
    uint32_t mode = entry & MODE_MASK; 
    mode &= (~PE_P);
    set_ptab_entry_flags(page_dir, vaddr, mode);

    // only write back if dirty
    if(entry & MODE_MASK & PE_D)
    {
        // min(swap_size + swap_loc - get_disk_sector, 8)
        int block_count = SECTORS_PER_PAGE;
        int disk_sector = get_disk_sector(&page_map[i]);
        int sectors = page_map[i].swap_size + page_map[i].swap_loc - disk_sector;
        if(sectors < block_count)
        {
            block_count = sectors;
        }
        int check = scsi_write(disk_sector, block_count, (char *) page_addr(i));
        ASSERT(check == 0);
    }
}

// Decide which page to replace, return the page number 
int page_replacement_policy(void)
{
    ASSERT(q.size > 0);
    int page_replace_index = q.pages[q.tail];
    // update queue
    q.size--;
    q.tail = (q.tail + 1) % PAGEABLE_PAGES;
    
    return page_replace_index;
}