#include "print.h"
#include "types.h"
#include "test.h"

/** ================ 测试辅助函数==============  */
/** 测试打印格式化输出 */
#define TEST_PRINT(title, fmt, ...) \
    do { \
        print_line("\n--- " #title " ---\n"); \
        printf(fmt, ##__VA_ARGS__); \
    } while(0)

/** 测试printf */
void 
test_print() 
{
    // 测试十进制整数
    TEST_PRINT(Decimal Basic, "%d %d %d", 123, -456, 0);       // 预期: 123 -456 0
    TEST_PRINT(Decimal Long, "%ld", (int64)2147483648L);     // 预期: 2147483648（测试32位溢出）
    TEST_PRINT(Decimal LongLong, "%lld", (int64)-123456789012345LL); // 预期: -123456789012345

    // 测试十六进制
    TEST_PRINT(Hex Basic, "%x %x", 255, (uint32)0xDeadBeef);          // 预期: ff deadbeef
    TEST_PRINT(Hex Long, "%lx", (uint64)0xCAFE1234);        // 预期: cafe1234
    TEST_PRINT(Hex LongLong, "%llx", (uint64)0x123456789ABCDEF0LL); // 预期: 123456789abcdef0

    // 测试指针
    void *p = (void*)0x12345678;
    TEST_PRINT(Pointer Normal, "%p", p);                      // 预期: 0x0000000012345678（格式依赖实现）
    TEST_PRINT(Pointer Null, "%p", NULL);                      // 预期: 0x0000000000000000


    // 测试字符串
    TEST_PRINT(String Normal, "%s", "Hello");                  // 预期: Hello
    TEST_PRINT(String Null, "%s", (char*)0);                   // 预期: (null)
    TEST_PRINT(String Mixed, "A%sb%sc", "1", "2");             // 预期: A1b2c

    // 测试无符号整数
    TEST_PRINT(Unsigned Basic, "%u %u", 123, (unsigned)-1);    // 预期: 123 4294967295
    TEST_PRINT(Unsigned Long, "%lu", (uint64)4294967296UL);  // 预期: 4294967296
    TEST_PRINT(Unsigned LongLong, "%llu", (uint64)123456789012345ULL); // 预期: 123456789012345

    // 测试特殊格式
    TEST_PRINT(Percent Sign, "%%");                            // 预期: %
    TEST_PRINT(Mixed Format, "a%%b%d%sc", 123, "test");        // 预期: a%b123testc

    // 测试异常格式
    TEST_PRINT(Unknown Format, "%a %lz");                      // 预期: %a %lz
    TEST_PRINT(Partial Format, "x%");                          // 预期: x%
    TEST_PRINT(Long Partial, "%l");                            // 预期: %l
    consputc('\n');
}

/** 测试 assert */
void 
test_assert ()
{
    int x = -1;
    int* y = &x;
    assert(x > 0, 
        "x should be positive, got %d, ptr_16 is %x, ptr is %p", 
        x, y, y);
    // 输出：panic: x should be positive, got -1
    // 然后进入死循环
}