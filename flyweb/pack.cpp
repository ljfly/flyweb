#include "flyweb.h"


int setNonblocking(int fd) {
  int flags;
  if (-1 ==(flags = fcntl(fd, F_GETFL, 0)))
    flags = 0;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

process* find_process_by_sock_slow(int sock) {
  int i;
  for (i = 0; i < MAX_PORCESS; i++) {
    if (processes[i].sock == sock) {
      return &processes[i];
    }
  }
  return 0;
}

process* find_empty_process_for_sock(int sock) {
  if (sock < MAX_PORCESS && sock >= 0 && processes[sock].sock == NO_SOCK) {
    return &processes[sock];
  } else {
    return find_process_by_sock_slow(NO_SOCK);
  }
}

process* find_process_by_sock(int sock) {
  if (sock < MAX_PORCESS && sock >= 0 && processes[sock].sock == sock) {
    return &processes[sock];
  } else {
    return find_process_by_sock_slow(sock);
  }
}

void reset_process(process* process) {
  process->read_pos = 0;
  process->write_pos = 0;
}
