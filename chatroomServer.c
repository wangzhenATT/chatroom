#define _GNU_SOURCE 1

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>

#define POLLFDLIMIT 5
#define BUFSIZE 1024
#define USERLIMIT 5

struct ClientData
{
	struct sockaddr_in clientaddr;
	char *write;
	char readbuf[BUFSIZE];
};

int SetNonBlock(int sockfd)
{
	int oldoption = fcntl(sockfd, F_GETFL);
	int newoption = oldoption | O_NONBLOCK;
	fcntl(sockfd, F_SETFL, newoption);
	return oldoption;
}

int main(int argc, char *argv[])
{
	if(argc <= 2)
	{
		printf("usage : %s ipaddress port\n", argv[0]);
		return 1;
	}

	char *ip;
	int port;
	int listenfd;
	int connfd;
	struct sockaddr_in servaddr;
	socklen_t len;
	int ret;

	ip = argv[1];
	port = atoi(argv[2]);
	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	inet_pton(AF_INET, ip, &servaddr.sin_addr);
	servaddr.sin_port = htons(port);

	listenfd = socket(PF_INET, SOCK_STREAM, 0);
	assert(listenfd >= 0);
	len = sizeof(servaddr);
	ret = bind(listenfd, (struct sockaddr*)&servaddr, len);
	assert(ret != -1);
	ret = listen(listenfd, 5);
	assert(ret != -1);
	SetNonBlock(listenfd);
	struct pollfd fdarr[POLLFDLIMIT+1];
	struct ClientData user[USERLIMIT+1];
	int count = 0;
	for(int i = 1; i <= POLLFDLIMIT; i++)
	{
		fdarr[i].fd = -1;
		fdarr[i].events = 0;
		fdarr[i].revents = 0;
	}
	fdarr[0].fd = listenfd;
	fdarr[0].events = POLLIN;
	fdarr[0].revents = 0;
	
	int nopollout;
	while(1)
	{
		nopollout = 1;
		ret = poll(fdarr, count+1, -1);
		if(ret < 0)
		{
			printf("poll error!\n");
			break;
		}
		for(int i = 0; i <= count; i++)
		{
			if((fdarr[i].fd == listenfd) && (fdarr[i].revents & POLLIN))
			{
				struct sockaddr_in clientaddr;
				socklen_t clientlen = sizeof(clientaddr);
				connfd = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
				if(connfd < 0)
				{
					printf("connect error and errno is : %d\n", errno);
					continue ;
				}
				if(count >= POLLFDLIMIT)
				{
					char *info = "too many users";
					printf("%s\n", info);
					send(connfd, info, strlen(info), 0);
					close(connfd);
					continue ;
				}
				count++;
				fdarr[count].fd = connfd;
				fdarr[count].events =  POLLRDHUP;
				fdarr[count].revents = 0;
				user[count].clientaddr = clientaddr;
				user[count].write = NULL;
				SetNonBlock(connfd);
				printf("come a new user %d, now have %d users\n", connfd, count);
			}
			else if(fdarr[i].revents & POLLRDHUP)
			{
				user[i] = user[count];
				close(fdarr[i].fd);
				fdarr[i] = fdarr[count];
				count--;
				i--;
				printf("one user left\n");
			}
			else if(fdarr[i].revents & POLLIN)
			{
				int sockfd = fdarr[i].fd;
				memset(user[i].readbuf, '\0', BUFSIZE);
				ret = recv(sockfd, user[i].readbuf, BUFSIZE-1, 0);
				printf("get %d bytes data from client %d\n", ret, sockfd);
				if(ret < 0)
				{
					if(errno != EAGAIN)
					{
						close(sockfd);
						user[i] = user[count];
						fdarr[i] = fdarr[count];
						i--;
						count--;
					}
				}
				else if(ret == 0)
				{
					close(sockfd);
					user[i] = user[count];
					fdarr[i] = fdarr[count];
					i--;
					count--;
				}
				else
				{
					for(int j = 1; j <= count; j++)
					{
						if(fdarr[j].fd == sockfd)
						{
							//continue ;
							fdarr[j].events &= ~POLLIN;
							continue ;
						}
						fdarr[j].events &= ~POLLIN;
						fdarr[j].events |= POLLOUT;
						user[j].write = user[i].readbuf;
					}
					break;
				}
			}
			else if(fdarr[i].revents & POLLOUT)
			{
				int sockfd = fdarr[i].fd;
				send(sockfd, user[i].write, strlen(user[i].write), 0);
				user[i].write = NULL;
				fdarr[i].events &= ~POLLOUT;
				//fdarr[i].events |= POLLIN;
				//printf("out success\n");
			}
		}
		//
		for(int m = 1; m <= count; m++)
		{
			if((fdarr[m].events & POLLOUT) && (fdarr[m].events != -1))
			{
				nopollout = 0;
				break;
			}
		}
		if(nopollout == 1)
		{
			for(int m = 1; m <= count; m++)
			{
					fdarr[m].events |= POLLIN;
			}
		}
	}
	close(listenfd);
	return 0;
}
