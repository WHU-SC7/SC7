

# 带有export的变量会在递归调用子目录的Makefile时传递下去
export TOOLPREFIX =loongarch64-linux-gnu-

export CC  = ${TOOLPREFIX}gcc
export AS  = ${TOOLPREFIX}gcc
export LD  = ${TOOLPREFIX}gcc
export OBJCOPY = ${TOOLPREFIX}objcopy
export OBJDUMP = ${TOOLPREFIX}objdump
export AR  = ${TOOLPREFIX}ar

#现在include目录独立出来了
export INCLUDE_FALGES = -I../include/kernel -I../include/hsai 

export ASFLAGS = -ggdb3 -march=loongarch64 -mabi=lp64d -O0
export ASFLAGS += -Iinclude $(INCLUDE_FALGES)
export ASFLAGS += -MD
export CFLAGS = -ggdb3 -Wall -Werror -O0 -fno-omit-frame-pointer 
export CFLAGS += -Iinclude $(INCLUDE_FALGES)
export CFLAGS += -MD #生成make的依赖文件到.d文件
export CFLAGS += -DNUMCPU=1 #宏
export CFLAGS += -march=loongarch64 -mabi=lp64d
export CFLAGS += -ffreestanding -fno-common -nostdlib -fno-stack-protector 
export CFLAGS += -fno-pie -no-pie 
export CFLAGS += -DDEBUG=1
export LDFLAGS = -z max-page-size=4096
export WORKPATH = $(shell pwd)
export BUILDPATH = $(WORKPATH)/build/loongarch#build/loongarch

LD_SCRIPT = hal/loongarch/ld.script

