#include "core.h"
#include "tlibc_print.h"
#include "tlibc.h"

/**
 * @brief 格式化显示getdents64的内容，没有错误处理
 */
void print_getdents64_buf(struct linux_dirent64 *buf) //要求buf无数据部分是全0
{
    struct linux_dirent64 *data = buf;
    PRINT_COLOR(BRIGHT_CYAN_COLOR_PINRT, "off\tinode\ttype\tname\t\n");
    while (data->d_off != 0) //< 检查不严谨，但是考虑到每次list_file会清空ls_buf为0,这样是可以的
    {
        // printf("%d\t%d\t%d\t%s\n",data->d_off,data->d_ino,data->d_type,data->d_name);
        __printf("%d\t", data->d_off);
        __printf("%d\t", data->d_ino);
        switch (data->d_type)
        {
        case DT_DIR: //< 目录，蓝色
            PRINT_COLOR(BLUE_COLOR_PRINT, "DIR\t");
            PRINT_COLOR(BLUE_COLOR_PRINT, "%s\t", data->d_name);
            break;
        case DT_REG: //< 普通文件，白色
            __printf("FILE\t");
            __printf("%s\t", data->d_name);
            break;
        case DT_CHR: //< 字符设备，如console，黄色
            PRINT_COLOR(YELLOW_COLOR_PRINT, "CHA\t");
            PRINT_COLOR(YELLOW_COLOR_PRINT, "%s\t", data->d_name);
            break;
        case DT_BLK: //< 块设备，黄色
            PRINT_COLOR(YELLOW_COLOR_PRINT, "BLK\t");
            PRINT_COLOR(YELLOW_COLOR_PRINT, "%s\t", data->d_name);
            break;
        case DT_LNK: //< 符号链接，
            PRINT_COLOR(GREEN_COLOR_PRINT, "LNK\t");
            PRINT_COLOR(GREEN_COLOR_PRINT, "%s\t", data->d_name);
            break;
        default: //< 未知，红色
            PRINT_COLOR(RED_COLOR_PRINT, "%d\t", data->d_type);
            PRINT_COLOR(RED_COLOR_PRINT, "%s\t", data->d_name);
            break;
        }
        __printf("\n");
        // char *s=(char*)data; //<调试时逐个字节显示
        // for(int i=0;i<data->d_reclen;i++)
        // {
        //     printf("%d ",*s++);
        // }
        // printf("\n");
        data = (struct linux_dirent64 *)((char *)data + data->d_reclen); //< 遍历
    }
}

#define LS_BUF_SIZE 4096 //缓冲区大小
void ls(int argc, char *argv[])
{
    int open_ret;
    if(argc == 1) //没有给参数
    {
        open_ret = __openat(AT_FDCWD,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC,0644);
    }
    if(argc == 2) //一个参数
    {
        char *path = argv[1];
        __printf("参数: %s\n",path);
        open_ret = __openat(AT_FDCWD,path,O_RDONLY|O_DIRECTORY|O_CLOEXEC,0644);
    }
    if(argc > 2)
    {
        __printf("参数超过两个，太多了\n");
        return;
    }
    char getdent_buf[LS_BUF_SIZE];
    for(int i=0;i<LS_BUF_SIZE;i++) //必须先清零
        getdent_buf[i]=0;
    if(open_ret < 0)
        panic("打开失败\n");
    __getdents64(open_ret,(struct linux_dirent64 *)getdent_buf, LS_BUF_SIZE);
    __close(open_ret);
    print_getdents64_buf((struct linux_dirent64 *)getdent_buf);
}

void touch(int argc, char *argv[])
{
    //先认为参数正确
    if(argc == 1)
    {
        __printf("缺少参数\n");
        return;
    }
    if(argc >2)
    {
        __printf("参数超过两个，太多了\n");
        return;
    }
    char *path = argv[1];
    int open_ret = __creat(path,0644);
    __close(open_ret);
    // __printf("openat返回值: %d\n",open_ret);
}

char *command_table[] = {
    "ls",
    "touch",
    "cat"
};
#define COMMAND_MAX_LEN 16 //命令的最大长度
#define COMMAND_NUM sizeof(command_table) / sizeof(command_table[0]) //命令的个数

#define MAX_ARGS 16
struct command{
    char *name;             //命令名，如ls
    char *args[MAX_ARGS];   //参数列表
    int argc;               //参数个数
};
/**
 * @brief 解析输入.破坏性解析，会改变input的某些' '为0
 * @return 返回0表示正常解析，返回负数表示解析错误，不同负数对应不同错误
 */
