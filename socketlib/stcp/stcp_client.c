
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <sys/time.h>

#include "../common/debug.h"
#include "stcp_client.h"

/*面向应用层的接口*/

//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现.
//
//  目标: 你的任务就是设计并实现下面的函数原型.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

void sendbuf_send(int sockfd);
void sendbuf_ack(int sockfd, unsigned seqnum);
int getTime();

tcb_list_item tcb_list[MAX_TRANSPORT_CONNECTIONS];
unsigned int gSeqNum = 0;
int stcp2sip_conn;

static pthread_mutex_t stcp2sip_conn_mutex = PTHREAD_MUTEX_INITIALIZER;

// stcp客户端初始化
//
// 这个函数初始化TCB表, 将所有条目标记为NULL.  
// 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量, 该变量作为sip_sendseg和sip_recvseg的输入参数.
// 最后, 这个函数启动seghandler线程来处理进入的STCP段. 客户端只有一个seghandler.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void stcp_client_init(int conn) {
	for(int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i ++) {
		tcb_list[i].used = 0;
	}

	Log("STCP client initialized.");
	stcp2sip_conn = conn;

	pthread_t tid;
	pthread_create(&tid, NULL, seghandler, NULL);
	Assert(tid > 0, "Creating seghandler thread failure!");
}

// 创建一个客户端TCB条目, 返回套接字描述符
//
// 这个函数查找客户端TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化. 例如, TCB state被设置为CLOSED，客户端端口被设置为函数调用参数client_port. 
// TCB表中条目的索引号应作为客户端的新套接字ID被这个函数返回, 它用于标识客户端的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_sock(unsigned int client_port) {
	int i;
	for(i = 0; i < MAX_TRANSPORT_CONNECTIONS; i ++) {
		if(tcb_list[i].used == 0) {
			break;
		}
	}

	if(i == MAX_TRANSPORT_CONNECTIONS) {
		Log("No empty TCB!\n");
		return -1;
	} else {
		tcb_list[i].used = 1;
		memset(&tcb_list[i].tcb, 0, sizeof(tcb_list[i].tcb));
		tcb_list[i].tcb.state = CLOSED;
		tcb_list[i].tcb.client_portNum = client_port;
		tcb_list[i].tcb.bufMutex = malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(tcb_list[i].tcb.bufMutex, 0);
		tcb_list[i].tcb.next_seqNum = gSeqNum;

		Log("Client socket %d was created.", i);
		return i;
	}
}

// 连接STCP服务器
//
// 这个函数用于连接服务器. 它以套接字ID和服务器的端口号作为输入参数. 套接字ID用于找到TCB条目.  
// 这个函数设置TCB的服务器端口号,  然后使用sip_sendseg()发送一个SYN段给服务器.  
// 在发送了SYN段之后, 一个定时器被启动. 如果在SYNSEG_TIMEOUT时间之内没有收到SYNACK, SYN 段将被重传. 
// 如果收到了, 就返回1. 否则, 如果重传SYN的次数大于SYN_MAX_RETRY, 就将state转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_connect(int sockfd, int nodeID, unsigned int server_port) {
	assert(tcb_list[sockfd].used == 1);
	assert(tcb_list[sockfd].tcb.state == CLOSED);

	tcb_list[sockfd].tcb.server_nodeID = nodeID;
	tcb_list[sockfd].tcb.server_portNum = server_port;

	seg_t seg;
	make_seg(&seg, &tcb_list[sockfd], SYN, NULL, 0);
	stcp_hdr_to_network_order(&seg.header);

	tcb_list[sockfd].tcb.state = SYNSENT;

	int n = SYN_MAX_RETRY;
	while(n --> 0) {	// interesting --> operator
		pthread_mutex_lock(&stcp2sip_conn_mutex);
		if(sip_sendseg(stcp2sip_conn, tcb_list[sockfd].tcb.server_nodeID, &seg) == 1) {
			Log("STCP client socket %d sending SYN %d!", sockfd, SYN_MAX_RETRY - n);
		} else {
			Log("STCP client socket %d sending SYN %d failed!", sockfd, SYN_MAX_RETRY - n);
		}
		pthread_mutex_unlock(&stcp2sip_conn_mutex);

		usleep(SYN_TIMEOUT / 1000);

		if(tcb_list[sockfd].tcb.state == CONNECTED) {
			Log("stcp client connect succeeded!");
			return 1;
		}

		Log("STCP client socket %d SYN %d timeout!", sockfd, SYN_MAX_RETRY - n);
	}

	tcb_list[sockfd].tcb.state = CLOSED;
	Log("stcp client connect failed!");

	return -1;
}

