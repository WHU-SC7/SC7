#ifndef __STRING_H__
#define __STRING_H__

#include "types.h"

int memcmp(const void *, const void *, uint);
void *memcpy(void *dst, const void *src, uint n);
void *memmove(void *, const void *, uint);
void *memset(void *, int, uint);
char *safestrcpy(char *, const char *, int);
int strlen(const char *);
int strncmp(const char *, const char *, uint);
char *strncpy(char *, const char *, int);
int strcmp(const char *, const char *);
char *strcpy(char *, const char *);
char *strcat(char *dest, const char *src);
char *strncat(char *dest, const char *src, uint n);
char *strrchr(const char *s, int c);
char *strstr(const char *p1, const char *p2);
char *strchr(const char *s, int c);
char *strtok(char *str, const char *delim);
int atoi(const char *str);

#endif