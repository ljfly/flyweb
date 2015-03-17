#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <langinfo.h>
#include <locale.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/sendfile.h>
#define USE_SENDFILE 1
#define USE_TCP_CORK 1

#define MAXEVENTS 10240
#define MAX_PORCESS 10240
#define MAX_URL_LENGTH 256
#define PORT 8080
#define INDEX_FILE "/index.html"


#define STATUS_READ_REQUEST_HEADER 0
#define STATUS_SEND_RESPONSE_HEADER 1
#define STATUS_SEND_RESPONSE  2

#define NO_SOCK -1
#define NO_FILE -1

#define RFC1123_DATE_FMT "%a, %d %b %Y %H:%M:%S %Z"

#define header_404 "HTTP/1.1 404 Not Found\r\nServer: fly_web/1.0\r\n" \
    "Content-Type: text/html\r\nConnection: Close\r\n\r\n<h1>Not found</h1>"
#define header_400 "HTTP/1.1 400 Bad Request\r\nServer: fly_web/1.0\r\n" \
    "Content-Type: text/html\r\nConnection: Close\r\n\r\n<h1>Bad request</h1>"
#define header_200_start "HTTP/1.1 200 OK\r\nServer: fly_web/1.0\r\n" \
    "Content-Type: text/html\r\nConnection: Close\r\n"
#define header_304_start "HTTP/1.1 304 Not Modified\r\nServer: fly_web/1.0\r\n" \
    "Content-Type: text/html\r\nConnection: Close\r\n"

#define header_end "\r\n"

#define HEADER_IF_MODIFIED_SINCE "If-Modified-Since: "

#define copy_str(dst, src) strncpy(dst, src, strlen(src) + 1)
#define write_to_header(string_to_write) copy_str(process->buf + strlen(process->buf), string_to_write)

struct process {
  static const int kBufferSize = 4024;
  int sock;
  int status;
  int response_code;
  int fd;
  int read_pos;
  int write_pos;
  int total_length;
  char buf[kBufferSize];
};

int setNonblocking(int fd);

process* find_process_by_sock(int sock);

process* accept_sock(int listen_sock);

void read_request(process* process);

void send_response_header(process *process);

void send_response(process *process);

void cleanup(process *process);

void handle_error(process *process, const char* error_string);

void reset_process(process *process);

int open_file(char *filename);



