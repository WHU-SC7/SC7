#编译.o文件到build/riscv/kernel目录
#调用make riscv

#已经定义了 CFLAGS += -Iinclude; ASFLAGS += -Iinclude

# card source
c_src := $(wildcard *.c) #user.c
s_src := $(wildcard *.S) #usys.S

# card obj file name
c_obj = $(patsubst %.c,$(RISCV_BUILDPATH)/kernel/%.o,$(c_src))
s_obj = $(patsubst %.S,$(RISCV_BUILDPATH)/kernel/%.o,$(s_src))

# the source files' dependencies, created by gcc
depends = \
	$(patsubst %.c, $(RISCV_BUILDPATH)/kernel/%.d, $(c_src)) \
	$(patsubst %.S, $(RISCV_BUILDPATH)/kernel/%.d, $(s_src))

riscv: $(c_obj) $(s_obj)

$(c_obj): $(RISCV_BUILDPATH)/kernel/%.o: %.c 
	$(RISCV_CC) -c $(RISCV_CFLAGS) -MF $(RISCV_BUILDPATH)/kernel/$*.d -o $@ $<

$(s_obj): $(RISCV_BUILDPATH)/kernel/%.o: %.S 
	$(RISCV_CC) -c $(RISCV_ASFLAGS) -MF $(RISCV_BUILDPATH)/kernel/$*.d -o $@ $<

-include $(depends)