// 发送数据给STCP服务器
//
// 这个函数发送数据给STCP服务器. 你不需要在本实验中实现它。
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_send(int sockfd, void* data, unsigned int length) {
	assert(tcb_list[sockfd].used == 1);
	assert(tcb_list[sockfd].tcb.state == CONNECTED);
	assert(data && (length > 0));

	// pack data to buffer
	pthread_mutex_lock(tcb_list[sockfd].tcb.bufMutex);
	int n = (length + MAX_SEG_LEN - 1) / MAX_SEG_LEN;
	for(int i = 0; i < n; i ++) {
		segBuf_t *pseg = malloc(sizeof(segBuf_t));
		memset(pseg, 0, sizeof(segBuf_t));
		make_seg(&pseg->seg, &tcb_list[sockfd], DATA, data, length >= MAX_SEG_LEN ? MAX_SEG_LEN : length);
		stcp_hdr_to_network_order(&pseg->seg.header);

		if(tcb_list[sockfd].tcb.sendBufHead == NULL) {
			tcb_list[sockfd].tcb.sendBufHead = pseg;
			tcb_list[sockfd].tcb.sendBufTail = pseg;
			tcb_list[sockfd].tcb.sendBufunSent = pseg;
		} else {
			tcb_list[sockfd].tcb.sendBufTail->next = pseg;
			tcb_list[sockfd].tcb.sendBufTail = pseg;
			if(tcb_list[sockfd].tcb.sendBufunSent == NULL) {
				tcb_list[sockfd].tcb.sendBufunSent = pseg;
			}
		}

		Log("STCP client socket %d add user data to sendBuf.", sockfd);

		data += MAX_SEG_LEN;
		length -= MAX_SEG_LEN;
	}

	pthread_mutex_unlock(tcb_list[sockfd].tcb.bufMutex);

	// send data out
	sendbuf_send(sockfd);

	return 1;
}

// 断开到STCP服务器的连接
//
// 这个函数用于断开到服务器的连接. 它以套接字ID作为输入参数. 套接字ID用于找到TCB表中的条目.  
// 这个函数发送FIN segment给服务器. 在发送FIN之后, state将转换到FINWAIT, 并启动一个定时器.
// 如果在最终超时之前state转换到CLOSED, 则表明FINACK已被成功接收. 否则, 如果在经过FIN_MAX_RETRY次尝试之后,
// state仍然为FINWAIT, state将转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_disconnect(int sockfd) {
	assert(tcb_list[sockfd].used == 1);
	assert(tcb_list[sockfd].tcb.state == CONNECTED);

	int ret = -1;

	seg_t seg;
	make_seg(&seg, &tcb_list[sockfd], FIN, NULL, 0);
	stcp_hdr_to_network_order(&seg.header);

	tcb_list[sockfd].tcb.state = FINWAIT;

	int n = FIN_MAX_RETRY;
	while(n --> 0) {
		pthread_mutex_lock(&stcp2sip_conn_mutex);
		if(sip_sendseg(stcp2sip_conn, tcb_list[sockfd].tcb.server_nodeID, &seg) == 1) {
			Log("STCP client %d sending FIN %d!", sockfd, FIN_MAX_RETRY - n);
		} else {
			Log("STCP client %d sending FIN %d failed!", sockfd, FIN_MAX_RETRY - n);
		}
		pthread_mutex_unlock(&stcp2sip_conn_mutex);

		usleep(FIN_TIMEOUT / 1000);

		if(tcb_list[sockfd].tcb.state == CLOSED) {
			Log("stcp client disconnect succeeded!");
			ret = 1;
			break;
		}

		Log("STCP client %d FIN %d timeout!", sockfd, FIN_MAX_RETRY - n);
	}

	tcb_list[sockfd].tcb.state = CLOSED;

	// free sendbuf
	pthread_mutex_lock(tcb_list[sockfd].tcb.bufMutex);
	while(tcb_list[sockfd].tcb.sendBufHead) {
		segBuf_t *tmp = tcb_list[sockfd].tcb.sendBufHead;
		tcb_list[sockfd].tcb.sendBufHead = tcb_list[sockfd].tcb.sendBufHead->next;
		free(tmp);
	}
	pthread_mutex_unlock(tcb_list[sockfd].tcb.bufMutex);

	return ret;
}

