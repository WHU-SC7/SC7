#ifndef __STRING_H__
#define __STRING_H__

#include "def.h"

// 辅助函数实现
int atoi(const char *str)
{
    int result = 0;
    int sign = 1;

    // 跳过前导空格
    while (*str == ' ' || *str == '\t')
    {
        str++;
    }

    // 处理符号
    if (*str == '-')
    {
        sign = -1;
        str++;
    }
    else if (*str == '+')
    {
        str++;
    }

    // 转换数字
    while (*str >= '0' && *str <= '9')
    {
        result = result * 10 + (*str - '0');
        str++;
    }

    return sign * result;
}

void *memset(void *dst, int c, uint n)
{
	char *cdst = (char *)dst;
	int i;
	for (i = 0; i < n; i++) {
		cdst[i] = c;
	}
	return dst;
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && *s2 && *s1 == *s2)
    {
        s1++;
        s2++;
    }
    return (unsigned char)*s1 - (unsigned char)*s2;
}

char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    while (*s)
    {
        if (*s == (char)c)
            last = s;
        s++;
    }
    // 检查 c 是否是空字符本身
    if (c == '\0')
        return (char *)s; // s 当前指向空终止符
    return (char *)last;
}

char *strchr(const char *s, int c)
{
    while (*s != '\0')
    {
        if (*s == c)
        {
            return (char *)s;
        }
        s++;
    }
    if (c == '\0')
    {
        return (char *)s;
    }
    return NULL;
}

char *strcpy(char *dest, const char *src)
{
    char *d = dest;
    while (*src)
    {
        *d = *src;
        d++;
        src++;
    }
    *d = '\0';
    return dest;
}

char *strncpy(char *s, const char *t, int n)
{
    char *os;

    os = s;
    while (n-- > 0 && (*s++ = *t++) != 0)
        ;
    while (n-- > 0)
        *s++ = 0;
    return os;
}



char *strcat(char *dest, const char *src)
{
    char *d = dest;
    while (*d)
    {
        d++;
    }
    while (*src)
    {
        *d = *src;
        d++;
        src++;
    }
    *d = '\0';
    return dest;
}

char* strstr(const char* p1, const char* p2)
{
    // p1,p2不要往后动
    // 需要一个变量记录从哪个位置开始匹配
    //char* s1 = p1;	// 这里赋值无所谓，就给NULL好了
    char* s1 = NULL;
    char* s2 = NULL;
    char* current = (char*)p1;	// 这里强制类型转换，因为p1是const修饰，赋值给了char*这个没有保护的，所以强转下，不然会报警告
    // 如果p2是空字符串，那就比不了
    if (*p2 == '\0')
    {
    	return (char*)p1;
    }
    // 真正的查找实现
    while (*current) // 判断*current是'\0'吗？不是就可以查找
    {
        s1 = current;
        s2 = (char*)p2;
    
        while ((*s1 != '\0') && (*s2 != '\0') && (*s1 == *s2))
        {
            s1++;
            s2++;
        }
        if (*s2 == '\0')
        {
            // 说明匹配到了
            return current;	// 返回子串地址
        }
        if (*s1 == '\0') 
        {
            // 如果子串比较长，那么肯定是找不到的
            return NULL;
        }
        current++;	// 不等于，那么current往后偏移
    }
    return NULL;	//找不到子串
}


int strlen(const char *s)
{
    const char *a = s;
    typedef size_t __attribute__((__may_alias__)) word;
    const word *w;
    for (; (uint64)s % SS; s++)
        if (!*s)
            return s - a;
    for (w = (const void *)s; !HASZERO(*w); w++)
        ;
    s = (const void *)w;
    for (; *s; s++)
        ;
    return s - a;
}

int _strlen(const char *s)
{
    int n;

    for (n = 0; s[n]; n++)
        ;
    return n;
}

void *memmove(void *dst, const void *src, uint n)
{
    const char *s;
    char *d;

    s = src;
    d = dst;
    if (s < d && s + n > d)
    {
        // 目标区域与源区域重叠，并且目标区域在源区域之后。
        // 从后往前复制，以避免在读取源数据之前覆盖它。
        s += n;
        d += n;
        while (n-- > 0)
            *--d = *--s;
    }
    else
    {
        // 没有重叠，或者目标区域在源区域之前。
        // 从前往后复制。
        while (n-- > 0)
            *d++ = *s++;
    }

    return dst;
}


/**
 * @brief 复制一块内存区域。
 *
 * 从内存区域 `src` 复制 `n` 个字节到内存区域 `dst`。
 * 此实现直接调用 `memmove` 以确保即使在区域重叠时也能安全操作。
 * 在标准C中，`memcpy` 通常假定区域不重叠以进行潜在优化。
 *
 * @param dst 指向目标内存块的指针。
 * @param src 指向源内存块的指针。
 * @param n 要复制的字节数。
 * @return 指向目标内存块 `dst` 的指针。
 */
void *memcpy(void *dst, const void *src, uint n)
{
    return memmove(dst, src, n);
}

char *strtok(char *str, const char *delim)
{
    static char *last_str = NULL;
    char *token_start;
    // char *token_end;
    
    // 如果传入的字符串不为 NULL，则开始新的分割
    if (str != NULL)
    {
        last_str = str;
    }
    else if (last_str == NULL)
    {
        return NULL;
    }
    
    // 跳过前导的分隔符
    while (*last_str != '\0')
    {
        const char *d = delim;
        int is_delim = 0;
        
        while (*d != '\0')
        {
            if (*last_str == *d)
            {
                is_delim = 1;
                break;
            }
            d++;
        }
        
        if (!is_delim)
        {
            break;
        }
        last_str++;
    }
    
    // 如果到达字符串末尾，返回 NULL
    if (*last_str == '\0')
    {
        last_str = NULL;
        return NULL;
    }
    
    // 找到当前 token 的起始位置
    token_start = last_str;
    
    // 找到当前 token 的结束位置
    while (*last_str != '\0')
    {
        const char *d = delim;
        int is_delim = 0;
        
        while (*d != '\0')
        {
            if (*last_str == *d)
            {
                is_delim = 1;
                break;
            }
            d++;
        }
        
        if (is_delim)
        {
            break;
        }
        last_str++;
    }
    
    // 如果找到了分隔符，将其替换为字符串结束符
    if (*last_str != '\0')
    {
        *last_str = '\0';
        last_str++;
    }
    else
    {
        // 到达字符串末尾
        last_str = NULL;
    }
    
    return token_start;
}







#endif
