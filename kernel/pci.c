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

#define PCIE0_ECAM_V        0x20000000 //< pci.h

//#define PCI_ADDR(addr)  (PCIE0_ECAM + (uint64)(addr))
#define PCI_ADDR_V(addr) (PCIE0_ECAM_V + (uint64)(addr))
#define PCI_REG8(reg)  (*(volatile uint8 *)PCI_ADDR_V(reg))
#define PCI_REG16(reg)  (*(volatile uint16 *)PCI_ADDR_V(reg))
#define PCI_REG32(reg)  (*(volatile uint32 *)PCI_ADDR_V(reg))
#define PCI_REG64(reg)  (*(volatile uint64 *)PCI_ADDR_V(reg))

void pci_config_read(void *buf, uint64 len, uint64 offset) //< virtio_pci_read_caps()使用
{
	volatile uint8 *dst = (volatile uint8 *)buf;
	while (len) {
		*dst = PCI_REG8(offset);
		--len;
		++dst;
		++offset;
	}
}

uint8 pci_config_read8(uint64 offset)
{
	return PCI_REG8(offset);
}

uint16 pci_config_read16(uint64 offset)
{
	return PCI_REG16(offset);
}

uint32 pci_config_read32(uint64 offset)
{
	return PCI_REG32(offset);
}

uint64 pci_config_read64(uint64 offset)
{
	return PCI_REG64(offset);
}

void pci_config_write8(uint32 offset, uint8 val)
{
	PCI_REG8(offset) = val;
}

void pci_config_write16(uint32 offset, uint16 val)
{
	PCI_REG16(offset) = val;
}

void pci_config_write32(uint32 offset, uint32 val)
{
	PCI_REG32(offset) = val;
}

/*pci.h,之后放到头文件去*/
#define PCIE0_MMIO          (0x40000000 | DMWIN_MASK)
#define PCIE0_MMIO_V          0x40000000

#define PAGE_ALIGN(sz)	((((uint64)sz) + 0x0fff) & ~0x0fff)
uint64 pci_alloc_mmio(uint64 sz)
{
	static uint64 s_off = 0;           // static 变量, 共用一个
	// MMIO 区域 + s_off
	// uint64 addr = PHYSTOP + s_off + 2 * PGSIZE;
	uint64 addr = PCIE0_MMIO_V + s_off;
	s_off += PAGE_ALIGN(sz);        // 按页 4k 对齐
	// printf("addr: 0x%016llx, 0x%016llx\n", addr, s_off);
	return addr;
}