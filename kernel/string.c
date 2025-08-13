#include "string.h"
#include "types.h"

/**
 * @brief 使用指定字节值填充一块内存区域。
 *
 * 将 `dst` 指向的内存块的前 `n` 个字节设置为指定的值 `c`
 * (`c` 会被解释为一个无符号字符)。
 *
 * @param dst 指向目标内存块的指针。
 * @param c 要设置的值（将被解释为无符号字符）。
 * @param n 要设置的字节数。
 * @return 指向目标内存块 `dst` 的指针。
 */
void *memset(void *dst, int c, uint n)
{
    char *cdst = (char *)dst;
    uint i;
    for (i = 0; i < n; i++)
    {
        cdst[i] = c;
    }
    return dst;
}

/**
 * @brief 比较两块内存区域。
 *
 * 比较 `v1` 和 `v2` 指向的内存区域的前 `n` 个字节。
 *
 * @param v1 指向第一个内存块的指针。
 * @param v2 指向第二个内存块的指针。
 * @param n 要比较的字节数。
 * @return 如果 `v1` 的前 `n` 个字节小于、等于或大于 `v2` 的前 `n` 个字节，
 *         则分别返回小于、等于或大于零的整数。
 */
int memcmp(const void *v1, const void *v2, uint n)
{
    const uchar *s1, *s2;

    s1 = v1;
    s2 = v2;
    while (n-- > 0)
    {
        if (*s1 != *s2)
            return *s1 - *s2;
        s1++, s2++;
    }

    return 0;
}

/**
 * @brief 移动一块内存区域，正确处理重叠区域。
 *
 * 从内存区域 `src` 复制 `n` 个字节到内存区域 `dst`。
 * 内存区域可能重叠：`memmove` 确保在覆盖之前正确复制 `src` 的原始字节。
 *
 * @param dst 指向目标内存块的指针。
 * @param src 指向源内存块的指针。
 * @param n 要移动的字节数。
 * @return 指向目标内存块 `dst` 的指针。
 */
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

/**
 * @brief 比较两个字符串的前 `n` 个字符。
 *
 * 按字典顺序比较以空字符结尾的字符串 `p` 和 `q`。
 * 最多比较 `n` 个字符。空字符之后的字符不参与比较。
 *
 * @param p 指向第一个字符串的指针。
 * @param q 指向第二个字符串的指针。
 * @param n 要比较的最大字符数。
 * @return 如果 `p` 的前 `n` 个字符小于、等于或大于 `q` 的前 `n` 个字符，
 *         则分别返回小于、等于或大于零的整数。
 */
int strncmp(const char *p, const char *q, uint n)
{
    while (n > 0 && *p && *p == *q)
        n--, p++, q++;
    if (n == 0)
        return 0;
    return (uchar)*p - (uchar)*q;
}

/**
 * @brief 从源字符串 `t` 复制最多 `n` 个字符到目标字符串 `s`。
 *
 * 从以空字符结尾的字符串 `t` 复制最多 `n` 个字节到 `s`。
 * 如果 `t` 的长度小于 `n` 个字节，`s` 的其余部分将用空字节填充。
 * 如果 `t` 的长度等于或大于 `n` 个字节，目标字符串 `s` 将不会以空字符结尾。
 *
 * @param s 指向目标字符串缓冲区的指针。
 * @param t 指向源字符串的指针。
 * @param n 要复制的最大字符数。
 * @return 指向目标字符串 `s` 的指针。
 */
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

/**
 * @brief 安全地从源字符串 `t` 复制最多 `n-1` 个字符到目标字符串 `s`，并确保空字符结尾。
 *
 * 从以空字符结尾的字符串 `t` 复制最多 `n-1` 个字符到 `s`，
 * 并始终在 `s` 中以空字符结尾（前提是 `n > 0`）。
 * 这可以防止缓冲区溢出并确保字符串的有效终止。
 *
 * @param s 指向目标字符串缓冲区的指针。
 * @param t 指向源字符串的指针。
 * @param n 目标缓冲区 `s` 的大小（包括空终止符的空间）。
 * @return 指向目标字符串 `s` 的指针。
 */
char *safestrcpy(char *s, const char *t, int n)
{
    char *os;

    os = s;
    if (n <= 0)
        return os;
    while (--n > 0 && (*s++ = *t++) != 0)
        ;
    *s = 0; // 确保空字符结尾
    return os;
}

