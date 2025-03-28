



.macro GEN_USER_SYSCALL sys_name sys_num
	.global \sys_name
	\sys_name :
	li a7, \sys_num
	ecall
	ret
.endm