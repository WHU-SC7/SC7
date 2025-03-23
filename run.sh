# 通常情况下该文件应当放在项目的根目录下

RUNENV_PREFIX=/home/lu/Downloads/qemu/
KERNEL_PREFIX=`pwd`

cd $RUNENV_PREFIX

./bin/qemu-system-loongarch64 \
	-M ls2k \
	-serial stdio \
	-k ./share/qemu/keymaps/en-us \
	-kernel ${KERNEL_PREFIX}/build/loongarch/kernel.elf \
	-serial vc \
	-m 1G \
	-net nic \
	-net user,net=10.0.2.0/24,tftp=/srv/tftp \
	-vnc :0 \
	-hda ${KERNEL_PREFIX}/tmp/disk \
	-hdb ~/Desktop/sdcard-loongarch.img #sdcard-la.img