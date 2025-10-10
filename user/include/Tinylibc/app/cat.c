#include "core.h"
#include "tlibc_print.h"
#include "tlibc.h"

void cat(int argc, char *argv[])
{
    if(argc != 2)
    {
        __printf("错误，需要一个参数!\n");
        return;
    }
    //获取大小然后输出
    if(*argv[1] == 0)
    {
        __printf("错误，传入空字符串!");
        return;
    }
    unsigned long cat_fd = __openat(AT_FDCWD,argv[1],O_RDWR,0644);
    if(cat_fd < 0)
    {
        __printf("错误,打开文件%s失败\n",argv[1]);
        return;
    }
    struct stat statbuf;
    char *ptr = (char *)&statbuf;
    for(int i=0; i<sizeof(struct stat); i++)
    {
        ptr[i] = 0;
    }
    int ret = fstat(cat_fd,&statbuf);
    unsigned long file_size = statbuf.st_size;
    if(ret != 0)
        panic("错误,fstat失败,返回值: %d\n", ret);
    __printf("fstat获取到文件大小: %d\n", file_size);
#define CAT_MAX_LEN 1024
    if(file_size > CAT_MAX_LEN)
    {
        __printf("文件内容大于%d, 将不显示文件内容\n", CAT_MAX_LEN);
    }
    char cat_buf[CAT_MAX_LEN];
    ret = __read(cat_fd, cat_buf, file_size); //根据长度读取文件内容然后输出
    __write(1,cat_buf,file_size);
}