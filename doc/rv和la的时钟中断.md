# 解析riscv和loongarch的时钟中断

# 首先是时钟中断产生的条件：
1.全局中断使能
2.时钟中断使能
3.设定的时钟值归零
在前两个条件满足的前提下，第三个条件才会触发时钟中断

loongarch开启时钟中断:
1. w_csr_crmd(r_csr_crmd() | CSR_CRMD_IE);
2. w_csr_ecfg(r_csr_ecfg() | TI_VEC);
3. w_csr_tcfg(tcfg_val); // 按格式设定好时钟中断间隔

riscv开启时钟中断:
1. w_sstatus(r_sstatus() | SSTATUS_SIE);
2. w_sie(r_sie() | SIE_STIE);
3. sbi_call(SBI_SET_TIMER, r_time() + INTERVAL, 0, 0); // 使用sbi的情况。不使用sbi会复杂一点.见[1]
三个条件必须都满足

# 如何关闭时钟中断呢？破坏三个条件的任意一个都可以，先讲前两个条件
loongarch关闭时钟中断
1. w_csr_crmd(r_csr_crmd() & ~CSR_CRMD_IE);
2. w_csr_ecfg(r_csr_ecfg() & ~TI_VEC);

riscv关闭时钟中断
1. w_sstatus(r_sstatus() & ~SSTATUS_SIE);
2. w_sie(r_sie() & ~SIE_STIE);
任选一条都能关闭时钟中断。

然后是破坏第三个条件，但是riscv和loongarch的判断条件不同，所以单独讲一下:
loongarch在时钟中断之后,硬件自动以相同的时钟间隔计时下一次
而riscv关注在时钟中断之后,需要软件手动设置下一次时钟中断的间隔,可以设置不同的间隔

那么破坏第三个条件的方法就是:
loongarch, w_csr_tcfg(0); 关闭计时
riscv, 实测可以set_timer(r_time()-10000000); // 为什么？见[2]

# 最后讲一下破坏三种条件的影响：
1. 关闭全局中断。那么不仅关闭了时钟中断，外部中断也被关闭了。
2. 关闭时钟中断使能。这时不会处理时钟中断，但是时钟还在计数。如果之后又开启时钟中断使能，那么有两种情况:
    a.开启之后计时才到期，这个简单，结果上就跟没有关闭时钟中断使能一样，就好像没有影响。 
    b.开启之前计时就到期了，那么一开启时钟中断使能，马上响应时钟中断
3. 修改设定的时钟值。之后要恢复时钟中断必须重新设定，那就是重新开始计时了

2和3的区别是,2不会影响时钟倒计时,只是有可能延迟响应时钟中断；但是3会重置时钟
1会影响到其他中断，如果只是为了关闭时钟中断，一般不要使用1

# [1]不使用sbi,开启时钟中断的指令，必须在M态使用：
    /* enable supervisor-mode timer interrupts. */
    w_mie(r_mie() | MIE_STIE);
  
    /* enable the sstc extension (i.e. stimecmp). */
    w_menvcfg(r_menvcfg() | (1L << 63)); 
  
    /* allow supervisor to use stimecmp and time. */
    w_mcounteren(r_mcounteren() | 2);
  
    /* ask for the very first timer interrupt. */
    w_stimecmp(r_time() + INTERVAL);

如果不使用sbi,riscv设定下一次时钟中断间隔的代码也要改
不使用sbi的代码     w_stimecmp(r_time() + INTERVAL);
sbi的代码          sbi_call(SBI_SET_TIMER, r_time() + INTERVAL, 0, 0);

# [2]riscv通过比较time和stimecmp的值来判断计时是否到期  //不使用sbi的话，sbi也类似
time和stimecmp都是64的寄存器,表示时钟周期数。理论上计算机一次运行不会导致溢出
所以设置stimecmp为比time小的数,time就不会等于stimecmp了


