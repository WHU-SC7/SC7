#编译.o文件到$(WORKPATH)/user/build/riscv目录
#调用make riscv
#但是现在用户程序不会链接到内核，而是在user目录下生成init_code

#已经定义了 CFLAGS += -Iinclude; ASFLAGS += -Iinclude

# card source
c_src := $(wildcard *.c) #user.c
s_src := $(wildcard *.S) #usys.S

# card obj file name
c_obj = $(patsubst %.c,$(WORKPATH)/user/build/riscv/%.o,$(c_src))
s_obj = $(patsubst %.S,$(WORKPATH)/user/build/riscv/%.o,$(s_src))

# 用户程序二进制及头文件生成相关变量
user_out := $(WORKPATH)/user/build/riscv/user.out
user_bin := $(WORKPATH)/user/build/riscv/user.bin
user_header := ../build/rv_init_code.h
user_objs := $(WORKPATH)/user/build/riscv/user.o $(WORKPATH)/user/build/riscv/usys.o

# 链接用户程序
$(user_out): $(user_objs)
	$(RISCV_LD) -nostdlib -static -e  init_main -T user_initcode.ld -Ttext 0 -o $@ $^

# 提取二进制数据
$(user_bin): $(user_out)
	$(RISCV_OBJCOPY) -S -O binary $< $@

vector_name = $(subst /,_,$(subst .,_,$(user_bin)))#路径相关的数组名,末尾不要有空格....
# 生成C头文件，确保目录存在
$(user_header): $(user_bin)
	mkdir -p $(dir $@)
	xxd -i $< > $@
	sed -i 's/$(vector_name)/init_code/g' $(user_header)


# the source files' dependencies, created by gcc
depends = \
	$(patsubst %.c, $(WORKPATH)/user/build/riscv/%.d, $(c_src)) \
	$(patsubst %.S, $(WORKPATH)/user/build/riscv/%.d, $(s_src))

#现在不需要把用户程序链接到内核了
riscv: init_path $(user_header) #$(c_obj) $(s_obj)

$(c_obj): $(WORKPATH)/user/build/riscv/%.o: %.c 
	$(RISCV_CC) -c $(RISCV_CFLAGS) -MF $(WORKPATH)/user/build/riscv/$*.d -o $@ $<

$(s_obj): $(WORKPATH)/user/build/riscv/%.o: %.S 
	$(RISCV_CC) -c $(RISCV_ASFLAGS) -MF $(WORKPATH)/user/build/riscv/$*.d -o $@ $<

-include $(depends)

init_path:
	mkdir -p $(WORKPATH)/user/build/riscv/
