
.section .text.sigtrampoline
.align 12   # 4k对齐
.global sigtrampoline
sigtrampoline:
sigreturn:
        li a7, 715
        ecall
