

// 定义一个结构体来存储每个字符的ASCII艺术表示
typedef struct {
    int code; // 字符的ASCII码
    char *lines[7]; // 字符的每一行的ASCII表示
  } FigletChar;


void printf_figlet(char *fmt);
void printf_figlet_color(char *fmt);
void test_figlet();