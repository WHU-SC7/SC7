#编译.o文件到build/loongarch/kernel目录
#调用make all (对应loongarch)

# card source
c_src = $(wildcard *.c)
s_src = $(wildcard *.S)

# card obj file name
c_obj = $(patsubst %.c,$(BUILDPATH)/kernel/%.o,$(c_src))
s_obj = $(patsubst %.S,$(BUILDPATH)/kernel/%.o,$(s_src))

# the source files' dependencies, created by gcc
depends = \
	$(patsubst %.c, $(BUILDPATH)/kernel/%.d, $(c_src)) \
	$(patsubst %.S, $(BUILDPATH)/kernel/%.d, $(s_src))

ifeq ($(QEMU),virt) ##把宏QEMU_VIRT设定为1,uart.c会使用virt的串口地址。
CFLAGS += -DQEMU_VIRT=1 
endif

.PHONY: all

all: $(c_obj) $(s_obj)

$(c_obj): $(BUILDPATH)/kernel/%.o: %.c 
	$(CC) -c $(CFLAGS) -MF $(BUILDPATH)/kernel/$*.d -o $@ $<

$(s_obj): $(BUILDPATH)/kernel/%.o: %.S 
	$(AS) -c $(ASFLAGS) -MF $(BUILDPATH)/kernel/$*.d -o $@ $<

-include $(depends)

#这里没有riscv目标