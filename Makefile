# configure 
CONF_CPU_NUM = 2
export CONF_ARCH ?= loongarch
export CONF_PLATFORM ?= qemu_2k1000
# CONF_LINUX_BUILD = 1

# make variable define 

HOST_OS = $(shell uname)
HAL_LIB_NAME = hal_${CONF_ARCH}_${CONF_PLATFORM}.a

# 带有export的变量会在递归调用子目录的Makefile时传递下去

export TOOLPREFIX =loongarch64-linux-gnu-
export DEFAULT_CXX_INCLUDE_FLAG = \
	-include $(WORKPATH)/kernel/include/xn6_config.h \
	-include $(WORKPATH)/kernel/include/klib/virtual_function.hh \
	-include $(WORKPATH)/kernel/include/klib/global_operator.hh \
	-include $(WORKPATH)/kernel/include/types.hh \
	-include $(WORKPATH)/hsai/include/hsai_defs.h

export CC  = ${TOOLPREFIX}gcc
export CXX = ${TOOLPREFIX}g++
export CPP = ${TOOLPREFIX}cpp
export AS  = ${TOOLPREFIX}g++
export LD  = ${TOOLPREFIX}g++
export OBJCOPY = ${TOOLPREFIX}objcopy
export OBJDUMP = ${TOOLPREFIX}objdump
export AR  = ${TOOLPREFIX}ar

export ASFLAGS = -ggdb -march=loongarch64 -mabi=lp64d -O0
export ASFLAGS += -I include
export ASFLAGS += -MD
export CFLAGS = -ggdb -Wall -Werror -O0 -fno-omit-frame-pointer
export CFLAGS += -I include
export CFLAGS += -MD 
export CFLAGS += -DNUMCPU=$(CONF_CPU_NUM)
export CFLAGS += -DARCH=$(CONF_ARCH)
export CFLAGS += -DPLATFORM=$(CONF_PLATFORM)
export CFLAGS += -DOPEN_COLOR_PRINT=1
# export CFLAGS += -DOS_DEBUG										# open debug output
ifeq ($(HOST_OS),Linux)
export CFLAGS += -DLINUX_BUILD=1
endif
export CFLAGS += -march=loongarch64 -mabi=lp64d
export CFLAGS += -ffreestanding -fno-common -nostdlib -fno-stack-protector 
export CFLAGS += -fno-pie -no-pie 
# export CFLAGS += -static-libstdc++ -lstdc++
export CXXFLAGS = $(CFLAGS)
export CXXFLAGS += -std=c++23
export CXXFLAGS += $(DEFAULT_CXX_INCLUDE_FLAG)
export LDFLAGS = -z max-page-size=4096

export WORKPATH = $(shell pwd)
export BUILDPATH = $(WORKPATH)/build/loongarch


STATIC_MODULE = \
	$(BUILDPATH)/kernel.a \
	$(BUILDPATH)/$(HAL_LIB_NAME) 
#	$(BUILDPATH)/user/user.a \
#	$(BUILDPATH)/hsai.a \
#	$(BUILDPATH)/thirdparty/EASTL/libeastl.a \

LD_SCRIPT = hal/$(CONF_ARCH)/$(CONF_PLATFORM)/ld.script

COMPILE_START_TIME := $(shell cat /proc/uptime | awk -F '.' '{print $$1}')


# .PHONY 是一个伪规则，其后面依赖的规则目标会成为一个伪目标，使得规则执行时不会实际生成这个目标文件
.PHONY: all clean test initdir probe_host compile_all load_kernel cp_to_bin EASTL EASTL_test config_platform print_compile_time

# rules define 

all: probe_host compile_all load_kernel cp_to_bin
	@current_time=`cat /proc/uptime | awk -F '.' '{print $$1}'`; \
	echo "######## 生成结束时间戳: $${current_time} ########"; \
	time_interval=`expr $${current_time} - $(COMPILE_START_TIME)`; \
	runtime=`date -u -d @$${time_interval} "+%Mm %Ss"`; \
	echo "######## 生成用时 : $${runtime} ########"
	@echo "__________________________"
	@echo "-------- 生成成功 --------"

