
c_src := $(wildcard *.c)
s_src := $(wildcard *.S)

c_obj = $(patsubst %.c,$(RISCV_BUILDPATH)/kernel/%.o,$(c_src))
s_obj = $(patsubst %.S,$(RISCV_BUILDPATH)/kernel/%.o,$(s_src))
objs = $(c_obj) $(s_obj)

riscv: initdir $(RISCV_BUILDPATH)/hal_riscv_qemu_virt.a

initdir:
	mkdir -p $(RISCV_BUILDPATH)/kernel

$(RISCV_BUILDPATH)/hal_riscv_qemu_virt.a: $(objs)
	$(AR) rcs $@ $^

$(c_obj): $(RISCV_BUILDPATH)/kernel/%.o: %.c 
	$(RISCV_CC) -c $(RISCV_CFLAGS) -MF $(RISCV_BUILDPATH)/kernel/$*.d -o $@ $<

$(s_obj): $(RISCV_BUILDPATH)/kernel/%.o: %.S 
	$(RISCV_CC) -c $(RISCV_ASFLAGS) -MF $(RISCV_BUILDPATH)/kernel/$*.d -o $@ $<