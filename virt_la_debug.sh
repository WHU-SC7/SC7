qemu-system-loongarch64 \
	-M virt \
	-serial stdio \
	-smp 1 \
	-kernel build/loongarch/kernel-la \
	-m 1G \
	-display none \
    -drive file=tmp/fs.img,if=none,format=raw,id=x0  \
    -device virtio-blk-pci,drive=x0,bus=pcie.0 \
	-s -S
    #-device virtio-blk-pci,drive=x0,bus=virtio-mmio-bus.0 \

#/home/lu/oscomp/qemu-system-loongarch/bin/