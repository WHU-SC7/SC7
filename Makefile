

# 带有export的变量会在递归调用子目录的Makefile时传递下去
export TOOLPREFIX =loongarch64-linux-gnu-

export CC  = ${TOOLPREFIX}gcc
export AS  = ${TOOLPREFIX}gcc
export LD  = ${TOOLPREFIX}gcc
export OBJCOPY = ${TOOLPREFIX}objcopy
export OBJDUMP = ${TOOLPREFIX}objdump
export AR  = ${TOOLPREFIX}ar

#现在include目录独立出来了
export INCLUDE_FALGES = -I$(WORKPATH)/include/kernel -I$(WORKPATH)/include/hsai -I$(WORKPATH)/include/kernel/fs
export INCLUDE_FALGES += -I$(WORKPATH)/include/hal/loongarch -I$(WORKPATH)/include/hal/riscv

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
export CFLAGS += -DDEBUG=0
export LDFLAGS = -z max-page-size=4096
export WORKPATH = $(shell pwd)
export BUILDPATH = $(WORKPATH)/build/loongarch#build/loongarch

LD_SCRIPT = hal/loongarch/ld.script

##要编译的源文件
la_hal_srcs = $(wildcard hal/loongarch/*.c hal/loongarch/*.S) #hal loongarch 文件
rv_hal_srcs = $(wildcard hal/riscv/*.S hal/riscv/*.c) #hal riscv 文件
##上面rv与la的文件名排列顺序不一样，rv_hal_srcs是.S文件在前
##非常奇怪，riscv链接时要求按 entry stack uart sc7_start顺序来，否则不能正常输出字符。
##可能是依赖关系没处理好？

#用户程序的源文件
la_user_srcs = $(wildcard user/loongarch/*.S user/loongarch/*.c) #user loongarch 文件
rv_user_srcs = $(wildcard user/riscv/*.S user/riscv/*.c) #user riscv 文件

#driver与架构有关
la_driver_srcs = $(wildcard kernel/driver/loongarch/*.c)
rv_driver_srcs = $(wildcard kernel/driver/riscv/*.c)

#kernel下还分fs
kernel_srcs = $(wildcard kernel/*.c \
								kernel/fs/*.c \
								)
hsai_srcs = $(wildcard hsai/*.c)

#loongarch的所有.c文件和.S文件
la_srcs = $(la_hal_srcs) $(hsai_srcs) $(kernel_srcs) $(la_driver_srcs)
la_src_names = $(notdir $(la_srcs)) 
la_c_objs = $(patsubst %.c,$(BUILDPATH)/kernel/%.o,$(la_src_names)) #先替换c
la_objs = $(patsubst %.S,$(BUILDPATH)/kernel/%.o,$(la_c_objs)) #再替换S,获得所有目标文件路径


# .PHONY 是一个伪规则，其后面依赖的规则目标会成为一个伪目标，使得规则执行时不会实际生成这个目标文件
.PHONY: init_la_dir clean

all: init_la_dir init_rv_dir
#la
	$(MAKE) -C hal/loongarch QEMU=virt
	$(MAKE) -C kernel
	$(MAKE) -C hsai
	$(LD) $(LDFLAGS) -T $(LD_SCRIPT) -o $(la_kernel) $(la_objs) 
#rv
	$(MAKE) riscv -C hal/riscv SBI=1
	$(MAKE) riscv -C kernel SBI=1
	$(MAKE) riscv -C hsai SBI=1
	$(RISCV_LD) $(RISCV_LDFLAGS) -T $(SBI_RISCV_LD_SCRIPT) -o $(rv_kernel) $(rv_objs)
	cp $(la_kernel) ./kernel-la
	cp $(rv_kernel) ./kernel-rv

init_la_dir:
	mkdir -p $(BUILDPATH)/kernel

#定义loongarhc系统镜像路径和名字
la_kernel = $(WORKPATH)/build/loongarch/kernel-la

#使用的磁盘文件，为了方便，两个架构使用同一个
rv_disk_file = ../sdcard-rv.img
# rv_disk_file = tmp/fs.img
#la_disk_file = tmp/fs.img
la_disk_file = ../sdcard-la.img

clean: #删除rv,la的build路径
	rm -rf build/loongarch
	rm -rf build/riscv
	rm -rf user/build

docker_la: #多线程加快速度
	make __docker_la -j

__docker_la: init_la_dir docker_compile_all  #docker_compile_all会先删除build/loongarch再重新编译
	$(LD) $(LDFLAGS) -T $(LD_SCRIPT) -o $(la_kernel) $(la_objs) 
	@echo "__________________________"
	@echo "-------- 生成成功 --------"
	
#参考bakaos的启动选项，dokcer_la_qemu启动有问题，磁盘读写不正常
virt: 
	@echo "la_disk_file = $(la_disk_file)"
	@echo "__________________________"
	qemu-system-loongarch64 \
	-kernel build/loongarch/kernel-la \
	-m 1G -nographic -smp 1 \
				-drive file=$(la_disk_file),if=none,format=raw,id=x0  \
                -device virtio-blk-pci,drive=x0 -no-reboot  \
				-device virtio-net-pci,netdev=net0 \
                -netdev user,id=net0,hostfwd=tcp::5555-:5555,hostfwd=udp::5555-:5555   \
				-rtc base=utc \
	-s -S
#                -rtc base=utc
#-drive file=disk.img,if=none,format=raw,id=x1 -device virtio-blk-pci,drive=x1 \

run:
	qemu-system-loongarch64 \
	-kernel build/loongarch/kernel-la \
	-M virt -cpu la464 \
	-m 1G -nographic -smp 2 \
				-drive file=$(la_disk_file),if=none,format=raw,id=x0  \
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
export RISCV_CFLAGS += -DDEBUG=0
export RISCV_LDFLAGS = -z max-page-size=4096

export RISCV_CFLAGS += -DRISCV=1 #宏

RISCV_LD_SCRIPT =hal/riscv/ld.script

#riscv的所有.c和.S文件
rv_srcs = $(rv_hal_srcs) $(kernel_srcs) $(hsai_srcs) $(rv_driver_srcs)
rv_src_names = $(notdir $(rv_srcs))
rv_c_objs = $(patsubst %.c,$(RISCV_BUILDPATH)/kernel/%.o,$(rv_src_names)) #先替换c
rv_objs = $(patsubst %.S,$(RISCV_BUILDPATH)/kernel/%.o,$(rv_c_objs)) #再替换S,获得所有目标文件路径

#只有sbi的版本了
clean_rv:
	rm -rf build/riscv
#user应该不用清理

init_rv_dir: #为各模块创建好目录
	mkdir -p build/riscv/kernel
	
#定义loongarhc系统镜像路径和名字
rv_kernel = $(RISCV_BUILDPATH)/kernel-rv	

load_riscv_kernel: $(RISCV_LD_SCRIPT) $(rv_objs)
	$(RISCV_LD) $(RISCV_LDFLAGS) -T $(RISCV_LD_SCRIPT) -o $(rv_kernel) $(rv_objs)
	

QEMUOPTS = -machine virt -bios none -kernel build/riscv/kernel-rv -m 1G -smp 1 -nographic
QEMUOPTS += -drive file=$(rv_disk_file),if=none,format=raw,id=x0
QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
QEMUOPTS += -rtc base=utc
#QEMUOPTS += -d guest_errors,unimp,in_asm -D /home/ly/Desktop/os2025/qemu.log
QEMUOPTS += -s -S

rv_qemu: #评测docker运行riscv qemu,本机也可以 调试后缀 ：-gdb tcp::1235  -S
	qemu-system-riscv64 $(QEMUOPTS)

sbi: #多线程加快速度
	make __sbi -j

#编译使用open-sbi的riscv内核。区别是内核起始段变为0x80200000和不调用start函数。为了兼容，编译时宏跳过start函数体的内容
__sbi: clean_rv init_rv_dir sbi_compile_riscv 
	$(RISCV_LD) $(RISCV_LDFLAGS) -T $(SBI_RISCV_LD_SCRIPT) -o $(rv_kernel) $(rv_objs)
	@echo "__________________________"
	@echo "-------- 生成成功 --------"

sbi_compile_riscv:
	$(MAKE) riscv -C user/riscv  
#让hal层编译start.c时传入宏sbi
	$(MAKE) riscv -C hal/riscv SBI=1
	$(MAKE) riscv -C kernel SBI=1
	$(MAKE) riscv -C hsai SBI=1

SBI_RISCV_LD_SCRIPT =hal/riscv/sbi_ld.script

sbi_load_riscv_kernel: $(SBI_RISCV_LD_SCRIPT) $(rv_objs)
	$(RISCV_LD) $(RISCV_LDFLAGS) -T $(SBI_RISCV_LD_SCRIPT) -o $(rv_kernel) $(rv_objs)


sbi_QEMUOPTS = -machine virt -bios default -kernel build/riscv/kernel-rv -m 1G -smp 1 -nographic
sbi_QEMUOPTS += -drive file=$(rv_disk_file),if=none,format=raw,id=x0
sbi_QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0
sbi_QEMUOPTS += -rtc base=utc
#sbi_QEMUOPTS += -d guest_errors,unimp,in_asm -D /home/ly/Desktop/os2025/qemu.log
sbi_QEMUOPTS += -s -S

sbi_qemu: #初赛，使用opensbi
	@echo "rv_disk_file = $(rv_disk_file)"
	@echo "__________________________"
	qemu-system-riscv64 $(sbi_QEMUOPTS)

	
#不调试，直接运行
run_sbi:
	qemu-system-riscv64 \
		-machine virt -bios default \
		-kernel build/riscv/kernel-rv \
		-m 1G -smp 3 -nographic \
		-drive file=$(rv_disk_file),if=none,format=raw,id=x0 -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

#写Makefile时使用，查看要编译的源文件
show:
	@echo $(rv_hal_srcs)

#编译并把initcode反汇编
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

# 磁盘镜像文件
FS_IMG = tmp/fs.img

# 镜像大小，单位M
FS_SIZE_MB = 256

# 临时挂载点
MOUNT_DIR = /tmp/fs_mount_dir

.PHONY: make_fs_img

make_fs_img:
	@echo "==> 创建空白镜像文件 $(FS_IMG) 大小 $(FS_SIZE_MB)M"
	@sudo dd if=/dev/zero of=$(FS_IMG) bs=1M count=$(FS_SIZE_MB) status=progress
	@echo "==> 格式化为 ext4 文件系统"
	@sudo mkfs.ext4 -F -b 4096 -O ^has_journal -N 16000 $(FS_IMG)
	@sudo tune2fs -m 0 $(FS_IMG)  # 关闭5%预留空间
	@echo "==> 创建临时挂载点 $(MOUNT_DIR)"
	@sudo mkdir -p $(MOUNT_DIR)
	@echo "==> 挂载镜像"
	@sudo mount -o loop $(FS_IMG) $(MOUNT_DIR)
	@echo "==> 复制 $(riscv_disk_file) 到镜像根目录"
	@sudo cp tmp/cases/* $(MOUNT_DIR)/
	@echo "==> 同步数据"
	@sync
	@echo "==> 卸载镜像"
	@sudo umount $(MOUNT_DIR)
	@echo "==> 清理挂载点"
	@sudo rmdir $(MOUNT_DIR)
	@echo "==> 完成 $(FS_IMG) 包含 tmp/cases/* 文件"
	
