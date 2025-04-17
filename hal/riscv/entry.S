	# qemu -kernel loads the kernel at 0x80000000
        # and causes each CPU to jump there.
        # ld_script causes the following code to
        # be placed at 0x80000000.
.global start
.global xn6_start_kernel
.section .text

.global _entry
_entry:
	# set up a stack for C.
        # with a 4096-byte stack per CPU.
        la sp, entry_stack
#中文注释
#使用sbi时，在这里不能读mhartid!!
        #li a0, 1024*4
	#csrr a1, mhartid #now there only is one CPU
        #addi a1, a1, 1
        #mul a0, a0, a1
        #add sp, sp, a0
	# jump to xn6_start_kernel() in kernel/xn6_start_kernel.c
        call start
spin:
        j spin