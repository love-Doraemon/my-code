/*
 * 程序名：demo27.cpp，此程序用于演示HTTP客户端。
 * 作者：吴从周。
*/
#include "../_public.h"
 
int main(int argc,char *argv[])
{
  if (argc!=3)
  {
    printf("Using:./demo27 ip port\nExample:./demo27 www.weather.com.cn 80\n\n"); return -1;
  }

  CTcpClient TcpClient;

  // 向服务端发起连接请求。
  if (TcpClient.ConnectToServer(argv[1],atoi(argv[2]))==false)
  {
    printf("TcpClient.ConnectToServer(%s,%s) failed.\n",argv[1],argv[2]); return -1;
  }

  char buffer[202400];
  memset(buffer,0,sizeof(buffer));

  strcpy(buffer,\
     "GET / HTTP/1.1\r\n"\
     "User-Agent: Wget/1.14 (linux-gnu)\r\n"\
     "Accept: */*\r\n"\
     "Host: www.weather.com.cn:80\r\n"\
     "Connection: Keep-Alive\r\n"\
     "\r\n");

  send(TcpClient.m_connfd,buffer,strlen(buffer),0);

  while (true)
  {
  memset(buffer,0,sizeof(buffer));
  //Readn(TcpClient.m_connfd,buffer,109093);
  if (recv(TcpClient.m_connfd,buffer,100000,0)<=0) break;
  printf("%s\n",buffer);
  }
}

