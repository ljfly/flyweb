
#include "fly_http.h"

#define ONEKILO 1024
#define ONEMEGA 1024 * ONEKILO
#define ONEGIGA 1024 * ONEMEGA

//函数作用：处理客户端链接的线程例程,函数参数：param为客户conn_sock
void *fly_thread_func(void *param); 

u_short thread_num = 0; //记录当前处理线程的数量

pthread_mutex_t thread_num_mutex = PTHREAD_MUTEX_INITIALIZER;

void thread_num_add();

void thread_num_minus();

int32_t thread_num_read();
 /*
 *函数作用：根据解析下来的tyhp_http_header_t来处理客户的请求
 *函数参数：  phttphdr指向要处理的tyhp_http_header_t
 *			  out保存了处理的结果，即http响应包
 *函数返回值: HTTP状态码
 */
int do_http_header(http_header_t *phttphdr, string& out);
/*
 *函数作用：通过HTTP状态码返回友好语句
 *函数参数：HTTP状态码
 *函数返回值: 相应的语句
 */
char *get_state_by_codes(int http_codes);
/*main 函数*/
int main(int argc, char const *argv[])
{
	int 			   	listen_fd;
	int 				conn_sock;
	int 				nfds;
	int 				epollfd;
	u_short				listen_port;
	struct servent		*pservent;
	struct epoll_event 	ev;
	struct epoll_event 	events[MAX_EVENTS];
	struct sockaddr_in 	server_addr;
	struct sockaddr_in 	client_addr;
	socklen_t 			addrlen;
	pthread_attr_t 	   	pthread_attr_detach;
	Epollfd_connfd 	   	epollfd_connfd;
	pthread_t 		   	tid;

	if(argc != 3){
		printf("Usage: %s <config_path> <port> \n", argv[0]);
		exit(-1);
	}

	if(is_file_existed(argv[1]) == -1){
		perror("is_file_existed");
		exit(-1);
	}

	if(fly_parse_config(argv[1]) == -1){
		printf("fly_parse_config error\n");
		exit(-1);
	}
	//创建监听套接字
	listen_fd = Socket(AF_INET, SOCK_STREAM, 0);
	//设置监听套接字为非阻塞模式
	set_nonblocking(listen_fd);
	//对监听套接字设置SO_REUSEADDR选项
	set_reuse_addr(listen_fd);

//	pservent = Getservbyname("http", "tcp");
	//获取端口号
	listen_port = atoi(argv[2]);

	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(listen_port);
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	//将sockaddr_in与监听套接字绑定
	Bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));

	Listen(listen_fd, MAX_BACKLOG);
	//创建epoll文件描述符
	epollfd = Epoll_create(MAX_EVENTS);
	//可读事件
	ev.events = EPOLLIN;
	ev.data.fd = listen_fd;
	//将监听事件加入epoll中
	Epoll_ctl(epollfd, EPOLL_CTL_ADD, listen_fd, &ev);
	//设置线程属性为detach
	pthread_attr_init(&pthread_attr_detach);
	pthread_attr_setdetachstate(&pthread_attr_detach, PTHREAD_CREATE_DETACHED);

	for(;;){
		//等待直到有描述符就绪
		nfds = Epoll_wait(epollfd, events, MAX_EVENTS, -1);
		//若Epoll_wait被中断则重新调用该函数
		if(nfds == -1 && errno == EINTR) continue;
		//处理监听套接字触发的事件
		for(int i = 0; i != nfds; ++i) {
			//处理监听套接字触发的事件
			if(events[i].data.fd == listen_fd) {
				conn_sock = Accept(listen_fd, (struct sockaddr*)&client_addr, &addrlen);
				//设置新链接上的套接字为非阻塞模式
				set_nonblocking(conn_sock);
				//设置读事件和ET模式
				ev.events = EPOLLIN | EPOLLET;
				ev.data.fd = conn_sock;
				//将监听事件加入epoll中
				Epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev);
			}
			else {
				epollfd_connfd.epollfd = epollfd;
				epollfd_connfd.connfd = events[i].data.fd;
				ev.data.fd = conn_sock;
				//epoll删除这个客户端套接字
				Epoll_ctl(epollfd, EPOLL_CTL_DEL, conn_sock, &ev);
				//处理链接
				pthread_create(&tid, &pthread_attr_detach, &fly_thread_func,(void*)&epollfd_connfd);

			}
		}
	}

	pthread_attr_destroy(&pthread_attr_detach);
	close(listen_fd);
	return 0;
}