/**
 * @brief 计算以空字符结尾的字符串的长度。
 *
 * 计算字符串 `s` 中直到（但不包括）第一个空字符的字符数。
 *
 * @param s 指向字符串的指针。
 * @return 字符串中的字符数，不包括空终止符。
 */
int strlen(const char *s)
{
    int n;

    for (n = 0; s[n]; n++)
        ;
    return n;
}

/**
 * @brief 一个不做任何操作的空函数。
 *
 * 此函数作为占位符，不执行任何操作。
 * 它可以接受一个整数和可变参数，但它们都未被使用。
 *
 * @param _ 一个整数参数（未使用）。
 * @param ... 可变参数（未使用）。
 * @return 此函数返回 void。
 */
void dummy(int _, ...)
{
}

/**
 * @brief 将以空字符结尾的字符串从源 `t` 复制到目标 `s`。
 *
 * 将以空字符结尾的字符串 `t` 复制到 `s`，包括空字节。
 * 目标字符串 `s` 必须足够大以容纳副本。
 * 此函数不执行任何边界检查。
 *
 * @param s 指向目标字符串的指针。
 * @param t 指向源字符串的指针。
 * @return 指向目标字符串 `s` 的指针。
 */
char *
strcpy(char *s, const char *t)
{
    char *os;

    os = s;
    while ((*s++ = *t++) != 0)
        ;
    return os;
}

/**
 * @brief 比较两个以空字符结尾的字符串。
 *
 * 按字典顺序比较以空字符结尾的字符串 `p` 和 `q`。
 *
 * @param p 指向第一个字符串的指针。
 * @param q 指向第二个字符串的指针。
 * @return 如果 `p` 小于、等于或大于 `q`，则分别返回小于、等于或大于零的整数。
 */
int strcmp(const char *p, const char *q)
{
    while (*p && *p == *q)
        p++, q++;
    return (uchar)*p - (uchar)*q;
}

/**
 * @brief 将源字符串追加到目标字符串的末尾。
 *
 * 将以空字符结尾的字符串 `src` 追加到以空字符结尾的字符串 `dest` 的末尾。
 * `src` 的第一个字节会覆盖 `dest` 的空终止符。
 * 目标字符串 `dest` 必须有足够的已分配空间来容纳结果。
 * 此函数不执行任何边界检查。
 *
 * @param dest 指向目标字符串的指针。
 * @param src 指向源字符串的指针。
 * @return 指向目标字符串 `dest` 的指针。
 */
char *
strcat(char *dest, const char *src)
{
    char *p = dest;
    while (*p)
    {
        ++p;
    }
    while (*src)
    {
        *p++ = *src++;
    }
    *p = '\0';
    return dest;
}

/**
 * @brief 将源字符串的最多 `n` 个字符追加到目标字符串的末尾。
 *
 * 将以空字符结尾的字符串 `src` 追加到以空字符结尾的字符串 `dest` 的末尾，
 * 但最多只追加 `src` 中的 `n` 个字符。如果 `n` 大于 0，
 * 或者 `dest` 最初已空字符结尾并且从 `src` 复制了 `n` 个字符，
 * 目标字符串 `dest` 将始终以空字符结尾。
 *
 * @param dest 指向目标字符串的指针。此字符串必须有足够的已分配空间
 *             来容纳其原始内容加上来自 `src` 的最多 `n` 个字符以及空终止符。
 * @param src  指向源字符串的指针。
 * @param n    要从 `src` 追加的最大字符数。
 * @return     指向目标字符串 `dest` 的指针。
 */
char *strncat(char *dest, const char *src, uint n)
{
    char *p = dest;

    // 找到目标字符串的末尾
    while (*p != '\0')
    {
        p++;
    }

    // 从 src 追加字符，最多 n 个或直到 src 结束
    while (n-- > 0 && *src != '\0')
    {
        *p++ = *src++;
    }

    // 确保目标字符串以空字符结尾
    *p = '\0';

    return dest;
}