int parse_cmd(const char *input, struct command *command)
{
    //解析命令名
    char *str_start = (char *)input;
    char *str_end = (char *)input;
    if(!*str_start) //input是空字符串
        return -1;
    while(*str_end!=' '&& *str_end)
    {
        str_end++;
    }
    int command_len = str_end - str_start;
    if(command_len>COMMAND_MAX_LEN) //命令名过长
    {
        return -2;
    }
    command->name = str_start; //命令名正常
    command->args[0] = str_start; //第0个参数是命令名自身
    command->argc=1;

    //开始解析命令参数
    while(*str_end == ' ') //跳过所有的空格
        str_end++;
    if(!*str_end)//命令名后面没有带参数 
    {
        return 0;
    }
    //有参数，开始解析
    char *insert_ptr = str_start;//把命令名末尾的第一个空格改成0
    while(*insert_ptr != ' ')
        insert_ptr++;
    *insert_ptr = 0; //现在command->name指向的是以0结尾的字符串

    str_start = str_end;//已经跳过了所有空格，现在两个指针都指向第1个参数起始
    while(*str_start) //每一轮解析一个参数
    {
        command->args[command->argc] = str_start;
        command->argc++;
        while(*str_end!=' '&& *str_end)
        {
            str_end++;
        }
        // arg_len = str_end - str_start; //可以计算参数长度
        while(*str_end == ' ') //跳过所有空格
            str_end++;
        if(!*str_end)//这个参数之后读取到字符串末尾，结束
            return 0;
        else //还有后续参数
        {
            char *insert_ptr = str_start;//把这个参数末尾的第一个空格改成0
            while(*insert_ptr != ' ')
                insert_ptr++;
            *insert_ptr = 0; //现在command->name指向的是以0结尾的字符串
            str_start = str_end;
        }
    }
    return -3; //未知情况执行到末尾
}
void show_cmd_info(struct command *command)//显示struct command的信息
{
    __printf("命令名: %s, 命令个数: %d\n",command->name,command->argc);
    for(int i=0; i<command->argc; i++)
    {
        __printf("第%d个参数: %s\n",i,command->args[i]);
    }
}

/**
 * @brief 在命令表中匹配命令
 * @return 匹配的命令在表中的索引，失败返回-1
 */
int search_command(const char *input_str)
{
    for(int i=0; i<COMMAND_NUM; i++)//依次匹配表中所有命令名
    {
        char *command = command_table[i];
        //字符串匹配
        char *ptr = (char *)input_str;
        while(1)
        {
            if((*ptr == 0) && (*command == 0)) // input_str匹配到末尾，command也匹配到末尾，认为匹配上了
                return i;
            if(*ptr == *command) // 当前字符匹配成功
            {
                ptr++;
                command++;
            }
            else //当前字符不匹配
            {
                break;
            }
        }        
    }
    return -1;
}

/**
 * @brief 根据给定的index执行命令,并传入args和argc
 */
void run_command(int index, struct command *command)
{
    // show_cmd_info(command);
    if(index==0)
    {
        //之后考虑用fork,execve,现在就直接执行
        ls(command->argc,command->args);
    }
    else if(index==1)
    {
        touch(command->argc,command->args);
    }
    else
    {
        __printf("让我们假装执行了命令%s\n",command_table[index]);
        __printf("执行成功\n");
    }
}

/**
 * @brief shell,现在只能接收输入
 */
void shell()
{
    LOG("欢迎使用Tlibc Shell!\n");
    LOG("这是第一个版本, 只能输入输出, 按q退出shell\n");
    LOG("不要输入方向键好吗，这个版本不支持\n");

    while(1)
    {
        // 单字符输入，简单但是不标准。每个字符读取都要陷入内核，开销大
        // char input_c;
        // __read(0,&input_c,1);
        // if(input_c == 'q')
        //     break;
        // __printf("接收到输入，字符的码值: %d\n",input_c);

        // 一次读取完整的输入，以enter输入结尾
        // 疑问，内核返回的缓冲区是否应该以enter的码值结尾。 现在SC7不会
        char buf[256];
        for(int i=0; i<256; i++) //每次都清空缓冲区，防止未定义行为
            buf[i] = 0;
        int read_count = __read(0,buf,256); //读取一次输入
        if(read_count < 0)
        {
            panic("读取错误!\n");
        }
        if(buf[0]=='q' && buf[1]==0)    //输入是单字符就退出
            break;
        if(buf[0]==0)
        {
            continue;
        }
        // __printf("输入: %s, read返回值: %d\n",buf,read_count);

        //解析命令输入，获取命令名和参数
        struct command command;//在栈上分配空间给struct command
        char *ptr = (char *)&command; //清零，否则会异常
        for(int i=0; i<sizeof(struct command); i++)
            ptr[i]=0;
        int ret = parse_cmd(buf,&command);
        if(ret == 0) //解析成功，开始执行
        {
            // show_cmd_info(&command);
            ret = search_command(command.name);
            if(ret != -1)
            {
                __printf("匹配到命令: %s,开始执行\n",command_table[ret]);
                run_command(ret,&command);
            }
            else
            {
                __printf("没有找到输入的命令:%s\n",buf);
            }
            continue;
        }
        else //解析失败
        {
            __printf("解析命令出错!, 错误码: %d\n",ret);
        }
    }
    tlibc_shutdown();
}