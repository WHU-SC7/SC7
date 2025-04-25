#include "types.h"



/*PCI配置空间数据的偏移*/
#define PCI_DEVICE_VENDER								0x00	/* 设备厂商ID和设备ID的寄存器偏移量*/
#define PCI_STATUS_COMMAND								0x04	/*状态和命令寄存器偏移量*/
#define PCI_CLASS_CODE_REVISION_ID						0x08	/*类型、子类型、次类型和修订号寄存器偏移量*/
#define PCI_BIST_HEADER_TYPE_LATENCY_TIMER_CACHE_LINE	0x0C    /*BIST（Built-In Self-Test，内建自测）、头部类型、延迟计时器和缓存行寄存器偏移量。*/
#define PCI_BASS_ADDRESS0								0x10/*（BARs，基地址寄存器），从0～5一共可以存六个地址信息*/
#define PCI_BASS_ADDRESS1								0x14
#define PCI_BASS_ADDRESS2								0x18
#define PCI_BASS_ADDRESS3								0x1C
#define PCI_BASS_ADDRESS4								0x20
#define PCI_BASS_ADDRESS5								0x24
#define PCI_CARD_BUS_POINTER							0x28/*卡总线指针寄存器偏移量*/
#define PCI_SUBSYSTEM_ID							    0x2C/*子系统ID寄存器偏移量*/
#define PCI_EXPANSION_ROM_BASE_ADDR                     0x30/*扩展ROM基地址寄存器偏移量*/
#define PCI_CAPABILITY_LIST                             0x34/*能力列表寄存器偏移量*/
#define PCI_RESERVED							        0x38/*保留寄存器偏移量*/
#define PCI_MAX_LNT_MIN_GNT_IRQ_PIN_IRQ_LINE			0x3C/*最大的总线延迟时间、最小的总线延迟时间、最大的访问延迟时间、中断引脚、中断线寄存器偏移量*/

/*IO地址和MEM地址的地址mask*/
#define PCI_BASE_ADDR_MEM_MASK           (~0x0FUL)//屏蔽低四位
#define PCI_BASE_ADDR_IO_MASK            (~0x03UL)//屏蔽低两位

/*向command中写入的控制位*/
#define  PCI_COMMAND_IO		0x1	/* I/O 空间的响应使能 */
#define  PCI_COMMAND_MEMORY	0x2	/* 内存空间的响应使能 */
#define  PCI_COMMAND_MASTER	0x4	/* 总线主控制使能 */
#define  PCI_COMMAND_SPECIAL	0x8	/* 特殊事务响应使能 */
#define  PCI_COMMAND_INVALIDATE	0x10	/* 使用内存写入并且使其失效 */
#define  PCI_COMMAND_VGA_PALETTE 0x20	/* 启用VGA调色板的监听 */
#define  PCI_COMMAND_PARITY	0x40	/* 启用奇偶校验检查 */
#define  PCI_COMMAND_WAIT	0x80	/* 启用地址/数据步进 */
#define  PCI_COMMAND_SERR	0x100	/* 启用系统错误（SERR） */
#define  PCI_COMMAND_FAST_BACK	0x200	/* 启用快速回写 */
#define  PCI_COMMAND_INTX_DISABLE 0x400 /* INTx模拟禁用 */







void pci_config_read(void *buf, uint64 len, uint64 offset);

uint8 pci_config_read8(uint64 offset);
uint16 pci_config_read16(uint64 offset);
uint32 pci_config_read32(uint64 offset);
uint64 pci_config_read64(uint64 offset);

void pci_config_write8(uint32 offset, uint8 val);
void pci_config_write16(uint32 offset, uint16 val);
void pci_config_write32(uint32 offset, uint32 val);

uint64 pci_alloc_mmio(uint64 sz);