probe_host:
	@echo "********************************"
	@echo "当前主机操作系统：$(HOST_OS)"
	@echo "编译目标平台：$(CONF_ARCH)-$(CONF_PLATFORM)"
	@echo "当前时间戳: $(COMPILE_START_TIME)"
	@echo "********************************"

compile_all:
#	$(MAKE) all -C thirdparty/EASTL
#	$(MAKE) all -C user
#	$(MAKE) all -C hsai
	$(MAKE) all -C hal/$(CONF_ARCH)
	$(MAKE) all -C kernel

EA: 
	$(MAKE) all -C thirdparty/EASTL

load_kernel: $(BUILDPATH)/kernel.elf

$(BUILDPATH)/kernel.elf: $(STATIC_MODULE) $(LD_SCRIPT)
	$(LD) $(LDFLAGS) -T $(LD_SCRIPT) -o $@ -Wl,--whole-archive $(STATIC_MODULE) -Wl,--no-whole-archive

cp_to_bin: kernel.bin

kernel.bin: $(BUILDPATH)/kernel.elf
	$(OBJCOPY) -O binary $(BUILDPATH)/kernel.elf ./kernel.bin

test:
	@echo $(COMPILE_START_TIME); sleep 3; \
	current_time=`cat /proc/uptime | awk -F '.' '{print $$1}'`; echo $${current_time}; \
	echo `expr $${current_time} - $(COMPILE_START_TIME)`
# $(MAKE) test -C hsai
#	$(MAKE) test -C kernel

print_inc:
	$(CPP) $(DEFAULT_CXX_INCLUDE_FLAG) -v /dev/null -o /dev/null

print_compile_time:
	@current_time=`cat /proc/uptime | awk -F '.' '{print $$1}'`; \
	time_interval=`expr $${current_time} - $(COMPILE_START_TIME)`; \
	runtime=`date -u -d @$${time_interval} "+%Mm %Ss"`; \
	echo "######## 生成用时 : $${runtime} ########"

clean:
	$(MAKE) clean -C kernel
	$(MAKE) clean -C user
#	$(MAKE) clean -C hsai
	$(MAKE) clean -C hal/$(CONF_ARCH)
	rm -rf build/riscv

.PHONY+= clean_module
clean_module:
	$(MAKE) clean -C $(mod)

EASTL_test:
	$(MAKE) test -C thirdparty/EASTL

loongarch: 
	./run.sh

docker_qemu:
	qemu-system-riscv64 -machine virt -bios none -kernel build/riscv/kernel-rv -m 128M -smp 1 -nographic

# config_platform:
# 	@cd hal/$(CONF_ARCH)/$(CONF_PLATFORM); \
# 		cp config.mk $(WORKPATH)/hsai/Makedefs.mk
# 	@echo "******** 配置成功 ********"
# 	@echo "- 架构 : ${CONF_ARCH}"
# 	@echo "- 平台 : ${CONF_PLATFORM}"
# 	@echo "**************************"

OSCOMP_TOOLPATH=/home/lu/tool_riscv-unknown-elf/os-contest-2024-image/kendryte-toolchain/bin
export RISCV_BUILDPATH = $(WORKPATH)/build/riscv
export RISCV_TOOLPREFIX =riscv64-unknown-elf-
#export RISCV_TOOLPREFIX =$(OSCOMP_TOOLPATH)/riscv64-unknown-elf-
export RISCV_DEFAULT_CXX_INCLUDE_FLAG = \
	-include $(WORKPATH)/kernel/include/xn6_config.h \
	-include $(WORKPATH)/kernel/include/klib/virtual_function.hh \
	-include $(WORKPATH)/kernel/include/klib/global_operator.hh \
	-include $(WORKPATH)/kernel/include/types.hh \
	-include $(WORKPATH)/hsai/include/hsai_defs.h

RISCV_CONF_CPU_NUM = 2
export RISCV_CONF_ARCH ?= riscv
export RISCV_CONF_PLATFORM ?= qemu_virt

