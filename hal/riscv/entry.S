# qemu -kernel loads the kernel at 0x80200000
        # and causes each CPU to jump there.
        # ld_script causes the following code to
        # be placed at 0x80200000.
.global start
.global sc7_start_kernel
.section .text

.global _entry
_entry:
        # entry_stack在process.c中声明
        la sp, entry_stack

        mv tp, a0

        li t0, 4096
        addi t1, tp, 1
        mul t2, t0, t1
        add sp, sp, t2
        
	# jump to sc7_start_kernel() in kernel/sc7_start_kernel.c
        j start
spin:
        j spin