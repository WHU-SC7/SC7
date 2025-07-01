

/*hal提供给hsai的函数*/

/*中断和异常*/
//必须提供这四个函数的地址
extern char trampoline[]; ///< trampoline 代码段的起始地址。hal要求trampoline有两个函数,uservec在前，userret在后
extern char uservec[];    ///< trampoline 用户态异常入口，
extern char userret[];    ///< trampoline 进入用户态。

extern void kernelvec();  ///< 内核态中断/异常入口

/*进程切换*/
//要求提供struct context的定义，建议定义在hal层
extern void swtch(struct context *idle, struct context *p);

/*输出设备*/
//至少提供一个uart设备
void uart_init(); //初始化uart
int put_char_sync( char c ); //输出一个字符. 如果uart串口忙，等待uart输出完当前字符，再发送char c给uart
void uartputc(int c);

/*控制状态寄存器*/
//开关中断功能

//准备进入用户态功能

/*时钟，待抽象*/
//初始化时钟中断

//设置下一个时钟中断


/*平台特定功能，只有loongarch有*/
//tlb重填函数
extern void handle_tlbr();
//merr处理函数
extern void handle_merr();

/*设备管理。包括设备驱动和设备树解析，这个在考虑怎么处理*/
