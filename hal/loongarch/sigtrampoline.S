
.section .text.sigtrampoline
.align 12   # 4k对齐
.global sigtrampoline
sigtrampoline:
sigreturn:
        li.d	$a7, 715
        syscall	0