// 关闭STCP客户
//
// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_close(int sockfd) {
	if(tcb_list[sockfd].tcb.state == CLOSED) {
		tcb_list[sockfd].used = 0;
		free(tcb_list[sockfd].tcb.bufMutex);
		Log("STCP client %d close succeeds.", sockfd);
		return 1;
	} else {
		Log("STCP client %d close fails.", sockfd);
		return -1;
	}
}

// 处理进入段的线程
//
// 这是由stcp_client_init()启动的线程. 它处理所有来自服务器的进入段. 
// seghandler被设计为一个调用sip_recvseg()的无穷循环. 如果sip_recvseg()失败, 则说明重叠网络连接已关闭,
// 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作. 请查看客户端FSM以了解更多细节.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int get_socket_by_port(unsigned src_port, unsigned dest_port) {
	for(int i = 0; i < MAX_TRANSPORT_CONNECTIONS; i ++) {
		if((tcb_list[i].used == 1)
			&& (tcb_list[i].tcb.client_portNum == src_port)
			&& (tcb_list[i].tcb.server_portNum == dest_port)) {
			return i;
		}
	}

	return -1;
}


void *seghandler(void* arg) {
	Log("Issuing seghandler.");

	int sfd;
	int src_nodeID;
	seg_t seg;
	while(1) {
		int ret = sip_recvseg(stcp2sip_conn, &src_nodeID, &seg);

		if(ret == -1) {
			Log("sip receive segment failed, exit!");
			pthread_exit(NULL);
		} else if(ret == 1) {
			Log("STCP client sip_recvseg was lost!");
			continue;
		}

		stcp_hdr_to_host_order(&seg.header);

		if((sfd = get_socket_by_port(seg.header.dest_port, seg.header.src_port)) == -1) {
			Assert(0, "Invalid connection!");
		}

		switch(tcb_list[sfd].tcb.state) {
			case CLOSED: {
				break;
			}

			case SYNSENT: {
				if(seg.header.type == SYNACK) {
					tcb_list[sfd].tcb.state = CONNECTED;
					Log("STCP client %d receiving SYNACK!", sfd);
				}

				break;
			}

			case CONNECTED: {
				if(seg.header.type == DATAACK) {
					Log("STCP client %d received DATAACK!", sfd);
					sendbuf_ack(sfd, seg.header.ack_num);
					sendbuf_send(sfd);
				}

				break;
			}

			case FINWAIT: {
				if(seg.header.type == FINACK) {
					tcb_list[sfd].tcb.state = CLOSED;
					Log("STCP client %d receiving FINACK!", sfd);
				}

				break;
			}

			default:	Assert(0, "Invalid state!");
		}
	}
}


// 这个线程持续轮询发送缓冲区以触发超时事件. 如果发送缓冲区非空, 它应一直运行.
// 如果(当前时间 - 第一个已发送但未被确认段的发送时间) > DATA_TIMEOUT, 就发生一次超时事件.
// 当超时事件发生时, 重新发送所有已发送但未被确认段. 当发送缓冲区为空时, 这个线程将终止.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//`1
void* sendBuf_timer(void* clienttcb) {
	assert(clienttcb);

	tcb_list_item *ptcb = (tcb_list_item*)clienttcb;
	assert(ptcb->used == 1);

	pthread_mutex_lock(ptcb->tcb.bufMutex);

	while(ptcb->tcb.unAck_segNum > 0) {
		// data timeout, resend all unacked segment
		if(getTime() - ptcb->tcb.sendBufHead->sentTime > DATA_TIMEOUT / 1000) {
			Log("STCP client send buffer timeout, resending...");
			for(segBuf_t *sb = ptcb->tcb.sendBufHead; sb && (sb != ptcb->tcb.sendBufunSent); sb = sb->next) {
				pthread_mutex_lock(&stcp2sip_conn_mutex);
				if(sip_sendseg(stcp2sip_conn, ptcb->tcb.server_nodeID, &sb->seg) == 1) {
					Log("Resended segment sequence number: %d", ntohl(sb->seg.header.seq_num));
					sb->sentTime = getTime();
				} else {
					Log("Sending segment failed!");
				}
				pthread_mutex_unlock(&stcp2sip_conn_mutex);
			}
		}
		pthread_mutex_unlock(ptcb->tcb.bufMutex);

		usleep(SENDBUF_POLLING_INTERVAL / 1000);

		pthread_mutex_lock(ptcb->tcb.bufMutex);
	}

	pthread_mutex_unlock(ptcb->tcb.bufMutex);

	Log("sendbuf timer exit");
	pthread_exit(NULL);

	return NULL;
}

