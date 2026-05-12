# Virtual Memory Management System

A kernel-level Virtual Memory Management System implemented as part of Project 5 of Princeton University's COS318 Operating Systems course.

This project implements a demand-paged virtual memory subsystem with:
- Two-level paging
- Page directories and page tables
- Virtual-to-physical address translation
- Page fault handling
- Swapping pages between disk and memory
- FIFO page replacement policy
- TLB invalidation
- Process-specific address spaces

---

## Features

- Implemented a two-level paging architecture similar to x86 systems
- Supports demand paging and lazy loading of pages
- Handles page faults dynamically
- Implements swapping between physical memory and disk
- FIFO-based page replacement policy
- Separate page directories for each process
- Dirty bit optimization for efficient disk writes
- Kernel and user memory separation
- TLB flushing after page table modifications

---

## Project Structure

```text
memory.h    -> Paging structures, constants, and function declarations
memory.c    -> Core virtual memory manager implementation
```

---

## Core Concepts Implemented

### 1. Two-Level Paging
- Page Directory
- Page Tables
- Virtual to Physical Address Translation

### 2. Demand Paging
Pages are loaded into memory only when accessed.

### 3. Page Fault Handling
Handles missing pages by:
- Allocating physical frames
- Swapping pages from disk
- Updating page tables
- Resuming execution

### 4. Swapping
Supports:
- Swap-in from disk
- Swap-out to disk
- Dirty page write-back optimization

### 5. Page Replacement
Implemented FIFO (First-In-First-Out) page replacement policy.


---

## Memory Layout

| Component | Description |
|---|---|
| Page Size | 4 KB |
| Paging Scheme | Two-Level Paging |
| Replacement Policy | FIFO |

---

## Important Functions

| Function | Purpose |
|---|---|
| `page_alloc()` | Allocates a physical page |
| `page_fault_handler()` | Handles page faults |
| `page_swap_in()` | Loads page from disk |
| `page_swap_out()` | Writes page to disk |
| `page_replacement_policy()` | Selects victim page |
| `setup_page_table()` | Creates process page tables |

---

## How It Works

```text
Virtual Address
       ↓
Page Directory
       ↓
Page Table
       ↓
Physical Memory
```

If a page is not present in memory:

```text
Page Fault
    ↓
Swap In From Disk
    ↓
Update Page Table
    ↓
Flush TLB
    ↓
Resume Process
```

---

## Technologies Used

- C Programming
- Operating Systems Concepts
- Paging and Virtual Memory
- Disk-backed Swapping
- QEMU Emulator

---

## How to Run

### 1. Clone the Repository

```bash
git clone <repository-link>
```

### 2. Move into the Project Directory

```bash
cd <repository-folder>
```

### 3. Clean Previous Builds

```bash
make clean
```

### 4. Build the Project

```bash
make
```

### 5. Run the Kernel Using QEMU

```bash
<qemu-command>
```

---

## Reference

Princeton University COS318 — Operating Systems  
Project 5: Virtual Memory

```
https://www.cs.princeton.edu/courses/archive/fall16/cos318/projects/project5/p5.html
```

---

## Author

Siva Rohith Reddy