/**
 * @brief 在字符串中查找字符的最后一次出现。
 *
 * 在 `s` 指向的字符串中查找 `c`（转换为 `char`）的最后一次出现。
 * 结尾的空字节也被认为是字符串的一部分。
 *
 * @param s 指向要搜索的字符串的指针。
 * @param c 要搜索的字符。
 * @return 指向字符 `c` 在字符串 `s` 中最后一次出现位置的指针，
 *         如果未找到字符则返回 `NULL`。如果 `c` 是空字符 ('\0')，
 *         则函数返回指向 `s` 的空终止符的指针。
 */
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

/**
 * @brief 在字符串中查找子字符串的第一次出现。
 *
 * 在以空字符结尾的字符串 `p1`（大海捞针）中查找以空字符结尾的字符串 `p2`（针）的第一次出现。
 *
 * @param p1 指向要搜索的字符串（大海捞针）的指针。
 * @param p2 指向要搜索的子字符串（针）的指针。
 * @return 指向子字符串 `p2` 在 `p1` 中第一次出现位置的指针，
 *         如果未找到子字符串则返回 `NULL`。如果 `p2` 是空字符串，则返回 `p1`。
 */
char *strstr(const char *p1, const char *p2)
{
    char *s1 = NULL;
    char *s2 = NULL;
    char *current = (char *)p1; // 用于遍历的可变指针

    // 如果 p2 是一个空字符串，则认为它出现在 p1 的开头
    if (*p2 == '\0')
    {
        return (char *)p1;
    }

    // 遍历 p1
    while (*current != '\0')
    {
        s1 = current;
        s2 = (char *)p2;

        // 尝试从 p1 的当前位置开始匹配 p2
        while ((*s1 != '\0') && (*s2 != '\0') && (*s1 == *s2))
        {
            s1++;
            s2++;
        }

        // 如果 s2 达到了它的空终止符，意味着 p2 已完全匹配
        if (*s2 == '\0')
        {
            return current; // 返回匹配的起始地址
        }

        // 如果 s1 在 s2 之前达到了它的空终止符，意味着 p1 在 p2 匹配完成之前结束了
        // 这隐式意味着没有匹配，所以继续到 p1 的下一个字符，如果已到末尾则返回 NULL。
        // 外层循环的条件 (*current != '\0') 处理到达 p1 末尾的情况。

        current++; // 移动到 p1 中的下一个字符，开始新的潜在匹配
    }
    return NULL; // 未找到子字符串
}

/**
 * @brief 在字符串中查找字符的第一次出现。
 *
 * 在 `s` 指向的字符串中查找 `c`（转换为 `char`）的第一次出现。
 * 结尾的空字节也被认为是字符串的一部分。
 *
 * @param s 指向要搜索的字符串的指针。
 * @param c 要搜索的字符。
 * @return 指向字符 `c` 在字符串 `s` 中第一次出现位置的指针，
 *         如果未找到字符则返回 `NULL`。如果 `c` 是空字符 ('\0')，
 *         则函数返回指向 `s` 的空终止符的指针。
 */
char *strchr(const char *s, int c)
{
    while (*s != '\0')
    {
        if (*s == (char)c)
        {
            return (char *)s;
        }
        s++;
    }
    // 如果 c 是空字符，返回指向空终止符的指针
    if (c == '\0')
    {
        return (char *)s;
    }
    return NULL;
}

/**
 * @brief 将字符串转换为整数。
 *
 * 解析以空字符结尾的字符串 `str`，并将其表示整数值的起始字符转换为 `int`。
 * 它处理前导空白字符和可选的符号（'+' 或 '-'）。
 *
 * @param str 指向要转换的字符串的指针。
 * @return 字符串表示的整数值。如果字符串开头不包含有效的整数表示，
 *         此实现会返回 0（对于更健壮的错误处理，标准库通常提供 `strtol`）。
 */
int atoi(const char *str)
{
    int result = 0;
    int sign = 1;

    // 跳过前导空白字符
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r' || *str == '\f' || *str == '\v')
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

/**
 * @brief 字符串分割函数
 *
 * 将字符串按照指定的分隔符进行分割。第一次调用时传入要分割的字符串，
 * 后续调用传入 NULL 来继续分割同一个字符串。
 *
 * @param str 要分割的字符串，第一次调用时传入，后续调用传入 NULL
 * @param delim 分隔符字符串
 * @return 返回下一个分割出的子字符串，如果没有更多分割结果则返回 NULL
 */
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
