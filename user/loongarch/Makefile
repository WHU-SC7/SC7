#编译.o文件到build/loongarch/kernel目录
#调用make riscv

#已经定义了 CFLAGS += -Iinclude; ASFLAGS += -Iinclude

# card source
c_src := $(wildcard *.c) #user.c
s_src := $(wildcard *.S) #usys.S

# card obj file name
c_obj = $(patsubst %.c,$(BUILDPATH)/kernel/%.o,$(c_src))
s_obj = $(patsubst %.S,$(BUILDPATH)/kernel/%.o,$(s_src))

# the source files' dependencies, created by gcc
depends = \
	$(patsubst %.c, $(BUILDPATH)/kernel/%.d, $(c_src)) \
	$(patsubst %.S, $(BUILDPATH)/kernel/%.d, $(s_src))

riscv: $(c_obj) $(s_obj)

$(c_obj): $(BUILDPATH)/kernel/%.o: %.c 
	$(CC) -c $(CFLAGS) -MF $(BUILDPATH)/kernel/$*.d -o $@ $<

$(s_obj): $(BUILDPATH)/kernel/%.o: %.S 
	$(CC) -c $(ASFLAGS) -MF $(BUILDPATH)/kernel/$*.d -o $@ $<

-include $(depends)