/*
 *函数作用：处理客户端链接的线程例程
 *函数参数：
 *函数返回值: NULL
 */
 #define TIMEOUT 1000*60*4

 void* fly_thread_func(void *param) {
 	thread_num_add();
 	http_header_t *phttphdr = alloc_http_header();

 	Epollfd_connfd *ptr_epollfd_connfd = (Epollfd_connfd*)param;
 	//获取客户连接socket
 	int conn_sock = ptr_epollfd_connfd->connfd;

 	struct epoll_event ev, events[2];
 	ev.events = EPOLLIN | EPOLLET;
 	ev.data.fd = conn_sock;
 	//针对客户链接的新epollfd
 	int epollfd = Epoll_create(2);
 	Epoll_ctl(epollfd, EPOLL_CTL_ADD, ev.data.fd, &ev);
 	int nfds = 0;

 	pthread_t tid = pthread_self();
 	printf("the %u thread already running\n", (unsigned int)tid);
 	//分配1M缓存来存http请求包
 	char *buff = (char*)fly_malloc(ONEMEGA);
 	bzero(buff, ONEMEGA);
	//关闭connfd的Nagle算法
 	set_off_tcp_nagle(conn_sock);
 	//设置接收超时时间为60秒
 	set_recv_timeout(conn_sock, 60, 0);
begin:
	int32_t nread = 0, n = 0;
	for(;;){
		if((n = read(conn_sock, buff+nread, ONEMEGA-1)) > 0)
			nread += n;
		else if(n == 0)
			break;
		else if(n == -1 && errno == EINTR)
			continue;
		else if(-1 == n && errno == EAGAIN)
			break;
		else if(n == -1 && errno == EWOULDBLOCK) {
			perror("socket read timeout");
			goto out;
		}
		else {
			perror("read http request error");
			fly_free(buff);
			break;
		}
	}

	if(nread != 0) {
		string str_http_request(buff, buff + nread);
		if(!parse_http_request(str_http_request, phttphdr)){
			perror("parse_http_request: parse str_http_request failed");
			goto out;
		}
		cout << "http request pack:" << endl;
		print_http_header(phttphdr);

		string out;
		int http_codes = do_http_header(phttphdr, out);

		cout << "http back pack" << endl << out << endl;

		char *out_buf = (char*)fly_malloc(out.size());
		if(out_buf == NULL)
			goto out;
		int i;
		for(i = 0; i != out.size(); ++i)
			out_buf[i] = out[i];
		out_buf[i] = '\0';
		int nwrite = 0, n = 0;
		if(http_codes == BADREQUEST 	|| 
		   http_codes == NOIMPLEMENTED 	||
		   http_codes == NOTFOUND 		||
		   (http_codes == OK && phttphdr->method == "HEAD")) {
			while((n = write(conn_sock, out_buf + nwrite, i)) != 0){
				if(n == -1) {
					if(errno == EINTR)
						continue;
					else 
						goto out;
				}
				nwrite += n;
			}
		}
		if(http_codes == OK){
			if(phttphdr->method == "GET"){
				while((n = write(conn_sock, out_buf + nwrite, i)) != 0){
					cout << n << endl;
					if(n == -1) {
						if(errno == EINTR)
							continue;
						else 
							goto out;
					}
					nwrite += n;
				}
		string real_url = fly_make_real_url(phttphdr->url);
		int fd = open(real_url.c_str(), O_RDONLY);
		int file_size = get_file_length(real_url.c_str());
		cout << "file_size " << file_size << endl;
		int nwrite = 0;
		cout << "sendfile : " << real_url.c_str() << endl;
	again:
		if((sendfile(conn_sock, fd, (off_t*)&nwrite, file_size)) < 0)
			perror("sendfile");
		if(nwrite < file_size)
			goto again;
		cout << "sendfile ok :" << nwrite << endl;
			}
		}
		free(out_buf);
		nfds = Epoll_wait(epollfd, events, 2, TIMEOUT);
		if(nfds == 0)
			goto out;
		for(int i = 0; i < nfds; ++i) {
			if(events[i].data.fd == conn_sock)
				goto begin;
			else 
				goto out;
		}
	}
out:
	free_http_header(phttphdr);
	close(conn_sock);
	thread_num_minus();
	printf("%u thread ends now\n", (unsigned int)tid);

 }

