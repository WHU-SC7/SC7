qemu-system-loongarch64 \
	-kernel build/loongarch/kernel-la \
	-m 1G -nographic -smp 1 \
				-drive file=tmp/test_echo_la,if=none,format=raw,id=x0  \
                -device virtio-blk-pci,drive=x0 -no-reboot  \
				-device virtio-net-pci,netdev=net0 \
                -netdev user,id=net0,hostfwd=tcp::5555-:5555,hostfwd=udp::5555-:555 \
				-s -S
#/home/lu/oscomp/qemu-system-loongarch/bin/