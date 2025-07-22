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



#endif
