#include "flyweb.h"
#include "tpool.h"
static int listen_sock;
static int efd;
static epoll_event event;
static char *doc_root;
static int current_total_processes;

process* accept_sock(int listen_sock) {
  int s;
  // 在 ET 模式下必须循环 accept 到返回 -1 为止
  while (1) {
    sockaddr in_addr;
    socklen_t in_len;
    int infd;
    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
    if (current_total_processes >= MAX_PORCESS) {
      // 请求已满， accept 之后直接挂断
      infd = accept(listen_sock, &in_addr, &in_len);
      if (infd == -1) {
        if (( errno == EAGAIN) ||
            (errno == EWOULDBLOCK)) {
          /* We have processed all incoming connections. */
          break;
        } else {
          perror("accept");
          break;
        }
      }
      close(infd);

      return NULL;
    }

    in_len = sizeof in_addr;
    infd = accept(listen_sock, &in_addr, &in_len);
    if (infd == -1) {
      if (( errno == EAGAIN) ||
          (errno == EWOULDBLOCK)) {
        break;
      } else {
        perror("accept");
        break;
      }
    }

  if( set_nonblocking(infd) == -1)
      abort();
  /* 开启套接字的tcp_cork选项，它是一种加强的nagle算法，过程和nagle算法类似， 都是累计数据然后发送。
  但它没有 nagle中1的限制，即使所有ack都已经收到，但我还是不想发送数据，我还
  想继续等待应用层更多的数据，所以它的效果比nagle更好 */
    int on = 1;
    setsockopt(infd, SOL_TCP, TCP_CORK, &on, sizeof(on));
    // 添加监视 sock 的读取状态
    event.data.fd = infd;
    event.events = EPOLLIN | EPOLLET;
    if( epoll_ctl(efd, EPOLL_CTL_ADD, infd, &event) == -1) {
      perror("epoll_ctl");
      abort();
    }
    process* process = find_empty_process_for_sock(infd);
    current_total_processes++;
    reset_process(process);
    process->sock = infd;
    process->fd = NO_FILE;
    process->status = STATUS_READ_REQUEST_HEADER;
  }
  return NULL;
}

// 根据目录名自动添加 index.html
int get_index_file(char *filename_buf, struct stat *pstat) {
  struct stat stat_buf;
  int s;
  if( lstat(filename_buf, &stat_buf) == -1) {
    // 文件或目录不存在
    return -1;
  }
  if (S_ISDIR(stat_buf.st_mode)) {
    // 是目录，追加 index.html
    strncpy(filename_buf + strlen(filename_buf), INDEX_FILE, sizeof(INDEX_FILE));
    // 再次判断是否是文件
    s = lstat(filename_buf, &stat_buf);
    if (s == -1 || S_ISDIR(stat_buf.st_mode)) {
      // 文件不存在，或者为目录
      int len = strlen(filename_buf);
      filename_buf[len] = 'l';
      filename_buf[len + 1] = 0;
      s = lstat(filename_buf, &stat_buf);
      if (s == -1 || S_ISDIR(stat_buf.st_mode)) {
        // 文件不存在，或者为目录
        return -1;
      }
    }
  }
  *pstat = stat_buf;
  return 0;
}

