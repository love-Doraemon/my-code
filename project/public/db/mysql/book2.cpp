#include "_mysql.h"       // 开发框架操作MySQL的头文件。

int main(int argc,char *argv[])
{
  connection conn;   // 数据库连接类。

  conn.connecttodb("192.168.174.132,root,mysqlpwd,mysql,3306","utf8");

  sqlstatement stmt1(&conn);
  sqlstatement stmt2(&conn);
  sqlstatement stmt3(&conn);

  if (stmt1.execute("insert into girls(id,name) values(20,'冰冰')")!=0)
  { printf("stmt1.execute() failed.\n%s\n%s\n",stmt1.m_sql,stmt1.m_cda.message); return -1; }

  if (stmt2.execute("update girls set weight=33.56 where id=10")!=0)
  { printf("stmt2.execute() failed.\n%s\n%s\n",stmt2.m_sql,stmt2.m_cda.message); return -1; }

  if (stmt3.execute("delete from girls where id=1")!=0)
  { printf("stmt3.execute() failed.\n%s\n%s\n",stmt3.m_sql,stmt3.m_cda.message); return -1; }

  sqlstatement stmt4(&conn);
  if (stmt4.execute("select id,name from girls")!=0)
  { printf("stmt4.execute() failed.\n%s\n%s\n",stmt4.m_sql,stmt4.m_cda.message); return -1; }
  while (stmt4.next()==0);

  if (stmt3.execute("insert into girls(id,name) values(21,'圆圆')")!=0)
  { printf("stmt3.execute() failed.\n%s\n%s\n",stmt3.m_sql,stmt3.m_cda.message); return -1; }

  if (stmt1.execute("update girls set weight=34.56 where id=11")!=0)
  { printf("stmt1.execute() failed.\n%s\n%s\n",stmt1.m_sql,stmt1.m_cda.message); return -1; }

  if (stmt2.execute("delete from girls where id=2")!=0)
  { printf("stmt2.execute() failed.\n%s\n%s\n",stmt2.m_sql,stmt2.m_cda.message); return -1; }

  conn.commit();

  return 0;
}

