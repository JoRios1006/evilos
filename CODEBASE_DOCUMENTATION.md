# EVILOS - Comprehensive Codebase Documentation

**Emacs Vi Layered Operating System**

A small, observable, and modifiable experimental x86-64 operating system kernel written in C with Lua as a high-level language component.

---

## Table of Contents

1. [Project Overview](#project-overview)
2. [Architecture Overview](#architecture-overview)
3. [Build System](#build-system)
4. [Core Components](#core-components)
5. [Memory Management](#memory-management)
6. [CPU Architecture (x86-64)](#cpu-architecture-x86-64)
7. [Drivers](#drivers)
8. [Utility Libraries](#utility-libraries)
9. [Integration Points](#integration-points)
10. [Current Implementation Status](#current-implementation-status)
11. [Development Roadmap](#development-roadmap)

---

## Project Overview

### Project Goals

Evilos is not intended to be a general-purpose operating system. Instead, it serves as a **laboratory for observable systems design**:

- **Small kernel**: Minimal, understandable implementation written in C
- **Observable**: All components expose state and allow inspection during execution
- **Modifiable**: Designed to support experimentation and incremental refinement
- **Layered**: Clear separation between hardware mechanisms (C) and high-level logic (Lua)

### Technology Stack

| Component | Technology | Notes |
|-----------|-----------|-------|
| **Platform** | x86-64 | Higher-half kernel model |
| **Bootloader** | Limine v11 | UEFI + BIOS compatible |
| **Language** | C11 + x86-64 Assembly | Freestanding environment |
| **Build System** | CMake + Ninja | LLVM/Clang toolchain |
| **Scripting** | Lua (planned) | High-level kernel logic |
| **Terminal** | Flanterm | VT100-compatible framebuffer terminal |
| **ACPI** | uACPI | ACPI table parsing and interpretation |

### Design Principles

1. **End-to-End (Smart Endpoints, Simple Transport)**
   - Intermediate layers transport data without predicting consumer semantics
   - Business logic remains at endpoints that understand data meaning

2. **Data Over Patterns**
   - Prefer explicit structures and functions over object hierarchies
   - Use tagged unions, state machines, and protocols

3. **Explicit State**
   - Components must declare their states explicitly
   - Finite State Machines (FSMs) when they clarify control flow

4. **Ring Buffers for Time-Sensitive Paths**
   - Pre-allocated circular buffers for interrupt handlers and streaming
   - No dynamic allocation in critical paths

5. **Top Half / Bottom Half Separation**
   - ISRs do minimal work: read hardware, build event, enqueue, acknowledge
   - Complex work happens outside interrupt context

---

## Architecture Overview

```
┌─────────────────────────────────────┐
│   UI / Shell / Modules (Lua)        │
├─────────────────────────────────────┤
│   Lua Runtime & Kernel Bindings     │
├─────────────────────────────────────┤
│   Memory Management & Events         │
│   ├─ kmalloc / kfree                 │
│   ├─ Event Ring Buffers              │
│   ├─ Queues & Protocols              │
│   └─ Primitives (Slice, StringView)  │
├─────────────────────────────────────┤
│   Drivers & Hardware Abstraction     │
│   ├─ UART                            │
│   ├─ Framebuffer / Flanterm          │
│   └─ Future: Timer, Keyboard, Disk   │
├─────────────────────────────────────┤
│   CPU Mechanisms (x86-64)            │
│   ├─ GDT / IDT / TSS                 │
│   ├─ Interrupt Handlers              │
│   └─ Privilege Levels & Segments     │
└─────────────────────────────────────┘
         ↓ (Limine Bootloader)
      Hardware
```

---

## Build System

### CMakeLists.txt

**File**: `CMakeLists.txt`

Orchestrates kernel compilation using modern CMake practices:

#### Key Configurations

- **Compiler**: Clang with x86-64 NONE ELF target
- **Standard**: C11 (freestanding)
- **Link Flags**:
  - `-mcmodel=kernel`: Higher-half kernel addressing
  - `--target=x86_64-unknown-none-elf`: Baremetal x86-64
  - `-fno-builtin -ffreestanding`: No libc
  - `-mno-red-zone -mno-mmx -mno-sse -mno-sse2`: Disable unsupported x86 features

#### Dependencies (FetchContent)

1. **Limine** (v11.x-binary)
   - Bootloader providing HHDM, Memory Map, Framebuffer requests
   
2. **Flanterm** (trunk)
   - VT100-compatible terminal renderer
   - Outputs to framebuffer via standard BIOS/UEFI mode
   
3. **uACPI** (master)
   - ACPI table parsing (RSDP, DSDT, etc.)
   - Kernel hooks provided via `src/uacpi_hooks/`

#### Compilation Targets

- **Kernel Object**: `kernel.elf`
  - Linked with custom linker script (`linker.ld`)
  - Includes all source files and external dependencies

- **Custom Targets**:
  - `iso`: Builds bootable ISO with Limine
  - `run`: Executes kernel in QEMU

### Linker Script (linker.ld)

**File**: `linker.ld`

Defines memory layout for the higher-half kernel:

```
Virtual Base: 0xffffffff80000000
├─ .text     → Executable (PT_LOAD FLAGS=5: R+X)
├─ .rodata   → Read-only data (PT_LOAD FLAGS=4: R)
├─ .data     → Initialized data (PT_LOAD FLAGS=6: R+W)
│  ├─ Limine .requests_start / .requests / .requests_end
│  └─ All global variables
└─ .bss      → Uninitialized data (R+W)
```

Each section is page-aligned (4 KiB boundaries).

---

## Core Components

### 1. Kernel Entry Point

**File**: `src/kernel.c`

Main kernel initialization and platform detection.

#### Responsibilities

- **Limine Protocol Handling**:
  - Declares Limine request structures in `.requests` section
  - Requests: HHDM, Framebuffer, Memory Map, Kernel Address, RSDP

- **Early Initialization**:
  - UART initialization on port 0x3F8 (standard COM1)
  - Flanterm framebuffer terminal setup
  - CPU setup: GDT, IDT
  - Dynamic memory initialization (kmalloc)

- **System State Inspection**:
  - Prints physical memory map (usable, reserved, ACPI regions)
  - Displays kernel physical/virtual base addresses
  - Reports HHDM offset for virtual address calculation

- **Testing Infrastructure**:
  - `test_kmalloc_stress()`: Validates buddy allocator and kmalloc/kfree
  - Basic alignment and coalescing tests

#### Key Functions

```c
void kprintf(const char *format, ...);
```
- Custom formatted print to both UART and Flanterm framebuffer
- Supports `%d`, `%x`, `%s`, `%v` (StringView), `%c`
- Automatically adds `\r` before `\n` for VT100 compliance

```c
void kmain(void);
```
- Main kernel function, called by Limine bootloader
- Executes initialization sequence and halts

#### Global State

- `global_ft_ctx`: Flanterm context for framebuffer output
- `vcanary`: Validation marker (0xCAFEBABE) to verify C runtime initialization
- `gi`: Iterator for memory map enumeration

---

### 2. C Standard Library (Freestanding)

**File**: `src/libk.c` + `src/string.h`

Provides essential C library functions without external dependencies.

#### String Functions

```c
size_t strlen(const char *s);
```
- Calculate null-terminated string length

```c
int vsnprintf(char *str, size_t size, const char *format, va_list args);
int snprintf(char *str, size_t size, const char *format, ...);
```
- Variable argument printf implementation
- Supports:
  - `%d`: int64 decimal
  - `%x`, `%X`: uint64 hexadecimal
  - `%s`: C string
  - `%v`: StringView (length-aware)
  - `%c`: character
  - `%%`: literal percent

#### Memory Functions

```c
void *memset(void *s, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
```
- Standard C memory operations (no external libc)
- `memmove` handles overlapping regions

#### Internal Helpers

```c
static void itoa_hex(uint64_t value, char *str);
static void itoa_dec(int64_t value, char *str);
```
- Convert integers to string representation
- Base 16 (hexadecimal) and base 10 (decimal)

---

### 3. Slice & StringView Primitives

**Files**: `src/slice.h` + `src/slice.c`

Lightweight, non-owning views over binary and text data.

#### Slice (Binary View)

```c
typedef struct {
    const uint8_t *data;
    size_t length;
} Slice;
```

Immutable view over arbitrary byte buffers. Never copies data.

#### StringView (Text View)

```c
typedef struct {
    const char *data;
    size_t length;
} StringView;
```

Immutable view over text. **Not null-terminated by default**.

#### Constructor Macros & Functions

```c
#define SV(literal) ((StringView){ .data = (literal), .length = sizeof(literal) - 1 })
```
- Compile-time constructor for string literals
- `sizeof()` includes `\0`, so subtract 1

```c
StringView sv_from_cstr(const char *str);
```
- Convert null-terminated C string to StringView at runtime

#### Core Operations

```c
bool sv_equals(StringView a, StringView b);
```
- Length-aware comparison without relying on null termination
- Returns true if contents and length are identical

```c
bool sv_starts_with(StringView sv, StringView prefix);
```
- Check if StringView begins with prefix
- Handles zero-length prefixes (always true)

```c
StringView sv_substr(StringView sv, size_t start, size_t len);
```
- Extract substring without copying
- Clamps to available data, returns zero-length on out-of-bounds start

```c
StringView sv_split_once(StringView sv, char delimiter, StringView *left);
```
- Split at first occurrence of delimiter
- Stores left part in `*left`, returns right part
- If no delimiter found, stores entire view in `*left`, returns empty view

#### Design Rationale

These primitives solve three problems:
1. **No null-termination dependency**: Work with partial data, framebuffer content, etc.
2. **No hidden copies**: View-based design allows efficient parsing
3. **Explicit length**: Safe iteration without hunting for `\0`

---

## Memory Management

### Buddy Allocator

**Files**: `src/memory/buddy_alloc.h` + `src/memory/buddy_alloc.c`

A power-of-two buddy allocator managing kernel heap.

#### Overview

The allocator divides memory into orders (power-of-two sizes):

```
Order 0 = 4 KiB (1 page)
Order 1 = 8 KiB
Order 2 = 16 KiB
...
Order N = 4 KiB * 2^N
```

#### Data Structure

- **Free List**: Separate list per order containing free blocks
- **Coalescing**: When a block is freed, adjacent buddy blocks are merged if also free
- **Splitting**: When allocating, larger free blocks are recursively split

#### Algorithm

**Allocation**:
1. Calculate required order from requested size
2. Search free list for first available block
3. If not found, search higher orders and split recursively
4. Return pointer to allocated block

**Deallocation**:
1. Determine block order from metadata
2. Free the block, mark as available
3. Check if buddy is also free → coalesce and propagate up
4. Insert into appropriate free list

#### Integration with Kernel

The buddy allocator is transparent to `kmalloc`/`kfree` users. The implementation is in a header file (single-compilation-unit pattern) to enable inline optimizations.

**Embedding**:
```c
struct buddy *kernel_buddy = buddy_embed(arena_virtual, largest_entry->length);
```

Initializes allocator within existing memory region without separate initialization page.

---

### Dynamic Memory API (kmalloc/kfree)

**Files**: `src/memory/kmalloc.h` + `src/memory/kmalloc.c`

Kernel-level malloc/free wrapping the buddy allocator.

#### Initialization

```c
void kmalloc_init(void);
```

- Scans Limine memory map for largest USABLE region
- Uses HHDM (Higher-Half Direct Map) offset to convert physical to virtual addresses
- Initializes buddy allocator within that region
- Prints arena base, size, and success status

#### Allocation

```c
void *kmalloc(size_t size);
```

- Defers to `buddy_malloc()` if initialized
- Returns NULL on allocation failure or if not initialized
- Guarantees 8-byte alignment (buddy requirement)

#### Deallocation

```c
void kfree(void *ptr);
```

- No-op if called before `kmalloc_init()`
- Defers to `buddy_free()` for actual deallocation
- Safe to pass NULL

#### Stress Testing

Kernel includes built-in stress test (`test_kmalloc_stress()`):

```c
void test_kmalloc_stress(void);
```

- Tests alignment: All allocations must be 8-byte aligned
- Tests coalescing: Freed blocks can be reused
- Tests mass allocation: 100 simultaneous 1 KiB allocations
- Verifies subsequent free operations
- Prints diagnostic messages to UART/framebuffer

---

## CPU Architecture (x86-64)

### Global Descriptor Table (GDT)

**Files**: `src/arch/x86_64/gdt.h` + `src/arch/x86_64/gdt.c` + `src/arch/x86_64/gdt_flush.S`

Defines memory segmentation for x86-64.

#### Purpose

In long mode (64-bit), segment registers are largely ignored for memory protection. However, the GDT remains essential for:
- Privilege levels (Ring 0 = kernel, Ring 3 = user)
- TSS (Task State Segment) descriptor for context switching and emergency stacks
- Syscall/Sysret fast paths (future)

#### GDT Layout

```
Index 0: Null Descriptor (required)
Index 1: Kernel Code (Ring 0, executable)
Index 2: Kernel Data (Ring 0, readable/writable)
Index 3: User Data (Ring 3, readable/writable)
Index 4: User Code (Ring 3, executable)
Index 5-6: TSS Descriptor (occupies 2 entries in 64-bit)
```

#### Structures

```c
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;           // Privilege, type bits
    uint8_t  granularity;      // Size, limit upper bits
    uint8_t  base_high;
} __attribute__((packed));

struct tss_entry {                  // 64-bit TSS descriptor (16 bytes)
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;        // Bits 32-63 of TSS base
    uint32_t reserved;
} __attribute__((packed));

struct tss_64 {
    uint32_t reserved0;
    uint64_t rsp0;              // Kernel stack pointer for Ring 0
    uint64_t rsp1;              // Kernel stack pointer for Ring 1
    uint64_t rsp2;              // Kernel stack pointer for Ring 2
    uint64_t reserved1;
    uint64_t ist[7];            // Interrupt Stack Table (emergency stacks)
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;       // I/O permission bitmap offset
} __attribute__((packed));
```

#### Initialization

```c
int gdt_init(void);
```

1. **Sets up descriptors** for kernel code/data and user code/data
2. **Configures TSS**:
   - Allocates 16 KiB emergency stack for double faults
   - Stores stack pointer in TSS `ist[0]`
   - Splits TSS descriptor across two GDT entries (64-bit requirement)
3. **Loads GDT** via `lgdt` instruction (implemented in `gdt_flush.S`)
4. **Loads TSS** via `ltr` instruction with selector 0x28 (GDT entry 5 * 8 + 0)

#### Emergency Stack

```c
static uint8_t double_fault_stack[16384] __attribute__((aligned(16)));
```

Dedicated stack for double faults (occurs when handling an exception itself fails). Prevents triple faults and system reboot.

---

### Interrupt Descriptor Table (IDT)

**Files**: `src/arch/x86_64/idt.h` + `src/arch/x86_64/idt.c` + `src/arch/x86_64/isr_stubs.S`

Maps CPU exceptions and interrupts to handler routines.

#### IDT Entry (16 bytes)

```c
struct idt_entry {
    uint16_t isr_low;           // Handler address bits 0-15
    uint16_t kernel_cs;         // Kernel code segment (0x08)
    uint8_t  ist;               // Interrupt Stack Table index (0-7)
    uint8_t  attributes;        // Present, DPL, type
    uint16_t isr_mid;           // Handler address bits 16-31
    uint32_t isr_high;          // Handler address bits 32-63
    uint32_t reserved;
} __attribute__((packed));
```

#### IDTR (Interrupt Descriptor Table Register)

```c
struct idtr {
    uint16_t limit;             // Size - 1 of IDT
    uint64_t base;              // Virtual address of IDT array
} __attribute__((packed));
```

#### Exception Handling

The kernel handles the 32 standard x86-64 exceptions:

```
0  = Divide Error (#DE)
1  = Debug (#DB)
2  = Non-Maskable Interrupt (NMI)
3  = Breakpoint (#BP)
4  = Overflow (#OF)
5  = BOUND Range Exceeded (#BR)
6  = Invalid Opcode (#UD)
7  = Device Not Available (#NM)
8  = Double Fault (#DF) -- uses IST slot 1
9  = Coprocessor Segment Overrun
10 = Invalid TSS (#TS)
11 = Segment Not Present (#NP)
12 = Stack-Segment Fault (#SS)
13 = General Protection Fault (#GP)
14 = Page Fault (#PF)
15 = Reserved
16 = x87 Floating-Point Error (#MF)
17 = Alignment Check (#AC)
18 = Machine Check (#MC)
19 = SIMD Floating-Point Exception (#XF)
20 = Virtualization Exception (#VE)
21 = Control Protection Exception (#CP)
22-27 = Reserved
28 = Hypervisor Injection Exception (#HV)
29 = VMM Communication Exception (#VC)
30 = Security Exception (#SX)
31 = Reserved
```

#### Interrupt Frame

When an exception occurs, the CPU automatically pushes context:

```c
struct interrupt_frame {
    // Pushed by ISR stubs
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_number, error_code;           // Custom, added by stubs
    
    // Automatically pushed by CPU on exception
    uint64_t rip, cs, rflags, rsp, ss;
} __attribute__((packed));
```

#### Initialization

```c
int idt_init(void);
```

1. **Registers all 32 ISR stubs** (implemented in assembly)
2. **Assigns IST slots**:
   - Most exceptions use IST 0 (normal kernel stack)
   - Double Fault (#8) uses IST 1 (emergency stack)
3. **Loads IDT** via `lidt` instruction

#### ISR Stubs (Assembly)

**File**: `src/arch/x86_64/isr_stubs.S`

Each exception vector requires a stub that:
1. Pushes the exception number
2. For exceptions with error code, the CPU already pushed it; for others, push 0
3. Saves all general-purpose registers
4. Calls `isr_handler()` in C
5. Restores context and executes `iretq`

The stubs are generated via macro expansion to minimize code duplication.

---

### Exception Handler

**File**: `src/arch/x86_64/isr.c`

```c
void isr_handler(struct interrupt_frame *frame);
```

Unified exception handler that:
1. **Decodes exception**: Translates vector number to human-readable name
2. **Dumps diagnostics**:
   - Exception type and vector number
   - Error code (if applicable)
   - Critical registers: RIP, RSP, RAX, RBX, RCX, RDX
3. **Halts**: Executes `cli; hlt` in infinite loop

#### Design Notes

- **No dynamic allocation**: Panic path uses only stack and UART
- **Minimal dependencies**: Calls only `uart_puts()` and `uart_print_hex()`
- **VT100 output**: Panic screen printed to framebuffer via `kprintf()`

---

## Drivers

### UART Serial Driver

**Files**: `src/drivers/uart.h` + `src/drivers/uart.c`

Interface to x86-64 UART (8250-compatible serial port).

#### Hardware Details

UART registers are I/O-port mapped at base address + offset:

| Offset | Name | Purpose |
|--------|------|---------|
| +0 | DATA | Transmit/Receive buffer |
| +1 | IER | Interrupt Enable Register |
| +2 | FCR | FIFO Control Register |
| +3 | LCR | Line Control Register |
| +4 | MCR | Modem Control Register |
| +5 | LSR | Line Status Register |

#### Initialization

```c
int uart_init(uint16_t port);
```

Standard 8250 initialization sequence:
1. Disable interrupts (IER = 0x00)
2. Enable DLAB (Divisor Latch Access Bit)
3. Set divisor for baud rate: 115200 baud ⟹ divisor 1 (0x01)
4. Disable DLAB, set 8-bit word, 1 stop bit (LCR = 0x03)
5. Configure FIFO (FCR = 0xC7): enable, clear TX/RX, 14-byte threshold
6. Configure modem lines (MCR = 0x0B): RTS+DTR set

#### I/O Primitives

```c
static void outb(uint16_t port, uint8_t val);
static uint8_t inb(uint16_t port);
```

- Inline assembly for x86 port I/O operations
- `outb`: Write byte to I/O port
- `inb`: Read byte from I/O port

#### Output Functions

```c
void uart_putc(char c);
void uart_puts(const char *str);
void uart_print_hex(uint64_t value);
```

- **putc**: Wait for TX FIFO ready (LSR & 0x20), write character
- **puts**: Loop over null-terminated string, output each character
- **print_hex**: Convert 64-bit value to 16-digit hex with "0x" prefix

#### Usage

Primary use is kernel diagnostics:
- Boot messages
- Memory map enumeration
- Panic context dumps
- Test result reporting

Output can be captured via QEMU's `-serial stdio` option.

---

## Utility Libraries & Integration Points

### uACPI Integration

**File**: `src/uacpi_hooks/kernel_api.c` + `src/uacpi_hooks/uacpi_libc.h`

Bridges between the generic uACPI ACPI parser and Evilos kernel services.

#### Dynamic Memory Hooks

```c
void *uacpi_kernel_alloc(uacpi_size size);
void uacpi_kernel_free(void *mem);
```

Delegates to `kmalloc()`/`kfree()` for all uACPI allocations.

#### RSDP Discovery

```c
uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address);
```

Converts Limine's virtual RSDP pointer to physical address by subtracting HHDM offset:

```c
uintptr_t virtual_rsdp = (uintptr_t)rsdp_req.response->address;
*out_rsdp_address = virtual_rsdp - hhdm_req.response->offset;
```

#### Virtual Address Mapping

```c
void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len);
void uacpi_kernel_unmap(void *addr, uacpi_size len);
```

Trivial implementation using HHDM:
- **Map**: Return `addr + HHDM_offset`
- **Unmap**: No-op (HHDM statically covers all physical memory)

#### Logging

```c
void uacpi_kernel_log(uacpi_log_level level, const uacpi_char *str);
```

Redirects uACPI debug output to `kprintf()`, which prints to both UART and framebuffer.

#### Timing Stubs (Not Implemented)

```c
uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void);
void uacpi_kernel_stall(uacpi_u8 usec);
```

Currently return 0 / no-op. TODO: Implement once timer hardware is available.

#### C Library Mapping

**File**: `src/uacpi_hooks/uacpi_libc.h`

uACPI expects standard C library functions. Map them to Evilos equivalents:

```c
#define uacpi_memcpy   memcpy
#define uacpi_memset   memset
#define uacpi_memcmp   memcmp
#define uacpi_strlen   strlen
#define uacpi_snprintf snprintf
#define uacpi_strcmp   evilos_strcmp  // Custom implementation
```

This avoids pulling in a full libc and ensures consistent behavior with freestanding C.

---

## Current Implementation Status

### Completed

| Component | Status | Notes |
|-----------|--------|-------|
| **Boot** | ✅ | Limine v11, HHDM, Memory Map, Framebuffer requests working |
| **Architecture** | ✅ | x86-64 entry, C runtime (globals/BSS initialization) |
| **GDT** | ✅ | Ring 0/3, kernel/user segments, TSS with double-fault stack |
| **IDT** | ✅ | All 32 exception vectors wired to unified handler |
| **ISR** | ✅ | Panic diagnostics with register dump and exception name |
| **UART** | ✅ | Serial output on COM1 (0x3F8) at 115200 baud |
| **Framebuffer** | ✅ | Flanterm VT100 terminal with graphics output |
| **Memory Map** | ✅ | Discovery and display of physical memory regions |
| **Buddy Allocator** | ✅ | Power-of-two allocation, splitting, coalescing |
| **kmalloc/kfree** | ✅ | Dynamic kernel memory with alignment guarantees |
| **StringView** | ✅ | Length-aware string views without null-termination dependency |
| **Slice** | ✅ | Generic binary views for memory regions |
| **ACPI RSDP** | ✅ | Discovery and virtual-to-physical address conversion |
| **uACPI Hooks** | ✅ | Integration layer for ACPI parsing |

### Partially Implemented

| Component | Status | Notes |
|-----------|--------|-------|
| **Stress Testing** | 🔶 | Basic kmalloc tests; comprehensive suite needed |
| **Exception Handling** | 🔶 | Diagnostics only; no recovery mechanisms |

### Not Yet Implemented

| Component | Status | Notes |
|-----------|--------|-------|
| **Virtual Memory** | ❌ | No paging, no Areas, no VMM |
| **Interrupts** | ❌ | No PIC/APIC initialization, no IRQ routing |
| **Timer** | ❌ | No PIT/HPET, no time awareness |
| **Keyboard/Input** | ❌ | No PS/2 controller, no event queues |
| **Event System** | ❌ | No ring buffers, no inter-subsystem messaging |
| **Lua Runtime** | ❌ | No embedded Lua interpreter |
| **Processes/Tasks** | ❌ | No scheduler, no context switching |
| **Filesystem** | ❌ | No VFS, no disk drivers |
| **IPC** | ❌ | No message passing, pipes, sockets |

---

## Development Roadmap

### Phase 1: Reliability of Existing Components (Immediate)

**Focus**: Make current subsystems production-grade.

#### 1.1 Buddy Allocator / kmalloc

- [ ] Comprehensive stress tests (simultaneous, out-of-order free)
- [ ] Fragmentation measurement
- [ ] Block splitting/coalescing verification
- [ ] Ownership contract documentation
- [ ] Double-free detection (if desired)

#### 1.2 Exception Handling

- [ ] Test each exception type deliberately
  - #DE (divide by zero): `asm("mov $0, %rax; cqo; idiv %rax;");`
  - #UD (invalid opcode): `asm(".byte 0x0F, 0xFF;");`
  - #GP (general protection): Invalid segment access
  - #PF (page fault): Access unmapped memory
- [ ] Verify double-fault path
- [ ] CR2 capture for page faults
- [ ] Unified register dump format

#### 1.3 StringView / Slice Invariants

- [ ] Comprehensive unit tests (host-side first)
- [ ] Documentation of null-termination non-requirement
- [ ] Ownership semantics clarification
- [ ] Performance benchmarks

### Phase 2: Event Infrastructure

**Focus**: Build messaging backbone for hardware-to-consumer communication.

#### 2.1 Event Types & Tagged Unions

- [ ] Define `EventType` enum
- [ ] Create typed events: keyboard, timer, hardware, syscall, page-fault
- [ ] Use tagged unions to avoid "God Event" structure

#### 2.2 Ring Buffer (SPSC/MPSC)

- [ ] Pre-allocated circular buffer
- [ ] Producer/consumer separation
- [ ] Overflow behavior definition
- [ ] Host-side tests + kernel self-tests

#### 2.3 ISR ↔ Bottom-Half Integration

- [ ] Top-half: minimal work, event enqueue, hardware ack
- [ ] Bottom-half: work queue outside interrupt context
- [ ] Ensure Lua never executes from ISR

### Phase 3: Hardware Integration

**Focus**: Expand hardware support beyond serial and display.

#### 3.1 Interrupts (PIC/APIC)

- [ ] Initialize 8259 Programmable Interrupt Controller (BIOS mode)
- [ ] Or APIC + I/O APIC (modern systems)
- [ ] Route IRQs to IDT vectors
- [ ] Count interrupts per IRQ

#### 3.2 Timer

- [ ] PIT (Intel 8254): Simple, available on all x86
- [ ] Or HPET: Higher resolution
- [ ] Periodic tick (e.g., 1 ms)
- [ ] Boot time tracking

#### 3.3 Keyboard

- [ ] PS/2 controller initialization
- [ ] Scan code to ASCII conversion
- [ ] Key press/release events to ring buffer

### Phase 4: Virtual Memory

**Focus**: Separate physical and virtual address spaces.

#### 4.1 Page Tables

- [ ] Create/destroy page tables
- [ ] Map single pages
- [ ] Change permissions (R, W, X)
- [ ] Unmap and reclaim

#### 4.2 Areas (Virtual Regions)

- [ ] Define Area structure: range, permissions, backing, commit policy
- [ ] Area tree/interval tree for lookup
- [ ] Anonymous, file-backed, shared, MMIO backing types
- [ ] Separate backing from commit policy (don't multiply enums)

#### 4.3 Page Fault Handling

- [ ] On #PF, find Area, determine backing type
- [ ] Anonymous: allocate new page
- [ ] File-backed: load from VFS
- [ ] Shared: obtain physical page
- [ ] MMIO: direct mapping
- [ ] Map and resume

### Phase 5: Lua Integration (High-Level Scripting)

**Focus**: Bridge kernel mechanisms to high-level policy.

- [ ] Embed Lua interpreter in kernel
- [ ] Expose minimal C API for kernel functions
- [ ] Implement ring buffer operations in Lua
- [ ] Execute event consumers as Lua coroutines (not from ISR)
- [ ] Shell / REPL for runtime inspection

### Phase 6: Multi-Tasking (Process Model)

**Focus**: Concurrent task execution.

- [ ] Task structure (TCB: task control block)
- [ ] Scheduler (round-robin first, then preemptive)
- [ ] Context switch
- [ ] Syscall entry point
- [ ] Signal handling (basic)

### Phase 7: IPC & Synchronization

**Focus**: Inter-process communication and coordination.

- [ ] Mutex / spinlock primitives
- [ ] Message passing queues
- [ ] Pipe abstraction
- [ ] Shared memory regions

### Phase 8: Filesystem & Disk

**Focus**: Persistent storage.

- [ ] ATA/SATA driver (or VIRTIO for QEMU)
- [ ] Minimal filesystem (ramfs first, then ext2)
- [ ] VFS abstraction
- [ ] File descriptor table per process

---

## File Structure Summary

```
evilos/
├── CMakeLists.txt           # Build configuration
├── linker.ld                # Kernel memory layout
├── limine.conf              # Bootloader config
├── LICENSE                  # GPL v2.0
├── README.md                # Project overview & architecture
├── TODO.md                  # Development roadmap
├── TOOLS.md                 # Toolchain specification
├── CODEBASE_DOCUMENTATION.md # This file
│
└── src/
    ├── kernel.c             # Main entry point, initialization
    ├── libk.c               # C standard library (snprintf, memcpy, etc.)
    ├── string.h             # libk declarations
    ├── slice.h              # StringView / Slice primitives
    ├── slice.c              # StringView / Slice implementations
    ├── limine.h             # Limine bootloader protocol (external)
    ├── flanterm.h           # Flanterm terminal library (external)
    │
    ├── arch/x86_64/
    │   ├── gdt.h            # Global Descriptor Table definitions
    │   ├── gdt.c            # GDT initialization
    │   ├── gdt_flush.S      # Assembly: lgdt instruction
    │   ├── idt.h            # Interrupt Descriptor Table definitions
    │   ├── idt.c            # IDT initialization
    │   ├── isr.c            # Exception handler (unified)
    │   └── isr_stubs.S      # Exception stubs (asm)
    │
    ├── drivers/
    │   ├── uart.h           # UART serial port interface
    │   └── uart.c           # UART implementation
    │
    ├── memory/
    │   ├── buddy_alloc.h    # Buddy allocator (implementation header)
    │   ├── buddy_alloc.c    # Buddy allocator (single compilation)
    │   ├── kmalloc.h        # Kernel malloc/free API
    │   └── kmalloc.c        # kmalloc initialization & wrapper
    │
    └── uacpi_hooks/
        ├── kernel_api.c     # uACPI integration layer
        └── uacpi_libc.h     # C library mapping for uACPI
```

---

## Compilation & Execution

### Building the Kernel

```bash
mkdir build && cd build
cmake -G Ninja ..
ninja
```

Produces `kernel.elf` in the build directory.

### Creating Bootable ISO

```bash
ninja iso
```

Creates `kernel.iso` with Limine bootloader embedded.

### Running in QEMU

```bash
ninja run
```

Launches QEMU with:
- 512 MB RAM
- Emulated framebuffer (qemu-vga)
- Serial port connected to stdio
- ISO loaded as CD-ROM

### Serial Output Capture

QEMU forwards the serial port to stdout:

```
[OK] UART DEVICE INITIALIZED
[OK] GDT INITIALIZED
[OK] IDT INITIALIZED
[OK] kmalloc inicializado en 0xffff880000100000 (Tam: 503 MB)
--- MAPA DE MEMORIA FISICA ---
Base: 0x0 | Size: 0x9d000 | Tipo: USABLE
Base: 0x9d000 | Size: 0x63000 | Tipo: RESERVED
...
[OK] Pruebas de estres de memoria superadas.
[KERNEL] Halt.
```

---

## Design Philosophy Recap

### What Makes Evilos Different

1. **Explicit Over Implicit**
   - No hidden object models or inheritance chains
   - Clear data structures and function calls

2. **Observable**
   - All state can be inspected
   - Comprehensive logging and diagnostics
   - UART and framebuffer feedback

3. **Testable**
   - Unit tests on host before kernel integration
   - Stress tests validate assumptions
   - Incremental verification at each layer

4. **Modular**
   - Clear interfaces between subsystems
   - Minimal coupling
   - Easy to replace implementations

5. **Layered**
   - Hardware mechanisms in C
   - High-level policy in Lua
   - Clean separation of concerns

### Further Reading

- **README.md**: Architectural RFC and design principles
- **TODO.md**: Detailed roadmap with specific tasks
- **TOOLS.md**: Build environment requirements

---

## Contributing & Future Work

This codebase is actively developed. Key areas for contribution:

1. **Exception testing**: Verify each CPU exception path
2. **Stress testing**: Push memory allocator to limits
3. **Documentation**: Expand protocol specifications
4. **Optimization**: Profile and measure performance
5. **New subsystems**: Event ring buffers, interrupt controller, timer driver

All changes should maintain the principles of **clarity, observability, and testability**.

---

**Generated**: 2026-10-05  
**Platform**: x86-64 (Higher-Half Kernel)  
**License**: GPL v2.0