void read_request(process* process) {
  int sock = process->sock;
  int s;
  char* buf = process->buf;
  char read_complete = 0;

  ssize_t count;

  while (1) {
    count = read(sock, buf + process->read_pos, process->kBufferSize - process->read_pos);
    if (count == -1) {
      if (errno != EAGAIN) {
        handle_error(process, "read request");
        return;
      } else {
        // errno == EAGAIN 表示读取完毕
        break;
      }
    } else if (count == 0) {
      // 被客户端关闭连接
      cleanup(process);
      return;
    } else if (count > 0) {
      process->read_pos += count;
    }
  }

  int header_length = process->read_pos;
  // 判断请求是否完成
  if (header_length > process->kBufferSize - 1) {
    process->response_code = 400;
    process->status = STATUS_SEND_RESPONSE_HEADER;
    strncpy(process->buf, header_400, sizeof(header_400));
    send_response_header(process);
    handle_error(processes, "bad request");
    return;
  }
  buf[header_length]=0;
  read_complete =(strstr(buf, "\n\n") != 0) ||(strstr(buf, "\r\n\r\n") != 0);

  if (read_complete) {
    // 重置读取位置
    reset_process(process);
    // 获取get信息
    if (!strncmp(buf, "GET", 3) == 0) {
      bad_request(process);
      return;
    }
    // 得到第一行
    const char *n_loc = strchr(buf, '\n');
    const char *space_loc = strchr(buf + 4, ' ');
    if (n_loc <= space_loc) {
        bad_request(process);
      return;
    }
    char path[255];
    int len = space_loc - buf - 4;
    if (len > MAX_URL_LENGTH) {
      bad_request(process);
      return;
    }
    buf[header_length] = 0;
    strncpy(path, buf + 4, len);
    path[len] = 0;

    struct stat filestat;
    char fullname[256];
    char *prefix = doc_root;
    strncpy(fullname, prefix, strlen(prefix) + 1);
    strncpy(fullname + strlen(prefix), path, strlen(path) + 1);
    s = get_index_file(fullname, &filestat);
    if (s == -1) {
      not_found( process);
      return;
    }

    int fd = open(fullname, O_RDONLY);

    process->fd = fd;
    if (fd < 0) {
      not_found( process);
      return;
    } else {
      process->response_code = 200;
    }

    char tempstring[256];

    // 检查有无 If-Modified-Since，返回 304
    char* c = strstr(buf, HEADER_IF_MODIFIED_SINCE);
    if (c != 0) {
      char* rn = strchr(c, '\r');
      if (rn == 0) {
        rn = strchr(c, '\n');
        if (rn == 0) {
          process->response_code = 400;
          process->status = STATUS_SEND_RESPONSE_HEADER;
          strncpy(process->buf,  header_400, sizeof(header_400));
          send_response_header(process);
          handle_error(processes, "bad request");
          return;
        }
      }
      int time_len = rn - c - sizeof(HEADER_IF_MODIFIED_SINCE) + 1;
      strncpy(tempstring, c + sizeof(HEADER_IF_MODIFIED_SINCE) - 1, time_len);
      tempstring[time_len] = 0; {
        tm tm;
        time_t t;
        strptime(tempstring, RFC1123_DATE_FMT, &tm);
        tzset();
        t = mktime(&tm);
        t -= timezone;
        gmtime_r(&t, &tm);
        if (t >= filestat.st_mtime) {
          process->response_code = 304;
        }
      }
    }

    // 开始 header
    process->buf[0] = 0;
    if (process->response_code == 304) {
      write_to_header(header_304_start);
    } else {
      write_to_header(header_200_start);
    }

    process->total_length = filestat.st_size;
    {
      // 写入当前时间
      tm tm_t;
      tm *tm;
      time_t tmt;
      tmt = time(NULL);
      tm = gmtime_r(&tmt, &tm_t);
      strftime(tempstring, sizeof(tempstring), RFC1123_DATE_FMT, tm);
      write_to_header("Date: ");
      write_to_header(tempstring);
      write_to_header("\r\n");

      // 写入文件修改时间
      tm = gmtime_r(&filestat.st_mtime, &tm_t);
      strftime(tempstring, sizeof(tempstring), RFC1123_DATE_FMT, tm);
      write_to_header("Last-modified: ");
      write_to_header(tempstring);
      write_to_header("\r\n");

      if (process->response_code == 200) {
        // 写入 content 长度
        snprintf(tempstring, sizeof(tempstring), "Content-Length: %ld\r\n", filestat.st_size);
        write_to_header(tempstring);
      }
    }

    // 结束 header
    write_to_header(header_end);

    process->status = STATUS_SEND_RESPONSE_HEADER;
    // 修改此 sock 的监听状态，改为监视写状态
    event.data.fd = process->sock;
    event.events = EPOLLOUT | EPOLLET;
    s = epoll_ctl(efd, EPOLL_CTL_MOD, process->sock, &event);
    if (s == -1) {
      perror("epoll_ctl");
      abort();
    }
    // 发送 header
    send_response_header(process);
  }
}


int write_all(process *process, char* buf, int n) {
  int done_write = 0;
  int total_bytes_write = 0;
  while (!done_write && total_bytes_write != n) {
    int bytes_write = write(process->sock, buf + total_bytes_write, n - total_bytes_write);
    if (bytes_write == -1) {
      if (errno != EAGAIN) {
        handle_error(process, "write");
        return 0;
      } else {
        // 写入到缓冲区已满了
        return total_bytes_write;
      }
    } else {
      total_bytes_write += bytes_write;
    }
  }
  return total_bytes_write;
}


void send_response_header(process *process) {
#ifdef USE_TCP_CORK
  int on = 0;
  setsockopt(process->sock, SOL_TCP, TCP_CORK, &on, sizeof(on));
#endif

  if (process->response_code != 200) {
    // 非 200 不进入 send_response
    int bytes_writen = write_all(process, process->buf+process->write_pos,
                                 strlen(process->buf)-process->write_pos);
    if (bytes_writen ==(int)strlen(process->buf) + process->write_pos) {
      // 写入完毕
      cleanup(process);
    } else {
      process->write_pos += bytes_writen;
    }
  } else {
    int bytes_writen = write_all(process, process->buf+process->write_pos,
                                 strlen(process->buf)-process->write_pos);
    if (bytes_writen ==(int)strlen(process->buf) + process->write_pos) {
      // 写入完毕
      process->status = STATUS_SEND_RESPONSE;
      send_response(process);
    } else {
      process->write_pos += bytes_writen;
    }
  }
}

