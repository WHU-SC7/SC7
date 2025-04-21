#include "types.h"
#include "print.h"

//< 在virtio_disk中
extern unsigned char bus1;
extern unsigned char device1;
extern unsigned char function1;

#define DMWIN1_MASK 0x8000000000000000//< memlayout.h 直接映射窗口1
#define PCIE0_ECAM          (0x20000000 | DMWIN1_MASK)//< pci.h

#define PCI_ADDR(addr)  (PCIE0_ECAM + (uint64)(addr))//< pci.c
// #define PCI_ADDR_V(addr) (PCIE0_ECAM_V + (uint64)(addr))
// #define PCI_REG8(reg)  (*(volatile uint8 *)PCI_ADDR_V(reg))
// #define PCI_REG16(reg)  (*(volatile uint16 *)PCI_ADDR_V(reg))
// #define PCI_REG32(reg)  (*(volatile uint32 *)PCI_ADDR_V(reg))
// #define PCI_REG64(reg)  (*(volatile uint64 *)PCI_ADDR_V(reg))
uint64 pci_device_probe(uint16 vendor_id, uint16 device_id)
{
    uint32 pci_id = (((uint32)device_id) << 16) | vendor_id;
    uint64 ret = 0;

    for (int bus = 0; bus < 255; bus++) {
        for (int dev = 0; dev < 32; dev++) {
            int func = 0;
            int offset = 0;
            uint64 off = (bus << 16) | (dev << 11) | (func << 8) | (offset);
            volatile uint32 *base = (volatile uint32 *)PCI_ADDR(off);
            uint32 id = base[0];   // device_id + vendor_id

            if (id != pci_id) continue;

            ret = off;

            // command and status register.
            // bit 0 : I/O access enable
            // bit 1 : memory access enable
            // bit 2 : enable mastering
            base[1] = 7;
            __sync_synchronize();
        	if (bus1 == bus && device1 == dev && function1 == func) {
        		continue;
        	} else {
        		if (!bus1 && !device1 && !function1) {
        			bus1 = bus;
        			device1 = dev;
        			function1 = func;
        		}
        	}

        	printf("%d %d %d\n", bus, dev, func);
        	goto out;

        }
    }
	// void* base_addr = PCIE0_MMIO;
	// for (int i = 0;i < 4096;i+=4) {
	// 	printf("%x ", *(volatile uint32 *)((uint64)base_addr + i));
	// }
	// printf("vendor_id: 0x%x\n", vendor_id);
	// printf("device_id: 0x%x\n", device_id);
	// printf("bar_addr : 0x%x\n", ret);          // ECAM 中的 offset
out:
    printf("pci_base: %p\n",ret);
    return ret;
}