void thread_num_add(){
	pthread_mutex_lock(&thread_num_mutex);
	++thread_num;
	pthread_mutex_unlock(&thread_num_mutex);
}

void thread_num_minus(){
	pthread_mutex_lock(&thread_num_mutex);
	--thread_num;
	pthread_mutex_unlock(&thread_num_mutex);
}

int do_http_header(http_header_t *phttphdr, string& out)
{
	char status_line[256] = {0};
	string crlf("\r\n");
	string server("Server: http\r\n");
	string Public("Public: GET, HEAD\r\n");
	string content_base = "Content-Base: " + fly_domain + crlf;
	string date = "Date:" + time_gmt() + crlf;

	string content_length("Content-Length: ");
	string content_location("Content-Location: ");
	string last_modified("Last-Modified: ");
	//string body("");

	if(phttphdr == NULL)
	{
		snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d %s\r\n", 
			BADREQUEST, get_state_by_codes(BADREQUEST));
		out = status_line + crlf;
		return BADREQUEST;
	}

	string method = phttphdr->method;
	string real_url = fly_make_real_url(phttphdr->url);
	string version = phttphdr->version;
	if(method == "GET" || method == "HEAD")
	{
		if(is_file_existed(real_url.c_str()) == -1)
		{
			snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d %s\r\n", 
				NOTFOUND, get_state_by_codes(NOTFOUND));
			out += (status_line + server + date + crlf); 
			return NOTFOUND;
		}
		else
		{
			int len = get_file_length(real_url.c_str());
			snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d %s\r\n", 
				OK, get_state_by_codes(OK));
			out += status_line;
			snprintf(status_line, sizeof(status_line), "%d\r\n", len);
			out += content_length + status_line;
			out += server + content_base + date;
			out += last_modified + get_file_modified_time(real_url.c_str()) + crlf + crlf;
		}
	}
	else if(method == "PUT")
	{
		snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d %s\r\n", 
				NOIMPLEMENTED, get_state_by_codes(NOIMPLEMENTED));
		out += status_line + server + Public + date + crlf;
		return NOIMPLEMENTED;
	}
	else if(method == "DELETE")
	{
		snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d %s\r\n", 
				NOIMPLEMENTED, get_state_by_codes(NOIMPLEMENTED));
		out += status_line + server + Public + date + crlf;
		return NOIMPLEMENTED;
	}
	else if(method == "POST")
	{
		snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d %s\r\n", 
				NOIMPLEMENTED, get_state_by_codes(NOIMPLEMENTED));
		out += status_line + server + Public + date + crlf;
		return NOIMPLEMENTED;
	}
	else
	{
		snprintf(status_line, sizeof(status_line), "HTTP/1.1 %d %s\r\n", 
			BADREQUEST, get_state_by_codes(BADREQUEST));
		out = status_line + crlf;
		return BADREQUEST;
	}

	return OK;
}

 char *get_state_by_codes(int http_codes)
{
	switch (http_codes)
	{
		case OK:
			return ok;
		case BADREQUEST:
			return badrequest;
		case FORBIDDEN:
			return forbidden;
		case NOTFOUND:
			return notfound;
		case NOIMPLEMENTED:
			return noimplemented;
		default:
			break;
	}

	return NULL;
}