void send_response(process *process) {
#ifdef USE_SENDFILE
  // 使用 linux sendfile 函数
  while (1) {
    off_t offset = process->read_pos;
    int s = sendfile(process-> sock, process -> fd, &offset, process->total_length - process -> read_pos);
    process->read_pos = offset;
    if (s == -1) {
      if (errno != EAGAIN) {
        handle_error(process, "sendfile");
        return;
      } else {
        // 写入到缓冲区已满了
        return;
      }
    }
    if (process->read_pos == process->total_length) {
      // 读写完毕
      cleanup(process);
      return;
    }
  }
#else
  // 文件已经读完
  char end_of_file = 0;
  while (1) {
    // 检查有无已读取还未写入的
    int size_remaining = process->read_pos - process->write_pos;
    if (size_remaining > 0) {
      // 写入
      int bytes_writen = write_all(process, process->buf+process->write_pos, size_remaining);
      process->write_pos += bytes_writen;
      // 接下来判断是否写入完毕，如果是，继续读文件，否则 return
      if (bytes_writen != size_remaining) {
        // 缓冲区满
        return;
      }
    }
    if (end_of_file) {
      // 读写完毕，关闭 sock 和文件
      cleanup(process);
      return;
    }
    // 用同步的方式读取到缓冲区满
    process -> read_pos = 0;
    process -> write_pos = 0;
    while (process->read_pos < process->kBufferSize) {
      int bytes_read = read(process->fd, process->buf, process->kBufferSize - process->read_pos);
      if (bytes_read == -1) {
        if (errno != EAGAIN) {
          handle_error(process, "read file");
          return;
        }
        break;
      } else if (bytes_read == 0) {
        end_of_file = 1;
        break;
      } else if (bytes_read > 0) {
        process->read_pos += bytes_read;
      }
    }
  }
#endif
}

void cleanup(process *process) {
  int s;
  if (process->sock != NO_SOCK) {
#ifdef USE_TCP_CORK
    int on = 0;
    setsockopt(process->sock, SOL_TCP, TCP_CORK, &on, sizeof(on));
#endif
    s = close(process->sock);
    current_total_processes--;
    if (s == NO_SOCK) {
      perror("close sock");
    }
  }
  if (process->fd != -1) {
    if( close(process->fd) == NO_FILE) {
      printf("fd: %d\n", process->fd);
      printf("\n");
      perror("close file");
    }
  }
  process->sock = NO_SOCK;
  reset_process(process);
}

void handle_request(int sock) {
  if (sock == listen_sock) {
 //  if( tpool_add_work(accept_sock, &sock) == -1)
   // perror("sock");
   accept_sock(sock);
  } else {
    process* process = find_process_by_sock(sock);
    if (process != 0) {
      switch (process->status) {
        case STATUS_READ_REQUEST_HEADER:
       //   read_request(process);
        tpool_add_work(read_request, (void*)process);
          break;
        case STATUS_SEND_RESPONSE_HEADER:
         // send_response_header(process);
        tpool_add_work(send_response_header, (void*)process);
          break;
        case STATUS_SEND_RESPONSE:
         // send_response(process);
        tpool_add_work(send_response, (void*)process);
          break;
        default:
          break;
      }
    }
  }
}

static int create_bind_listen(char *port) {
int        s,     listen_sock;  
struct sockaddr_in server_addr;
struct sockaddr_in client_addr;
socklen_t          addrlen;

listen_sock = socket(AF_INET, SOCK_STREAM,0);

 if( set_nonblocking(listen_sock) == -1)
    abort();

bzero(&server_addr, sizeof(server_addr));
server_addr.sin_family = AF_INET;
server_addr.sin_port = htons(atoi(port));
server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

bind(listen_sock,(struct sockaddr*)&server_addr, sizeof(server_addr));

if( listen(listen_sock, SOMAXCONN) == -1){
    perror("listen");
    abort();
  }
  return listen_sock;
}

int main(int argc, char *argv[]) {
  int s;
  epoll_event *events;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s [port] [doc root]\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  doc_root = argv[2];

  if (tpool_create(MAX_PTHREAD) != 0) {
        printf("tpool_create failed\n");
        exit(-1);
    }

  for (int i = 0;i < MAX_PORCESS; i ++) {
    processes[i].sock = NO_SOCK;
  }
  listen_sock = create_bind_listen(argv[1]);
  if (listen_sock == -1)
    abort();

  efd = epoll_create1(0);
  if (efd == -1) {
    perror("epoll_create");
    abort();
  }

  event.data.fd = listen_sock;
  event.events = EPOLLIN | EPOLLET;
   if( epoll_ctl(efd, EPOLL_CTL_ADD, listen_sock, &event) == -1) {
    perror("epoll_ctl");
    abort();
  }
  /* Buffer where events are returned */
  events = new epoll_event[MAXEVENTS];

  while (1) {
  int n = epoll_wait(efd, events, MAXEVENTS, -1);
    if (n == -1) {
      perror("epoll_wait");
    }
    for (int i = 0; i < n; i++) {
      if (( events[i].events & EPOLLERR) ||
          (events[i].events & EPOLLHUP)) {
        fprintf(stderr, "epoll error\n");
        close(events[i].data.fd);
        continue;
      }
      handle_request(events[i].data.fd);
    }
  }
//tpool_destroy();
  //free(events);
  close(listen_sock);
  return EXIT_SUCCESS;
}
