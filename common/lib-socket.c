
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <sys/socket.h>

#include "debug.h"
#include "lib-socket.h"

// send exactly n bytes
int sendn(int fd, char *data, int len) {
	int rc;
	int cnt = len;

	while(cnt > 0) {
		rc = send(fd, data, cnt, 0);
		if(rc < 0) {
			if(errno == EINTR)
				continue;
			return -1;
		}

		if(rc == 0)
			return len - cnt;
		data += rc;
		cnt -= rc;
	}

	return len;
}


// receive exactly n bytes
int rcvn(int fd, char *bp, int len) {
	int cnt;
	int rc;

	cnt = len;
	while(cnt > 0) {
		rc = recv(fd, bp, cnt, 0);
		if(rc < 0) {
			if(errno == EINTR)		/* interrupted? */
				continue;			/* restart the read */
			return -1;				/* return error */
		}

		if(rc == 0)					/* EOF? */
			return len - cnt;
		bp += rc;
		cnt -= rc;
	}

	return len;
}


int tcp_send_data(int conn, char *data, int len) {
	assert((conn >= 0) && data && len);

	// send "!&"
	if(sendn(conn, "!&", 2) != 2) {
		return -1;
	}

	// send data
	if(sendn(conn, data, len) != len) {
		return -1;
	}

	// send "!#"
	if(sendn(conn, "!#", 2) != 2) {
		return -1;
	}

	return 1;
}

int tcp_recv_data(int conn, char *data, unsigned max_len) {
	assert((conn >= 0) && data);

	char ch = 0;
	int flag = 0;
	unsigned cnt = 0;
	char rcv_buffer[2048];
	memset(rcv_buffer, 0, 2048);

	int state = 0;
	while(rcvn(conn, &ch, 1) == 1) {
		switch(state) {
			case 0: {
				if(ch == '!') {
					state = 1;
				} else {
					Assert(0, "Expect receiving leading '!', but received '%c'.", ch);
				}

				break;
			}

			case 1: {
				if(ch == '&') {
					state = 2;
				} else {
					Assert(0, "Expect receiving leading '&', but received '%c'.", ch);
				}

				break;
			}

			case 2: {
				if((flag == 1) && (ch == '#')) {
					cnt --;	// ignore last '!'
					memcpy(data, rcv_buffer, cnt);

					return cnt;
				}

				if(ch == '!') {
					flag = 1;
				} else {
					flag = 0;
				}

				rcv_buffer[cnt ++] = ch;
				break;
			}

		}

		if(cnt > (max_len + 1)) {
			Assert(0, "Receiving a too big packet.");
		}

	}

	return -1;
}