export RISCV_CC  = ${RISCV_TOOLPREFIX}gcc
export RISCV_CXX = ${RISCV_TOOLPREFIX}g++
export RISCV_CPP = ${RISCV_TOOLPREFIX}cpp
export RISCV_AS  = ${RISCV_TOOLPREFIX}g++
export RISCV_LD  = ${RISCV_TOOLPREFIX}ld
export RISCV_OBJCOPY = ${RISCV_TOOLPREFIX}objcopy
export RISCV_OBJDUMP = ${RISCV_TOOLPREFIX}objdump
export RISCV_AR  = ${RISCV_TOOLPREFIX}ar
#
#
#
export RISCV_ASFLAGS = -ggdb -march=rv64gc -mabi=lp64d -O0
#export RISCV_ASFLAGS += -I include
export RISCV_ASFLAGS += -MD
export RISCV_CFLAGS = -ggdb -Wall -Werror -O0 -fno-omit-frame-pointer
#export RISCV_CFLAGS += -I include
export RISCV_CFLAGS += -I.
export RISCV_CFLAGS += -MD 
export RISCV_CFLAGS += -DNUMCPU=$(RISCV_CONF_CPU_NUM)
export RISCV_CFLAGS += -DARCH=$(RISCV_CONF_ARCH)
export RISCV_CFLAGS += -DPLATFORM=$(RISCV_CONF_PLATFORM)
export RISCV_CFLAGS += -DOPEN_COLOR_PRINT=1
# export CFLAGS += -DOS_DEBUG										# open debug output
ifeq ($(HOST_OS),Linux)
export RISCV_CFLAGS += -DLINUX_BUILD=1
endif
export RISCV_CFLAGS += -march=rv64gc -mabi=lp64d
export RISCV_CFLAGS += -ffreestanding -fno-common -nostdlib -fno-stack-protector 
export RISCV_CFLAGS += -fno-pie -no-pie 
# export CFLAGS += -static-libstdc++ -lstdc++
export RISCV_CXXFLAGS = $(RISCV_CFLAGS)
export RISCV_CXXFLAGS += -std=c++20 #not support c++23

export RISCV_CXXFLAGS += -Wno-tautological-compare -I/usr/include #avoid compile error in tuple.h

export RISCV_CXXFLAGS += $(RISCV_DEFAULT_CXX_INCLUDE_FLAG)
export RISCV_LDFLAGS = -z max-page-size=4096
#
#
#
export RISCV_CFLAGS += -mcmodel=medany
export RISCV_CFLAGS += -mno-relax

export RISCV_LDFLAGS = -z max-page-size=4096

riscv: compile_riscv load_riscv_kernel
	
compile_riscv:
	$(MAKE) riscv -C hal/riscv
	$(MAKE) riscv -C kernel
	
load_riscv_kernel: $(RISCV_BUILDPATH)/kernel-rv

RISCV_LD_SCRIPT =hal/riscv/ld.script

RISCV_MODULE = \
	$(RISCV_BUILDPATH)/kernel.a \
	$(RISCV_BUILDPATH)/hal_riscv_qemu_virt.a
	
ld_objs = $(RISCV_BUILDPATH)/kernel/entry.o \
			$(RISCV_BUILDPATH)/kernel/stack.o \
			$(RISCV_BUILDPATH)/kernel/uart.o \
			$(RISCV_BUILDPATH)/kernel/xn6_start_kernel.o

#这个失败了: $(RISCV_LD) $(RISCV_LDFLAGS) -T $(RISCV_LD_SCRIPT) -o $@ $(RISCV_MODULE) $(RISCV_BUILDPATH)/kernel/*.o
$(RISCV_BUILDPATH)/kernel-rv: $(RISCV_LD_SCRIPT) $(ld_objs)
	$(RISCV_LD) $(RISCV_LDFLAGS) -T $(RISCV_LD_SCRIPT) -o $@ $(ld_objs)

riscv_qemu:
	qemu-system-riscv64 -machine virt -bios none -kernel build/riscv/kernel-rv -m 128M -smp 1 -nographic

