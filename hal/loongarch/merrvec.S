.section merrentry
.globl _wrp_machine_trap
.globl handle_merr
.align 0x4
handle_merr:
    la $t0, machine_trap  # 加载machine_trap的绝对地址到寄存器$t0
    jr $t0   
    ertn