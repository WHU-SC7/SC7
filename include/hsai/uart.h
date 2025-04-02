#ifndef __UART_H__
#define __UART_H__

void uart_init(); //初始化uart
int put_char_sync( char c ); //输出一个字符. 如果uart串口忙，等待uart输出完当前字符，再发送char c给uart

#endif