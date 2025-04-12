#编译.o文件到build/riscv/kernel目录
#调用make riscv

#已经定义了 CFLAGS += -Iinclude; ASFLAGS += -Iinclude

# card source
c_src := $(wildcard *.c) #user.c
s_src := $(wildcard *.S) #usys.S

# card obj file name
c_obj = $(patsubst %.c,$(RISCV_BUILDPATH)/kernel/%.o,$(c_src))
s_obj = $(patsubst %.S,$(RISCV_BUILDPATH)/kernel/%.o,$(s_src))

# 用户程序二进制及头文件生成相关变量
user_out := $(RISCV_BUILDPATH)/kernel/user.out
user_bin := $(RISCV_BUILDPATH)/kernel/user.bin
user_header := ../include/proc/user.h
user_objs := $(RISCV_BUILDPATH)/kernel/user.o $(RISCV_BUILDPATH)/kernel/usys.o

# 链接用户程序
$(user_out): $(user_objs)
	$(RISCV_LD) -nostdlib -static -e  init_main -Ttext 0 -o $@ $^

# 提取二进制数据
$(user_bin): $(user_out)
	$(RISCV_OBJCOPY) -S -O binary $< $@

# 生成C头文件，确保目录存在
$(user_header): $(user_bin)
	mkdir -p $(dir $@)
	xxd -i $< > $@


# the source files' dependencies, created by gcc
depends = \
	$(patsubst %.c, $(RISCV_BUILDPATH)/kernel/%.d, $(c_src)) \
	$(patsubst %.S, $(RISCV_BUILDPATH)/kernel/%.d, $(s_src))

riscv: $(c_obj) $(s_obj) $(user_header)

$(c_obj): $(RISCV_BUILDPATH)/kernel/%.o: %.c 
	$(RISCV_CC) -c $(RISCV_CFLAGS) -MF $(RISCV_BUILDPATH)/kernel/$*.d -o $@ $<

$(s_obj): $(RISCV_BUILDPATH)/kernel/%.o: %.S 
	$(RISCV_CC) -c $(RISCV_ASFLAGS) -MF $(RISCV_BUILDPATH)/kernel/$*.d -o $@ $<

-include $(depends)