#include <stdio.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
//保存客户端发送的数据包
struct msg
{
	char name[32];
	char data[128];
};

/*
	init_socket:创建一个套接字，进行初始化
	@ip :IP地址
	@port :端口号
	
	返回值：创建的套接字的描述符
*/
int init_socket(char*ip,char*port)
{
	//创建一个套接字
	
	int server_fd = socket(AF_INET,SOCK_STREAM,0);
	if(server_fd == -1)
	{
		perror("socket error:");
		return -1;
	}
	perror("socket:");
	
	//设置服务器的地址结构体
	struct sockaddr_in server_addr;
	bzero(&server_addr,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(port));
	server_addr.sin_addr.s_addr = inet_addr(ip);
	
	//设置套接字的选项，允许本IP地址重复使用
	 int a = 1;
	 int r = setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,&a,sizeof(a));

	
	//绑定bind:把自己的套接字描述符绑定到IP地址上去
	int ret = bind(server_fd, (struct sockaddr *)&server_addr,sizeof(server_addr));
	if(ret == -1)
	{
		perror("bind error:");
		return -1;
	}
	perror("bind :");
	
	//监听listen
	ret  = listen(server_fd, 3);
	if(ret == -1)
	{
		perror("listen error:");
		return -1;
	}
	perror("listen :");
	
	
	return server_fd;
}
/*
	Communication:接收客户端的请求并发生通信
*/
int Communication(int server_fd)
{
	struct sockaddr_in client_addr;//定义客户端的地址结构体
	socklen_t len = sizeof(client_addr);
	fd_set  readfd,tmpfd;//用来保存需要监听 的读的描述符. readfd :存储未被标记的文件描述符
	FD_ZERO(&readfd);//将读的描述符集合清零
	FD_SET(server_fd,&readfd);//将服务端的描述符加入读的描述符集合
	int maxfd = server_fd;//用来保存最大的文件描述符
	struct timeval timeout;//保存溢出的时间
	
	struct msg*msg=malloc(sizeof(struct msg));//保存客户端发送的数据包
	bzero(msg,sizeof(struct msg));
	char*name[128]={NULL};//指针数组，用来指向各客户端的名字
	int i;
	while(1)
	{
		timeout.tv_sec = 5;//why不放在外面？因为如果在2秒时有描述符就绪，则处理完回来后时间为上一次剩余的时间
		timeout.tv_usec = 0;
		tmpfd = readfd;//这样可以保证readfd中的描述符都没有被标记
		int r = select(maxfd+1,&tmpfd,NULL,NULL, &timeout);//对文件描述符集合进行监听
		if(r > 0)//表示有文件描述符就绪  客户端就绪 ：有可读的数据   服务端就绪 ：有客户端来链接
		{
			for(i = 3;i <= maxfd;i++)  //一个个检测哪个文件描述符准备就绪
			{
				if(FD_ISSET(i,&tmpfd))
				{
					if(i == server_fd)//服务器的文件描述符
					{
						int client_fd = accept(server_fd,(struct sockaddr*)&client_addr,&len);
						FD_SET(client_fd,&readfd);//把新的客户端的文件描述符加入到readfd的集合中
						
						//最大的文件描述符可能被更新
						if(client_fd > maxfd)
							maxfd = client_fd;
					}
					else//接收客户端的数据
					{
						bzero(msg,sizeof(struct msg));
						//一般在实际工作中通常会采用线程来对数据进行处理
						int re = read(i,msg,sizeof(struct msg));
						if(re == -1)
						{
							perror("read error");
							return -1;
						}
						//后面的处理可以丢到任务队列上，避免阻塞主线程，有机会再实现
						if(*msg->name == 0 && *msg->data == 0)//客户端下线后如果再往其文件描述符上写数据，客户端描述符会发一个让服务端死亡的信号，因此一旦发现有客户端下线就要剔除它
						{
							FD_CLR(i,&readfd);//清理掉已经下线的客户端描述符
							name[i]=NULL;//清空此客户端的名字
							free(name[i]);
							continue;
						}
						if(*msg->data == 0 && *msg->name != 0)//客户端第一次发送数据，为其名字
						{	
							name[i]=malloc(32);
							strcpy(name[i],msg->name);
							break;
						}
							
						//判断为私聊还是群聊
						if(*msg->name != 0)//私聊
						{
							int j;
							for(j=3;j<=maxfd;j++)//找出名字所对应的文件描述符
							{
								if(name[j] != NULL && strcmp(name[j],msg->name) == 0)//先要剔除掉没有打开的文件描述符所对应的名字数组
								{
									strcpy(msg->name,name[i]);
									re=write(j,msg,sizeof(struct msg));
									if(re == -1)
									{
										perror("write error");
										return -1;
									}
									perror("write");
									break;
								}	
							}
						}
						else//群聊
						{
							strcpy(msg->name,name[i]);
							int j;
							for(j=3;j<=maxfd;j++)//给每一个打开的文件描述符发送数据
							{
								if(j == server_fd)//不发给服务端
								{
									continue;
								}
								if(name[j] != NULL)//只给已经打开的文件描述符发
								{
									re=write(j,msg,sizeof(struct msg));
									if(re == -1)
									{
										perror("write error");
										return -1;
									}
									perror("write");
								}
							}
						}
					}
				}
			}
		}
		else if(r == 0)//时间溢出  在5秒内一直都没有文件描述符是可读（客户端）或 可连接（服务端）
		{
			printf("timeout\n");
		}
		else
		{
			perror("select error:");
			return -1;
		}
		
	}
	
}
int main(int argc,char**argv)
{
	//初始化套接字
	int server_fd = init_socket(argv[1],argv[2]);
	
	//对客户端的连接请求做出回应，发生通信
	Communication(server_fd);
	
	//关闭套接字
	close(server_fd);
	
}

//./client 192.168.31.142  8888