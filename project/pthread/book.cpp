// 本程序演示线程的创建和终止。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

pthread_mutex_t mutex;

volatile int var=0;

void *thmain(void *arg);    // 线程主函数。

int main(int argc,char *argv[])
{
  int ii=3;
  printf("=%d=\n",__sync_val_compare_and_swap(&ii,4,2));
  printf("ii=%d\n",ii);
  //if (bb==true) printf("true,ii=%d\n",ii);
  //if (bb==false) printf("false,ii=%d\n",ii);
  return 0;
  pthread_t thid1,thid2;

  // 创建线程。
  if (pthread_create(&thid1,NULL,thmain,NULL)!=0) { printf("pthread_create failed.\n"); exit(-1); }
  if (pthread_create(&thid2,NULL,thmain,NULL)!=0) { printf("pthread_create failed.\n"); exit(-1); }

  // 等待子线程退出。
  printf("join...\n");
  pthread_join(thid1,NULL);  
  pthread_join(thid2,NULL);  
  printf("join ok.\n");

  printf("var=%d\n",var);
}

void *thmain(void *arg)    // 线程主函数。
{
  for (int ii=0;ii<1000000;ii++)
  {
    __sync_synchronize();
    var++;
  }
}
