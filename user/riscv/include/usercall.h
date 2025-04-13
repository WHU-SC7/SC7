



extern int write( int fd, const void *buf, int len ) __attribute__( ( section( ".text.syscall_function" ) ) ); 
extern int getpid(void) __attribute__( ( section( ".text.syscall_function" ) ) ); 
extern int fork(void) __attribute__( ( section( ".text.syscall_function" ) ) ); 
extern int wait(int *code) __attribute__( ( section( ".text.syscall_function" ) ) ); 
extern int exit(int exit_status) __attribute__( ( section( ".text.syscall_function" ) ) ); 

