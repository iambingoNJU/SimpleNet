
#ifndef __LIB_SOCKET_H__
#define __LIB_SOCKET_H__


// send exactly n bytes
int sendn(int fd, char *data, int len);

// receive exactly n bytes
int rcvn(int fd, char *bp, int len);

// send data with "!&" in front and "!#" in tail
int tcp_send_data(int conn, char *data, int len);

// receive data begining with "!&" and ending with "!#"
int tcp_recv_data(int conn, char *data, unsigned max_len);


#endif /* __LIB_SOCKET_H__ */