##要编译的源文件
export la_hal_srcs = $(wildcard hal/loongarch/*.c hal/loongarch/*.S) #hal loongarch 文件
export rv_hal_srcs = $(wildcard hal/riscv/*.S hal/riscv/*.c) #hal riscv 文件
##上面rv与la的文件名排列顺序不一样，rv_hal_srcs是.S文件在前
##非常奇怪，riscv链接时要求按 entry stack uart xn6_start顺序来，否则不能正常输出字符。
##可能是依赖关系没处理好？

#用户程序的源文件
export la_user_srcs = $(wildcard user/loongarch/*.S user/loongarch/*.c) #user loongarch 文件
export rv_user_srcs = $(wildcard user/riscv/*.S user/riscv/*.c) #user riscv 文件

export kernel_srcs = $(wildcard kernel/*.c)
export hsai_srcs = $(wildcard hsai/*.c)

#loongarch的所有.c文件和.S文件
la_srcs = $(la_hal_srcs) $(hsai_srcs) $(kernel_srcs) 
la_src_names = $(notdir $(la_srcs)) 
la_c_objs = $(patsubst %.c,$(BUILDPATH)/kernel/%.o,$(la_src_names)) #先替换c
la_objs = $(patsubst %.S,$(BUILDPATH)/kernel/%.o,$(la_c_objs)) #再替换S,获得所有目标文件路径


# .PHONY 是一个伪规则，其后面依赖的规则目标会成为一个伪目标，使得规则执行时不会实际生成这个目标文件
.PHONY: la init_la_dir compile_all load_kernel clean la_qemu

la: init_la_dir compile_all load_kernel

init_la_dir:
	mkdir -p $(BUILDPATH)/kernel

compile_all: 
	$(MAKE) la -C user/loongarch
	$(MAKE) -C hal/loongarch
	$(MAKE) -C kernel
	$(MAKE) -C hsai

#定义loongarhc系统镜像路径和名字
la_kernel = $(WORKPATH)/build/loongarch/kernel-la

load_kernel: $(la_objs) $(LD_SCRIPT)
	$(LD) $(LDFLAGS) -T $(LD_SCRIPT) -o $(la_kernel) $(la_objs) 

clean: #删除rv,la的build路径
	rm -rf build/loongarch
	rm -rf build/riscv
	rm -rf user/build

la_qemu: 
	./run.sh

docker_la: init_la_dir docker_compile_all load_kernel
	@echo "__________________________"
	@echo "-------- 生成成功 --------"
	
#参考bakaos的启动选项，dokcer_la_qemu启动有问题，磁盘读写不正常
virt: 
	qemu-system-loongarch64 \
	-kernel build/loongarch/kernel-la \
	-m 1G -nographic -smp 1 \
				-drive file=tmp/test_echo_la,if=none,format=raw,id=x0  \
                -device virtio-blk-pci,drive=x0 -no-reboot  \
				-device virtio-net-pci,netdev=net0 \
                -netdev user,id=net0,hostfwd=tcp::5555-:5555,hostfwd=udp::5555-:5555   \
	-s -S
#                -rtc base=utc
#-drive file=disk.img,if=none,format=raw,id=x1 -device virtio-blk-pci,drive=x1 \

run:
	qemu-system-loongarch64 \
	-kernel build/loongarch/kernel-la \
	-m 1G -nographic -smp 1 \
				-drive file=tmp/test_echo_la,if=none,format=raw,id=x0  \
                -device virtio-blk-pci,drive=x0 -no-reboot  \
				-device virtio-net-pci,netdev=net0 \
                -netdev user,id=net0,hostfwd=tcp::5555-:5555,hostfwd=udp::5555-:5555

docker_compile_all: #编译之后想回归ls2k的版本，要先clean再make all
	rm -rf build/loongarch
	mkdir -p $(BUILDPATH)/kernel
	$(MAKE) la -C user/loongarch
	$(MAKE) -C hal/loongarch QEMU=virt
	$(MAKE) -C kernel
	$(MAKE) -C hsai

docker_la_qemu: #本机的qemu没有virt机型，评测机下才可以使用
	qemu-system-loongarch64 \
	-M virt \
	-serial stdio \
	-smp 1 \
	-kernel build/loongarch/kernel-la \
	-m 1G \
	-display none \
	-drive file=tmp/fs.img,if=none,format=raw,id=x0  \
    -device virtio-blk-pci,drive=x0,bus=pcie.0 
#	-s -S
#	-k ./share/qemu/keymaps/en-us #这一条在docker的qemu中会报错
#待添加磁盘挂载

#----------------------------------------------------------------------------------------------------
#以下是riscv变量和目标
#----------------------------------------------------------------------------------------------------

export RISCV_BUILDPATH = $(WORKPATH)/build/riscv
export RISCV_TOOLPREFIX =riscv64-linux-gnu-

export RISCV_CC  = ${RISCV_TOOLPREFIX}gcc
export RISCV_AS  = ${RISCV_TOOLPREFIX}gcc
export RISCV_LD  = ${RISCV_TOOLPREFIX}ld
export RISCV_OBJCOPY = ${RISCV_TOOLPREFIX}objcopy
export RISCV_OBJDUMP = ${RISCV_TOOLPREFIX}objdump

export RISCV_ASFLAGS = -ggdb3 -march=rv64gc -mabi=lp64d -O0
export RISCV_ASFLAGS += -MD
export RISCV_ASFLAGS += -Iinclude $(INCLUDE_FALGES) 
export RISCV_CFLAGS = -ggdb3 -Wall -Werror -O0 -fno-omit-frame-pointer
export RISCV_CFLAGS += -Iinclude $(INCLUDE_FALGES) 
export RISCV_CFLAGS += -MD 
export RISCV_CFLAGS += -DNUMCPU=1 #宏
export RISCV_CFLAGS += -DOPEN_COLOR_PRINT=1 #log宏，现在没有
export RISCV_CFLAGS += -march=rv64gc -mabi=lp64d
export RISCV_CFLAGS += -ffreestanding -fno-common -nostdlib -fno-stack-protector 
export RISCV_CFLAGS += -fno-pie -no-pie 
export RISCV_CFLAGS += -mcmodel=medany
export RISCV_CFLAGS += -mno-relax
export RISCV_CFLAGS += -DDEBUG=1
export RISCV_LDFLAGS = -z max-page-size=4096

export RISCV_CFLAGS += -DRISCV=1 #宏

RISCV_LD_SCRIPT =hal/riscv/ld.script

#riscv的所有.c和.S文件
rv_srcs = $(rv_hal_srcs) $(kernel_srcs) $(hsai_srcs) 
rv_src_names = $(notdir $(rv_srcs))
rv_c_objs = $(patsubst %.c,$(RISCV_BUILDPATH)/kernel/%.o,$(rv_src_names)) #先替换c
rv_objs = $(patsubst %.S,$(RISCV_BUILDPATH)/kernel/%.o,$(rv_c_objs)) #再替换S,获得所有目标文件路径

#sbi镜像和没有sbi的镜像有冲突，必须重新编译start.c和vmem.c。所以要清理build/rv
clean_rv:
	rm -rf build/riscv
#user应该不用清理

rv: clean_rv init_rv_dir compile_riscv load_riscv_kernel
	@echo "__________________________"
	@echo "-------- 生成成功 --------"

init_rv_dir: #为各模块创建好目录
	mkdir -p build/riscv/kernel
	
compile_riscv:
#先编译用户程序;然后kernel编译时会使用user/build下产生的initcode
	$(MAKE) riscv -C user/riscv  
	$(MAKE) riscv -C hal/riscv
	$(MAKE) riscv -C kernel
	$(MAKE) riscv -C hsai

#定义loongarhc系统镜像路径和名字
rv_kernel = $(RISCV_BUILDPATH)/kernel-rv	

load_riscv_kernel: $(RISCV_LD_SCRIPT) $(rv_objs)
	$(RISCV_LD) $(RISCV_LDFLAGS) -T $(RISCV_LD_SCRIPT) -o $(rv_kernel) $(rv_objs)
	
#很笨的老方法，暂时保留
ld_objs = $(RISCV_BUILDPATH)/kernel/entry.o \
			$(RISCV_BUILDPATH)/kernel/stack.o \
			$(RISCV_BUILDPATH)/kernel/uart.o \
			$(RISCV_BUILDPATH)/kernel/sc7_start_kernel.o

#riscv_disk_file = tmp/fs.img
riscv_disk_file = tmp/hello.elf

QEMUOPTS = -machine virt -bios none -kernel build/riscv/kernel-rv -m 128M -smp 1 -nographic
QEMUOPTS += -drive file=$(riscv_disk_file),if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
QEMUOPTS += -s -S

rv_qemu: #评测docker运行riscv qemu,本机也可以 调试后缀 ：-gdb tcp::1235  -S
	qemu-system-riscv64 $(QEMUOPTS)

#编译使用open-sbi的riscv内核。区别是内核起始段变为0x80200000和不调用start函数。为了兼容，编译时宏跳过start函数体的内容
sbi: clean_rv init_rv_dir sbi_compile_riscv sbi_load_riscv_kernel

sbi_compile_riscv:
	$(MAKE) riscv -C user/riscv  
#让hal层编译start.c时传入宏sbi
	$(MAKE) riscv -C hal/riscv SBI=1
	$(MAKE) riscv -C kernel SBI=1
	$(MAKE) riscv -C hsai SBI=1

SBI_RISCV_LD_SCRIPT =hal/riscv/sbi_ld.script

sbi_load_riscv_kernel: $(SBI_RISCV_LD_SCRIPT) $(rv_objs)
	$(RISCV_LD) $(RISCV_LDFLAGS) -T $(SBI_RISCV_LD_SCRIPT) -o $(rv_kernel) $(rv_objs)


sbi_QEMUOPTS = -machine virt -bios default -kernel build/riscv/kernel-rv -m 128M -smp 1 -nographic
sbi_QEMUOPTS += -drive file=$(riscv_disk_file),if=none,format=raw,id=x0
sbi_QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
sbi_QEMUOPTS += -s -S

sbi_qemu: #初赛，使用opensbi
	qemu-system-riscv64 $(sbi_QEMUOPTS)

run_sbi:
	qemu-system-riscv64 -machine virt -bios default -kernel build/riscv/kernel-rv -m 128M -smp 1 -nographic -drive file=$(riscv_disk_file),if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

show:
	@echo $(rv_hal_srcs)

user: initcode show_initcode_rv show_initcode_la

initcode:
	$(MAKE) riscv -C user/riscv
	$(MAKE) la -C user/loongarch

#输出汇编到文件user/build/rv_init_code.asm。la同理
show_initcode_rv:
#	riscv64-unknown-elf-objdump -D -b binary -m riscv:rv64  user/build/riscv/user.bin 
	riscv64-unknown-elf-objdump -D -b binary -m riscv:rv64  user/build/riscv/user.bin > user/build/rv_init_code.asm

show_initcode_la:
#	loongarch64-linux-gnu-objdump -D -b binary -m loongarch user/build/loongarch/user.bin
	loongarch64-linux-gnu-objdump -D -b binary -m loongarch user/build/loongarch/user.bin >user/build/la_init_code.asm