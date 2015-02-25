#ifndef _PACK_H_
#define _PACK_H_

#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/epoll.h>
#include <strings.h>
#include <string>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <utility>
#include <fstream>
#include <sstream>
#include <map>
#include <iostream>
#include <string.h>
#include <pthread.h>
#include <netinet/tcp.h>
#include <time.h>
#include <sys/stat.h>

using namespace std;

extern string fly_docroot;
#define FLY_DOCROOT	1
extern string fly_domain;
#define FLY_DOMAIN 	2

/***********************************  项目实用工具函数  *******************************************/
/*
 *函数作用：得到系统时间
 *函数参数：无
 *函数返回值: 系统时间 例如：Fri, 22 May 2009 06:07:21 GMT
 */
string time_gmt();
/*
 *函数作用：根据http请求包中的url和配置文件中的docroot配置选项构造真正的url
 *函数参数：url
 *函数返回值: 真正的url(绝对路径)
 */
string fly_make_real_url(const string& url);
/*
 *函数作用：测试文件是否存在
 *函数参数：path为绝对路径+文件名
 *函数返回值: -1表示文件不存在，其他值表示文件存在
 */
inline int is_file_existed(const char *path)
{
	int ret = open(path, O_RDONLY | O_EXCL);
	close(ret);
	return ret;
}
/*
 *函数作用：获得文件长度
 *函数参数：path为绝对路径+文件名
 *函数返回值: 文件长度
 */
int get_file_length(const char *path);
 /*
 *函数作用：获得文件最后修改时间
 *函数参数：path为绝对路径+文件名
 *函数返回值: 文件最后修改时间
 */
 string get_file_modified_time(const char *path);
 /*
 *函数作用：初始化全局变量fly_config_keyword_map，必须在使用fly_config_keyword_map前调用此函数
 *函数参数：无
 *函数返回值: 无
 */
 void init_config_keyword_map();
/*
 *函数作用：解析配置文件
 *函数参数：path为绝对路径+文件名
 *函数返回值: -1表示解析失败，0代表解析成功
 */
 int fly_parse_config(const char *path);
 /*
 *函数作用：设置文件描述符为非阻塞模式
 *函数参数：要设置的描述符
 *函数返回值: 无
 */
 void set_nonblocking(int fd);
 /*
 *函数作用：设置套接字SO_REUSEADDR选项
 *函数参数：要设置的套接字
 *函数返回值: 无
 */
 void set_reuse_addr(int sockfd);
 /*
 *函数作用：开启套接字TCP_NODELAY选项，关闭nagle算法
 *函数参数：要设置的套接字
 *函数返回值: 无
 */
 void set_off_tcp_nagle(int sockfd);
 /*
 *函数作用：关闭套接字TCP_NODELAY选项，开启nagle算法
 *函数参数：要设置的套接字
 *函数返回值: 无
 */
 void set_on_tcp_nagle(int sockfd);
/*
 *函数作用：开启套接字TCP_CORK选项
 *函数参数：要设置的套接字
 *函数返回值: 无
 */
 void set_on_tcp_cork(int sockfd);
 /*
 *函数作用：关闭套接字TCP_CORK选项
 *函数参数：要设置的套接字
 *函数返回值: 无
 */
 void set_off_tcp_cork(int sockfd);
/*
 *函数作用：设置套接字SO_RCVTIMEO选项，接收超时
 *函数参数：sockfd要设置的套接字, sec秒, usec毫秒
 *函数返回值: 无
 */
 void set_recv_timeout(int sockfd, int sec, int usec);
/*
 *函数作用：设置套接字SO_SNDTIMEO选项，发送超时
 *函数参数：sockfd要设置的套接字, sec秒, usec毫秒
 *函数返回值: 无
 */
 void set_snd_timeout(int sockfd, int sec, int usec);
/******************************************************************************************/

/***********************************  系统函数的包裹函数  *******************************************/
int Socket(int domain, int type, int protocol);
void Listen(int sockfd, int backlog);
void Bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int Accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
struct servent* Getservbyname(const char *name, const char *proto);
int Epoll_create(int size);
void Epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
int Epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);

void *fly_calloc(size_t nmemb, size_t size);
void *fly_malloc(size_t size);
void fly_free(void *ptr);
/******************************************************************************************/
#endif