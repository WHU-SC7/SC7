#ifndef __RISCV_H__
#define __RISCV_H__

#include "types.h"

// which hart (core) is this?
static inline uint64 r_mhartid()
{
    uint64 x;
    asm volatile("csrr %0, mhartid" : "=r"(x));
    return x;
}

// Machine Status Register, mstatus

#define MSTATUS_MPP_MASK (3L << 11) // previous mode.
#define MSTATUS_MPP_M (3L << 11)
#define MSTATUS_MPP_S (1L << 11)
#define MSTATUS_MPP_U (0L << 11)
#define MSTATUS_MIE (1L << 3) // machine-mode interrupt enable.

static inline uint64 r_mstatus()
{
    uint64 x;
    asm volatile("csrr %0, mstatus" : "=r"(x));
    return x;
}

static inline void w_mstatus(uint64 x)
{
    asm volatile("csrw mstatus, %0" : : "r"(x));
}

// machine exception program counter, holds the
// instruction address to which a return from
// exception will go.
static inline void w_mepc(uint64 x)
{
    asm volatile("csrw mepc, %0" : : "r"(x));
}

// Supervisor Status Register, sstatus

#define SSTATUS_SPP (1L << 8)  // Previous mode, 1=Supervisor, 0=User
#define SSTATUS_SPIE (1L << 5) // Supervisor Previous Interrupt Enable
#define SSTATUS_UPIE (1L << 4) // User Previous Interrupt Enable
#define SSTATUS_SIE (1L << 1)  // Supervisor Interrupt Enable
#define SSTATUS_UIE (1L << 0)  // User Interrupt Enable

static inline uint64 r_sstatus()
{
    uint64 x;
    asm volatile("csrr %0, sstatus" : "=r"(x));
    return x;
}

static inline void w_sstatus(uint64 x)
{
    asm volatile("csrw sstatus, %0" : : "r"(x));
}

// Supervisor Interrupt Pending
static inline uint64 r_sip()
{
    uint64 x;
    asm volatile("csrr %0, sip" : "=r"(x));
    return x;
}

static inline void w_sip(uint64 x)
{
    asm volatile("csrw sip, %0" : : "r"(x));
}

// Supervisor Interrupt Enable
#define SIE_SEIE (1L << 9) // external
#define SIE_STIE (1L << 5) // timer
#define SIE_SSIE (1L << 1) // software
static inline uint64 r_sie()
{
    uint64 x;
    asm volatile("csrr %0, sie" : "=r"(x));
    return x;
}

static inline void w_sie(uint64 x)
{
    asm volatile("csrw sie, %0" : : "r"(x));
}

// Machine-mode Interrupt Enable
#define MIE_STIE (1L << 5)  // supervisor timer
#define MIE_MEIE (1L << 11) // external
#define MIE_MTIE (1L << 7)  // timer
#define MIE_MSIE (1L << 3)  // software
static inline uint64 r_mie()
{
    uint64 x;
    asm volatile("csrr %0, mie" : "=r"(x));
    return x;
}

static inline void w_mie(uint64 x)
{
    asm volatile("csrw mie, %0" : : "r"(x));
}

// machine exception program counter, holds the
// instruction address to which a return from
// exception will go.
static inline void w_sepc(uint64 x)
{
    asm volatile("csrw sepc, %0" : : "r"(x));
}

static inline uint64 r_sepc()
{
    uint64 x;
    asm volatile("csrr %0, sepc" : "=r"(x));
    return x;
}

// Machine Exception Delegation
static inline uint64 r_medeleg()
{
    uint64 x;
    asm volatile("csrr %0, medeleg" : "=r"(x));
    return x;
}

static inline void w_medeleg(uint64 x)
{
    asm volatile("csrw medeleg, %0" : : "r"(x));
}

// Machine Interrupt Delegation
static inline uint64 r_mideleg()
{
    uint64 x;
    asm volatile("csrr %0, mideleg" : "=r"(x));
    return x;
}

static inline void w_mideleg(uint64 x)
{
    asm volatile("csrw mideleg, %0" : : "r"(x));
}

// Supervisor Trap-Vector Base Address
// low two bits are mode.
static inline void w_stvec(uint64 x)
{
    asm volatile("csrw stvec, %0" : : "r"(x));
}

