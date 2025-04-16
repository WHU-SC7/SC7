#编译.o文件到build/*/kernel目录，*可以为loongarch或riscv
#调用make all (对应loongarch)或make riscv

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

.PHONY: all riscv

all:  $(c_obj) $(s_obj)

$(c_obj): $(BUILDPATH)/kernel/%.o: %.c 
	$(CC) -c $(CFLAGS) -MF $(BUILDPATH)/kernel/$*.d -o $@ $<

$(s_obj): $(BUILDPATH)/kernel/%.o: %.S 
	$(CC) -c $(ASFLAGS) -MF $(BUILDPATH)/kernel/$*.d -o $@ $<

-include $(depends) #依赖文件.d，实现增量编译，不必每次全部编译

#----------------------------------------------------------------------------------------------------
#以下是riscv变量和目标
#----------------------------------------------------------------------------------------------------

#共用c_src,s_src
riscv_c_obj = $(patsubst %.c,$(RISCV_BUILDPATH)/kernel/%.o,$(c_src))
riscv_s_obj = $(patsubst %.S,$(RISCV_BUILDPATH)/kernel/%.o,$(s_src))

#SBI宏应该对loongarch没有影响
ifeq ($(SBI),1)
RISCV_CFLAGS+=-DSBI=1
RISCV_ASFLAGS+=-DSBI=1
endif

riscv_depends = \
	$(patsubst %.c, $(RISCV_BUILDPATH)/kernel/%.d, $(c_src)) \
	$(patsubst %.S, $(RISCV_BUILDPATH)/kernel/%.d, $(s_src))

riscv: $(riscv_c_obj) $(riscv_s_obj)

$(riscv_c_obj): $(RISCV_BUILDPATH)/kernel/%.o: %.c 
	$(RISCV_CC) -c $(RISCV_CFLAGS) -MF $(RISCV_BUILDPATH)/kernel/$*.d -o $@ $<

$(riscv_s_obj): $(RISCV_BUILDPATH)/kernel/%.o: %.S 
	$(RISCV_CC) -c $(RISCV_ASFLAGS) -MF $(RISCV_BUILDPATH)/kernel/$*.d -o $@ $<

-include $(riscv_depends)
