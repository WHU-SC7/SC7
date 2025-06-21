//
//
//
#include "string.h"
#include "types.h"

void *memset(void *dst, int c, uint n)
{
	char *cdst = (char *)dst;
	int i;
	for (i = 0; i < n; i++) {
		cdst[i] = c;
	}
	return dst;
}

int memcmp(const void *v1, const void *v2, uint n)
{
	const uchar *s1, *s2;

	s1 = v1;
	s2 = v2;
	while (n-- > 0) {
		if (*s1 != *s2)
			return *s1 - *s2;
		s1++, s2++;
	}

	return 0;
}

void *memmove(void *dst, const void *src, uint n)
{
	const char *s;
	char *d;

	s = src;
	d = dst;
	if (s < d && s + n > d) {
		s += n;
		d += n;
		while (n-- > 0)
			*--d = *--s;
	} else
		while (n-- > 0)
			*d++ = *s++;

	return dst;
}

// memcpy exists to placate GCC.  Use memmove.
void *memcpy(void *dst, const void *src, uint n)
{
	return memmove(dst, src, n);
}

int strncmp(const char *p, const char *q, uint n)
{
	while (n > 0 && *p && *p == *q)
		n--, p++, q++;
	if (n == 0)
		return 0;
	return (uchar)*p - (uchar)*q;
}

/**
 * @brief 从字符串t复制n个字符到字符串s
 * 
 * @param s 
 * @param t 
 * @param n 
 * @return char* 就是s
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

// Like strncpy but guaranteed to NUL-terminate.
char *safestrcpy(char *s, const char *t, int n)
{
	char *os;

	os = s;
	if (n <= 0)
		return os;
	while (--n > 0 && (*s++ = *t++) != 0)
		;
	*s = 0;
	return os;
}

int strlen(const char *s)
{
	int n;

	for (n = 0; s[n]; n++)
		;
	return n;
}

void dummy(int _, ...)
{
}

char*
strcpy(char *s, const char *t)
{
  char *os;

  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}

int
strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}

char *
strcat(char *dest, const char *src) {
    char *p = dest;
    while (*p) {
        ++p;
    }
    while (*src) {
        *p++ = *src++;
    }
    *p = '\0';
    return dest;
}

/**
 * @brief 查找字符串s中最后一次出现字符c的位置，返回指针或NULL
 * 
 * @param s 
 * @param c 
 * @return char* 指针或NULL
 */
char *strrchr(const char *s, int c)
{
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c)
            last = s;
        s++;
    }
    // 检查c是否为'\0'
    if (c == 0)
        return (char *)s;
    return (char *)last;
}