static inline uint64 r_stvec()
{
    uint64 x;
    asm volatile("csrr %0, stvec" : "=r"(x));
    return x;
}

// Machine Environment Configuration Register
static inline uint64
r_menvcfg()
{
  uint64 x;
  // asm volatile("csrr %0, menvcfg" : "=r" (x) );
  asm volatile("csrr %0, 0x30a" : "=r" (x) );
  return x;
}

static inline void 
w_menvcfg(uint64 x)
{
  // asm volatile("csrw menvcfg, %0" : : "r" (x));
  asm volatile("csrw 0x30a, %0" : : "r" (x));
}

// Supervisor Timer Comparison Register
static inline uint64
r_stimecmp()
{
  uint64 x;
  // asm volatile("csrr %0, stimecmp" : "=r" (x) );
  asm volatile("csrr %0, 0x14d" : "=r" (x) );
  return x;
}

static inline void 
w_stimecmp(uint64 x)
{
  // asm volatile("csrw stimecmp, %0" : : "r" (x));
  asm volatile("csrw 0x14d, %0" : : "r" (x));
}

// Machine-mode interrupt vector
static inline void w_mtvec(uint64 x)
{
    asm volatile("csrw mtvec, %0" : : "r"(x));
}

// use riscv's sv39 page table scheme.
#define SATP_SV39 (8L << 60)

#define MAKE_SATP(pagetable) (SATP_SV39 | (((uint64)pagetable) >> 12))

// supervisor address translation and protection;
// holds the address of the page table.
static inline void w_satp(uint64 x)
{
    asm volatile("csrw satp, %0" : : "r"(x));
}

static inline uint64 r_satp()
{
    uint64 x;
    asm volatile("csrr %0, satp" : "=r"(x));
    return x;
}

// Supervisor Scratch register, for early trap handler in trampoline.S.
static inline void w_sscratch(uint64 x)
{
    asm volatile("csrw sscratch, %0" : : "r"(x));
}

static inline void w_mscratch(uint64 x)
{
    asm volatile("csrw mscratch, %0" : : "r"(x));
}

// Supervisor Trap Cause
static inline uint64 r_scause()
{
    uint64 x;
    asm volatile("csrr %0, scause" : "=r"(x));
    return x;
}

// Supervisor Trap Value
static inline uint64 r_stval()
{
    uint64 x;
    asm volatile("csrr %0, stval" : "=r"(x));
    return x;
}

// Machine-mode Counter-Enable
static inline void w_mcounteren(uint64 x)
{
    asm volatile("csrw mcounteren, %0" : : "r"(x));
}

static inline uint64 r_mcounteren()
{
    uint64 x;
    asm volatile("csrr %0, mcounteren" : "=r"(x));
    return x;
}

// machine-mode cycle counter
static inline uint64 r_time()
{
    uint64 x;
    asm volatile("csrr %0, time" : "=r"(x));
    return x;
}

static inline uint64 rdtime() {
    uint64 x;
    asm volatile("rdtime %0" : "=r"(x));
    return x;
}

// enable device interrupts
static inline void intr_on()
{
    w_sstatus(r_sstatus() | SSTATUS_SIE);
}

// disable device interrupts
static inline void intr_off()
{
    w_sstatus(r_sstatus() & ~SSTATUS_SIE);
}

// are device interrupts enabled?
static inline int intr_get()
{
    uint64 x = r_sstatus();
    return (x & SSTATUS_SIE) != 0;
}

static inline uint64 r_sp()
{
    uint64 x;
    asm volatile("mv %0, sp" : "=r"(x));
    return x;
}

// read and write tp, the thread pointer, which holds
// this core's hartid (core number), the index into cpus[].
static inline uint64 r_tp()
{
    uint64 x;
    asm volatile("mv %0, tp" : "=r"(x));
    return x;
}

static inline void w_tp(uint64 x)
{
    asm volatile("mv tp, %0" : : "r"(x));
}

static inline uint64 r_ra()
{
    uint64 x;
    asm volatile("mv %0, ra" : "=r"(x));
    return x;
}

// flush the TLB.
static inline void sfence_vma()
{
    // the zero, zero means flush all TLB entries.
    asm volatile("sfence.vma zero, zero");
}



