#include <stdio.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>
#include <strings.h>
#include <arpa/inet.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
//发送的数据包结构体
struct msg
{
	char name[32];
	char data[128];
};

void*routine(void*arg)
{
	pthread_detach(pthread_self());//设置线程为分离状态，不用主线程收尸
	
	int client_fd = *((int*)arg);
	
	struct msg*msg=malloc(sizeof(struct msg));//开辟空间保存别的客户端发来的数据包
	//接收数据包
	while(1)
	{
		int ret = read(client_fd,msg,sizeof(struct msg));
		if(ret > 0)
		{
			printf("%s:%s\n",msg->name,msg->data);
		}
		
	}
}
//./client 192.168.31.142 8888 xxx
int main(int argc,char*argv[])
{
	//创建一个套接字
	int client_fd=socket(AF_INET, SOCK_STREAM, 0);
	if(client_fd==-1)
	{
		perror("socket error");
		return -1;
	}
	perror("socket:");
	
	//设置服务器的地址结构体
	struct sockaddr_in server_addr;
	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family=AF_INET;//协议族
	server_addr.sin_port=htons(atoi(argv[2]));//端口号
	server_addr.sin_addr.s_addr=inet_addr(argv[1]);//IP地址
	
	//连接,要给出对方的IP地址和端口号
	int re=connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if(re==-1)
	{
		perror("connect error");
		return -1;
	}
	perror("connect:");
	
	//读写数据，把自己的名字给服务器
	struct msg*msg=malloc(sizeof(*msg));//开辟数据包空间
	bzero(msg,sizeof(struct msg));//将数据包清零
	strcpy(msg->name,argv[3]);//将自己的名字给服务端，让服务端知道自己的描述符所对应的名字
	re=write(client_fd,msg,sizeof(struct msg));
	if(re == -1)
	{
		perror("write error");
		return -1;
	}
	
	//创建子线程进行读取别的客户端发来的数据
	pthread_t thread;
	re=pthread_create(&thread, NULL,routine, (void *)&client_fd);
	if(re == -1)
	{
		perror("pthread_create error");
		return -1;
	}
	//发送数据包
	while(1)
	{
		char buf[128]={0};
		bzero(msg,sizeof(struct msg));//在给数据包赋值之前要先清空数据包，因为群聊时发hello，msg->name中就不会为0而是之前的内容
		fgets(buf,sizeof(buf),stdin);//输入格式：hello zzz(私聊)、hello(群聊)
		sscanf(buf,"%s %s",msg->data,msg->name);//给其他客户端发送数据:yyy hello
		re=write(client_fd,msg,sizeof(struct msg));
		printf("re=%d\n",re);
		perror("write:");
		
	}
	
	//关闭套接字
	close(client_fd);
}