void sendbuf_send(int sockfd) {
	pthread_mutex_lock(tcb_list[sockfd].tcb.bufMutex);
	
	while((tcb_list[sockfd].tcb.sendBufunSent != NULL) && 
			(tcb_list[sockfd].tcb.unAck_segNum < GBN_WINDOW)) {

		pthread_mutex_lock(&stcp2sip_conn_mutex);
		if(sip_sendseg(stcp2sip_conn, tcb_list[sockfd].tcb.server_nodeID, &(tcb_list[sockfd].tcb.sendBufunSent->seg)) == 1) {
			Log("Sending segment out. seq_num = %d.", ntohl(tcb_list[sockfd].tcb.sendBufunSent->seg.header.seq_num));
		} else {
			Log("Sending segment failed.");
		}
		pthread_mutex_unlock(&stcp2sip_conn_mutex);

		tcb_list[sockfd].tcb.sendBufunSent->sentTime = getTime();

		if(tcb_list[sockfd].tcb.unAck_segNum == 0) {
			pthread_t tid;
			pthread_create(&tid, NULL, sendBuf_timer, (void*)&tcb_list[sockfd]);
			Assert(tid > 0, "Creating sendBuf timer failed!");
		}

		tcb_list[sockfd].tcb.unAck_segNum ++;
		tcb_list[sockfd].tcb.sendBufunSent = tcb_list[sockfd].tcb.sendBufunSent->next;
	}

	pthread_mutex_unlock(tcb_list[sockfd].tcb.bufMutex);
}

void sendbuf_ack(int sockfd, unsigned seqnum) {
	//Log("Receiving ack number: %u", seqnum);
	pthread_mutex_lock(tcb_list[sockfd].tcb.bufMutex);

	segBuf_t *p = tcb_list[sockfd].tcb.sendBufHead;
	while(p && (ntohl(p->seg.header.seq_num) < seqnum)) {
		tcb_list[sockfd].tcb.sendBufHead = tcb_list[sockfd].tcb.sendBufHead->next;
		tcb_list[sockfd].tcb.unAck_segNum --;
		segBuf_t *tmp = p;
		p = p->next;
		Log("Received seq_number: %d, free seq_num = %d, unAck_segNum = %d", seqnum, ntohl(tmp->seg.header.seq_num), tcb_list[sockfd].tcb.unAck_segNum);
		free(tmp);
	}

	if(p != NULL) {
		Assert(ntohl(p->seg.header.seq_num) == seqnum, "Invalid seqnum!!!");
	}

	pthread_mutex_unlock(tcb_list[sockfd].tcb.bufMutex);
}

void make_seg(seg_t *pseg, tcb_list_item *ptcb, unsigned short type, char *data, unsigned len) {
	assert(pseg && ptcb && ptcb->used);

	memset(pseg, 0, sizeof(seg_t));
	pseg->header.src_port = ptcb->tcb.client_portNum;
	pseg->header.dest_port = ptcb->tcb.server_portNum;
	pseg->header.seq_num = ptcb->tcb.next_seqNum;
	pseg->header.length = 0;
	pseg->header.type = type;
	pseg->header.checksum = 0;

	if(data && len) {
		pseg->header.length = len;
		//pseg->header.length = len;
		memcpy(pseg->data, data, len);
		ptcb->tcb.next_seqNum += len;
	}

	pseg->header.checksum = checksum(pseg);
}

// Get current time(ms)
int getTime() {
	struct timeval currentTime;
	gettimeofday(&currentTime, NULL);
	return (currentTime.tv_sec * 1000000 + currentTime.tv_usec);
}

