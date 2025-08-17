#ifndef PERSONALITY_H
#define PERSONALITY_H

/* Personality types */
#define PER_LINUX           0x0000
#define PER_LINUX_32BIT     0x0000 | ADDR_LIMIT_32BIT
#define PER_SVR4            0x0001 | STICKY_TIMEOUTS | MMAP_PAGE_ZERO
#define PER_SVR3            0x0002 | STICKY_TIMEOUTS | SHORT_INODE
#define PER_SCOSVR3         0x0003 | STICKY_TIMEOUTS | WHOLE_SECONDS | SHORT_INODE
#define PER_OSR5            0x0003 | STICKY_TIMEOUTS | WHOLE_SECONDS
#define PER_WYSEV386        0x0004 | STICKY_TIMEOUTS | SHORT_INODE
#define PER_ISCR4           0x0005 | STICKY_TIMEOUTS
#define PER_BSD             0x0006
#define PER_SUNOS           0x0006 | STICKY_TIMEOUTS
#define PER_XENIX           0x0007 | STICKY_TIMEOUTS | SHORT_INODE
#define PER_LINUX32         0x0008
#define PER_LINUX32_3GB     0x0008 | ADDR_LIMIT_3GB
#define PER_IRIX32          0x0009 | STICKY_TIMEOUTS
#define PER_IRIXN32         0x000a | STICKY_TIMEOUTS
#define PER_IRIX64          0x000b | STICKY_TIMEOUTS
#define PER_RISCOS          0x000c
#define PER_SOLARIS         0x000d | STICKY_TIMEOUTS
#define PER_UW7             0x000e | STICKY_TIMEOUTS | MMAP_PAGE_ZERO
#define PER_OSF4            0x000f
#define PER_HPUX            0x0010

/* Personality flags */
#define UNAME26             0x0020000  /* Return 2.6.x version for uname */
#define ADDR_NO_RANDOMIZE   0x0040000  /* Disable address space randomization */
#define FDPIC_FUNCPTRS      0x0080000  /* Use function descriptors for function pointers */
#define MMAP_PAGE_ZERO      0x0100000  /* Map page 0 as read-only */
#define ADDR_COMPAT_LAYOUT  0x0200000  /* Use compatibility layout */
#define READ_IMPLIES_EXEC   0x0400000  /* Make READ imply EXEC */
#define ADDR_LIMIT_32BIT    0x0800000  /* 32-bit address space limit */
#define SHORT_INODE         0x1000000  /* Return short inode numbers */
#define WHOLE_SECONDS       0x2000000  /* Round times to whole seconds */
#define STICKY_TIMEOUTS     0x4000000  /* Timeouts stick across exec */
#define ADDR_LIMIT_3GB      0x8000000  /* 3GB address space limit */

/* Personality mask */
#define PER_MASK            0x00ff

/* Get personality value */
#define personality(pers)   ((pers) & PER_MASK)

#endif /* PERSONALITY_H */