#define PT_LEVEL 3
#if defined SBI ///<虽然不改也没有问题，但是使用sbi时不应该从0x80000000开始映射。
#define KERNEL_BASE 0x80200000ul
#else
#define KERNEL_BASE 0x80000000ul
#endif
//适配la
#define dmwin_mask (0x0)
#define dmwin_win0 (0x0)
#define dmwin_win1 (0x0)

#define PGSIZE 4096 // bytes per page
#define PAGE_NUM (250 * 1024)
#define PGSHIFT 12 // bits of offset within a page

#define PGROUNDUP(sz) (((sz) + PGSIZE - 1) & ~(PGSIZE - 1))
#define PGROUNDDOWN(a) (((a)) & ~(PGSIZE - 1))

#define ALIGN_UP(num, align) (((num) + ((align) - 1)) & ~((align) - 1))

#define PTE_V (1L << 0) // valid
#define PTE_R (1L << 1)
#define PTE_W (1L << 2)
#define PTE_X (1L << 3)
#define PTE_U (1L << 4) // 1 -> user can access
#define PTE_G (1 << 5)
#define PTE_A (1 << 6)
#define PTE_D (1 << 7)
#define PTE_TRAMPOLINE  (PTE_R | PTE_X)
#define PTE_MAPSTACK  (PTE_R | PTE_W)
#define PTE_WALK (PTE_V)
#define PTE_USER (PTE_R|PTE_W|PTE_X|PTE_U)
#define PTE_TRAPFRAME (PTE_R | PTE_W)
#define PTE_RW (PTE_W | PTE_R )
#define PTE_STACK (PTE_R | PTE_W | PTE_U)

// shift a physical address to the right place for a PTE.
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)

#define PTE2PA(pte) (((pte) >> 10) << 12)

#define PTE_FLAGS(pte) ((pte) & 0x3FF)

// extract the three 9-bit page table indices from a virtual address.
#define PXMASK 0x1FF // 9 bits
#define PXSHIFT(level) (PGSHIFT + (9 * (level)))
#define PX(level, va) ((((uint64)(va)) >> PXSHIFT(level)) & PXMASK)

// one beyond the highest possible virtual address.
// MAXVA is actually one bit less than the max allowed by
// Sv39, to avoid having to sign-extend virtual addresses
// that have the high bit set.
#define MAXVA (1L << (9 + 9 + 9 + 12 - 1))
#define MAXUVA                  0x80000000L
#define USER_MMAP_START (MAXUVA - 0x10000000 -(2 * PGSIZE))
#define USER_STACK_TOP MAXUVA - PGSIZE
#define USER_STACK_DOWN USER_MMAP_START + PGSIZE

// sigcontext结构体定义，用于保存RISC-V处理器状态
struct sigcontext {
    // 通用寄存器，与trapframe结构体对应
    uint64 ra;        // x1 (ra)
    uint64 sp;        // x2 (sp) 
    uint64 gp;        // x3 (gp)
    uint64 tp;        // x4 (tp)
    uint64 t0;        // x5 (t0)
    uint64 t1;        // x6 (t1)
    uint64 t2;        // x7 (t2)
    uint64 s0;        // x8 (s0/fp)
    uint64 s1;        // x9 (s1)
    uint64 a0;        // x10 (a0)
    uint64 a1;        // x11 (a1)
    uint64 a2;        // x12 (a2)
    uint64 a3;        // x13 (a3)
    uint64 a4;        // x14 (a4)
    uint64 a5;        // x15 (a5)
    uint64 a6;        // x16 (a6)
    uint64 a7;        // x17 (a7)
    uint64 s2;        // x18 (s2)
    uint64 s3;        // x19 (s3)
    uint64 s4;        // x20 (s4)
    uint64 s5;        // x21 (s5)
    uint64 s6;        // x22 (s6)
    uint64 s7;        // x23 (s7)
    uint64 s8;        // x24 (s8)
    uint64 s9;        // x25 (s9)
    uint64 s10;       // x26 (s10)
    uint64 s11;       // x27 (s11)
    uint64 t3;        // x28 (t3)
    uint64 t4;        // x29 (t4)
    uint64 t5;        // x30 (t5)
    uint64 t6;        // x31 (t6)
    uint64 pc;        // 程序计数器 (epc)
    uint64 pstate;    // 处理器状态
};

#endif /* __RISCV_H__ */