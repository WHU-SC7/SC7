
unsigned long __write(int fd, const void *buf, int len);
unsigned long __read(int fd, const void *buf, int len);
unsigned long __openat(int fd, const char *pathname, int flags, unsigned short mode);
unsigned long __creat(const char *pathname, unsigned short mode);
unsigned long __close(int fd);

//printf
void print_int(int num);
void __printf(const char *fmt, ...);

//自定义
void shutdow();