/*
 *  程序名：filetoblob.cpp，此程序演示开发框架操作MySQL数据库（把图片文件存入BLOB字段）。
 *  作者：吴从周。
*/
#include "_mysql.h"   // 开发框架操作MySQL的头文件。

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

// 定义用于超女信息的结构，与表中的字段对应。
struct st_girls
{
  long   id;             // 超女编号，用long数据类型对应Oracle无小数的number(10)。
  char   name[31];       // 超女姓名，用char[31]对应Oracle的varchar2(30)。
  double weight;         // 超女体重，用double数据类型对应Oracle有小数的number(8,2)。
  char   btime[20];      // 报名时间，用char对应Oracle的date，格式：'yyyy-mm-dd hh24:mi:ssi'。
  char   pic[100000];    // 超女图片。
  unsigned long picsize; // 图片大小。
} stgirls;

int main(int argc,char *argv[])
{
  connection conn; // 数据库连接类

  // 登录数据库，返回值：0-成功，其它-失败。
  // 失败代码在conn.m_cda.rc中，失败描述在conn.m_cda.message中。
  if (conn.connecttodb("127.0.0.1,root,mysqlpwd,mysql,3306","utf8") != 0)
  {
    printf("connect database failed.\n%s\n",conn.m_cda.message); return -1;
  }
  
  sqlstatement stmt(&conn); // 操作SQL语句的对象。

  // 准备插入表的SQL语句。
  stmt.prepare("update girls set pic=:1 where id=:2");
  // prepare方法不需要判断返回值。
  // 为SQL语句绑定输入变量的地址，bindin方法不需要判断返回值。
  stmt.bindinlob(1,stgirls.pic,&stgirls.picsize);
  stmt.bindin(2,&stgirls.id);

  stgirls.id=1;   // 超女编号。
  // 把图片的内容加载到pic中。
  stgirls.picsize=filetobuf("1.jpg",stgirls.pic);

  // 执行SQL语句，一定要判断返回值，0-成功，其它-失败。
  // 失败代码在stmt.m_cda.rc中，失败描述在stmt.m_cda.message中。
  if (stmt.execute() != 0)
  {
      printf("stmt.execute() failed.\n%s\n%s\n",stmt.m_sql,stmt.m_cda.message); return -1;
  }
  printf("成功更新了%ld条记录。\n",stmt.m_cda.rpc); // stmt.m_cda.rpc是本次执行SQL影响的记录数。

  stgirls.id=2;   // 超女编号。
  // 把图片的内容加载到pic中。
  stgirls.picsize=filetobuf("2.jpeg",stgirls.pic);

  // 执行SQL语句，一定要判断返回值，0-成功，其它-失败。
  // 失败代码在stmt.m_cda.rc中，失败描述在stmt.m_cda.message中。
  if (stmt.execute() != 0)
  {
      printf("stmt.execute() failed.\n%s\n%s\n",stmt.m_sql,stmt.m_cda.message); return -1;
  }
  printf("成功更新了%ld条记录。\n",stmt.m_cda.rpc); // stmt.m_cda.rpc是本次执行SQL影响的记录数。

  printf("update table girls ok.\n");

  conn.commit(); // 提交数据库事务。
}

