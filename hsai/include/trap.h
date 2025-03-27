//参考ucore
//这是hal要向hsai提供的接口
//

void set_usertrap();//设置中断和异常的跳转地址，写csr，架构相关

void usertrap();//处理来自用户态的中断，异常

void usertrapret();//返回用户态
