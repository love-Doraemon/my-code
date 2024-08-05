#include "_mysql.h"       // 开发框架操作MySQL的头文件。

int main(int argc,char *argv[])
{
  connection conn1,conn2;   // 数据库连接类。

  conn1.connecttodb("192.168.174.132,root,mysqlpwd,mysql,3306","utf8");
  conn2.connecttodb("192.168.174.133,root,mysqlpwd,mysql,3306","utf8");

  conn1.execute("delete from girls where id<3");
  conn2.execute("delete from girls where id>3");

  conn1.commit();
  conn2.rollback();

  return 0;
}

