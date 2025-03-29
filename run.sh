# 通常情况下该文件应当放在项目的根目录下

RUNENV_PREFIX=/opt/qemu-bin-9.2.1
KERNEL_PREFIX=`pwd`

cd $RUNENV_PREFIX

./bin/qemu-system-loongarch64 \
	-M virt\
	-serial stdio \
	-k ./share/qemu/keymaps/en-us \
	-kernel ${KERNEL_PREFIX}/build/loongarch/kernel-la \
	-serial vc \
	-m 1G \
	-net nic \
	-net user,net=10.0.2.0/24,tftp=/srv/tftp \
	-vnc :0 \
	-hda ${KERNEL_PREFIX}/tmp/disk \
	-hdb /home/image/2025/sdcard-la.img