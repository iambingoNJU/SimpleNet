//ÎÄŒþÃû: server/stcp_server.c
//
//ÃèÊö: ÕâžöÎÄŒþ°üº¬STCP·þÎñÆ÷œÓ¿ÚÊµÏÖ. 
//
//ŽŽœšÈÕÆÚ: 2015Äê

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/time.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <assert.h>
#include <unistd.h>
#include "stcp_server.h"
#include "../topology/topology.h"
#include "../common/constants.h"
#include "../common/debug.h"


//tcb

static int tcb_cnt = 0;
//seghandler and  son
static server_tcb_t *server_tcb_ptr[MAX_TRANSPORT_CONNECTIONS];
static pthread_t seghandler_tid = -1;
static int SON_conn = -1;
//read is just one 
//write is more than one, so should have a mutex
static pthread_mutex_t mutex_SON_conn = PTHREAD_MUTEX_INITIALIZER;
//wait or wake up CLOSE_WAIT process
static pthread_cond_t cond_close_wait[MAX_TRANSPORT_CONNECTIONS];
static int refresh_close_wait[MAX_TRANSPORT_CONNECTIONS];
static pthread_t close_wait_tid = -1;
pthread_mutex_t close_wait_mutex = PTHREAD_MUTEX_INITIALIZER;
//Accept function
// static pthread_cond_t cond_wait_connected;
// static pthread_mutex_t mutex_wait_connected; //= PTHREAD_MUTEX_INITIALIZER;
/*面向应用层的接口*/


static int xstcp_packet_send(int sockfd, unsigned short int type, char *data, int data_len){
	char msg[1500];
	memset(msg, 0, sizeof(char) * 1500);
	seg_t* seg = (seg_t *)msg;
	seg->header.src_port = server_tcb_ptr[sockfd]->server_portNum;
	seg->header.dest_port = server_tcb_ptr[sockfd]->client_portNum;
	if (type == DATAACK){
		seg->header.seq_num = 0;//server_tcb_ptr[sockfd]->expect_seqNum;
		seg->header.ack_num = server_tcb_ptr[sockfd]->expect_seqNum;
	}
	else{
		seg->header.seq_num = 0;//server_tcb_ptr[sockfd]->expect_seqNum;
		seg->header.ack_num = 0;//server_tcb_ptr[sockfd]->expect_seqNum;
	}
	seg->header.length = data_len;
	seg->header.type = type;
	seg->header.rcv_win = 0;

	if (data != NULL){
		memcpy(seg->data, data, data_len);
	}
	seg->header.checksum = checksum(seg);
	stcp_hdr_to_network_order(&(seg->header));
	pthread_mutex_lock(&mutex_SON_conn);
	sip_sendseg(SON_conn, server_tcb_ptr[sockfd]->client_nodeID, seg);
	pthread_mutex_unlock(&mutex_SON_conn);
	return 1;
}
static void* xstcp_close_wait(void * sockfd){
	struct timeval now;
	struct timespec outtime; 
	struct timezone tz;
	int sockfd_int = *(int *)sockfd;
	pthread_mutex_lock(&close_wait_mutex);
	while(refresh_close_wait[sockfd_int] == 1){
		refresh_close_wait[sockfd_int] = 0;
		gettimeofday(&now, &tz);
		outtime.tv_sec = now.tv_sec + CLOSEWAIT_TIMEOUT;
		outtime.tv_nsec = now.tv_usec * 1000;
		fflush(stdout);
		pthread_cond_timedwait(&cond_close_wait[sockfd_int], &close_wait_mutex, &outtime);
	}
	pthread_mutex_unlock(&close_wait_mutex);
	server_tcb_ptr[sockfd_int]->state = CLOSED;
	Log("[STCP CLOSE WAIT] closed");
	pthread_exit(NULL);
}

static int ReceiveBufferSave(int sockfd, struct segment *seg){
	if (server_tcb_ptr[sockfd]->expect_seqNum == seg->header.seq_num){
		pthread_mutex_lock(server_tcb_ptr[sockfd]->bufMutex);
		if(RECEIVE_BUF_SIZE - server_tcb_ptr[sockfd]->usedBufLen >= seg->header.length){
			memcpy(&(server_tcb_ptr[sockfd]->recvBuf[server_tcb_ptr[sockfd]->usedBufLen]), seg->data, seg->header.length);
			// printf("RBF memcpy data: %s\n", &(server_tcb_ptr[sockfd]->recvBuf[server_tcb_ptr[sockfd]->usedBufLen]));
			server_tcb_ptr[sockfd]->usedBufLen += seg->header.length;
			server_tcb_ptr[sockfd]->expect_seqNum += seg->header.length;
			// printf("[STCP ReceiveBufferSave] Get Data:%s\n", seg->data);
			xstcp_packet_send(sockfd, DATAACK, NULL, 0);
			Log("[STCP ReceiveBufferSave] Get Data and Saved");
			// return 1;
		}
		else{
			Log("[STCP ReceiveBufferSave] Receive Buffer is full.So a packet is droped.");
		}
		pthread_mutex_unlock(server_tcb_ptr[sockfd]->bufMutex);
	}
	else{
		xstcp_packet_send(sockfd, DATAACK, NULL, 0);
		Log("[STCP ReceiveBufferSave] seq_num is not matched");
	}
	return 0;
}

//
//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现.
//
//  目标: 你的任务就是设计并实现下面的函数原型.
//

// stcp服务器初始化
//
// 这个函数初始化TCB表, 将所有条目标记为NULL. 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量,
// 该变量作为sip_sendseg和sip_recvseg的输入参数. 最后, 这个函数启动seghandler线程来处理进入的STCP段.
// 服务器只有一个seghandler.
//

void stcp_server_init(int conn) {
	memset(server_tcb_ptr, 0, sizeof(struct server_tcb *) * MAX_TRANSPORT_CONNECTIONS);
	SON_conn = conn;
	tcb_cnt = 0;
	pthread_create(&seghandler_tid, NULL, seghandler, NULL);
	Assert(seghandler_tid > 0, "[STCP Init] pthread create failed!!");
	Log("[STCP Init] Server Init");
  	return;
}

// 创建服务器套接字
//
// 这个函数查找服务器TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化, 例如, TCB state被设置为CLOSED, 服务器端口被设置为函数调用参数server_port. 
// TCB表中条目的索引应作为服务器的新套接字ID被这个函数返回, 它用于标识服务器的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.

int stcp_server_sock(unsigned int server_port) {
	int sock_idx = 0;
	for(sock_idx = 0; sock_idx < MAX_TRANSPORT_CONNECTIONS; ++sock_idx){
		if (server_tcb_ptr[sock_idx] == NULL){
			break;
		}
	}
	if (sock_idx == MAX_TRANSPORT_CONNECTIONS){
		Log("[STCP SOCK] TCBs are all used");
		return -1;
	}
	//init TCB infos
	server_tcb_ptr[sock_idx] = (server_tcb_t *) malloc(sizeof(struct server_tcb));
	memset(server_tcb_ptr[sock_idx], 0, sizeof(struct server_tcb));
	server_tcb_ptr[sock_idx]->state = CLOSED;
	server_tcb_ptr[sock_idx]->server_portNum = server_port;
	server_tcb_ptr[sock_idx]->client_nodeID = -1;
	tcb_cnt ++;
	//alloc mutex and init
	server_tcb_ptr[sock_idx]->bufMutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(server_tcb_ptr[sock_idx]->bufMutex, NULL);
	//alloc recvBuf and init
	server_tcb_ptr[sock_idx]->recvBuf = (char *)malloc(sizeof(char) * RECEIVE_BUF_SIZE);
	memset(server_tcb_ptr[sock_idx]->recvBuf, 0, sizeof(char) * RECEIVE_BUF_SIZE);
	server_tcb_ptr[sock_idx]->expect_seqNum = 0;
	server_tcb_ptr[sock_idx]->usedBufLen = 0;

	//init variables which close_wait function needed
 	pthread_cond_init(&cond_close_wait[sock_idx], NULL);
	for (int i = 0; i < MAX_TRANSPORT_CONNECTIONS; ++i){
		refresh_close_wait[i] = 1; 
	}
	Log("[STCP SOCK] sock succeeded");
	return sock_idx;
}

// 接受来自STCP客户端的连接
//
// 这个函数使用sockfd获得TCB指针, 并将连接的state转换为LISTENING. 它然后进入忙等待(busy wait)直到TCB状态转换为CONNECTED 
// (当收到SYN时, seghandler会进行状态的转换). 该函数在一个无穷循环中等待TCB的state转换为CONNECTED,  
// 当发生了转换时, 该函数返回1. 你可以使用不同的方法来实现这种阻塞等待.
//

int stcp_server_accept(int sockfd) {
	server_tcb_ptr[sockfd]->state = LISTENING;
	Log("[STCP Accept] State change to LISTENING");
	
	while(server_tcb_ptr[sockfd]->state != CONNECTED)
		usleep(1000);//ACCEPT_POLLING_INTERVAL/100000000);
	// pthread_cond_init(&cond_wait_connected, NULL);
	// pthread_mutex_init(&mutex_wait_connected, NULL);
	// pthread_mutex_lock(&mutex_wait_connected);

	// while(server_tcb_ptr[sockfd]->state != CONNECTED){
	// 	Log("[STCP Accept] connected is waiting");
	// 	pthread_cond_wait(&cond_wait_connected, &mutex_wait_connected);
	// }
	// pthread_mutex_unlock(&mutex_wait_connected);
	xstcp_packet_send(sockfd, SYNACK, NULL, 0);
	Log("[STCP Accept] SYN and Response SYNACK");
	// pthread_mutex_destroy(&mutex_wait_connected);
	// pthread_cond_destroy(&cond_wait_connected);
	return 1;
}

// 接收来自STCP客户端的数据
//
// 这个函数接收来自STCP客户端的数据. 你不需要在本实验中实现它.
//
int stcp_server_recv(int sockfd, void* buf, unsigned int length) {
	// Assert(length < RECEIVE_BUF_SIZE, "[STCP Recv] Error!! length > RECEIVE_BUF_SIZE, This Arch Cannot do it.");

	int recv_len = length;
	void* dst = buf;
	while(1){
		if (recv_len <= server_tcb_ptr[sockfd]->usedBufLen){
			pthread_mutex_lock(server_tcb_ptr[sockfd]->bufMutex);
			memcpy(dst, server_tcb_ptr[sockfd]->recvBuf, recv_len);
			memmove(server_tcb_ptr[sockfd]->recvBuf, &(server_tcb_ptr[sockfd]->recvBuf[recv_len]), server_tcb_ptr[sockfd]->usedBufLen - recv_len);
			server_tcb_ptr[sockfd]->usedBufLen -= recv_len;
			pthread_mutex_unlock(server_tcb_ptr[sockfd]->bufMutex);
			Log("[STCP Recv] Receive Message from Client");
			return 1;
		}
		else if(recv_len > RECEIVE_BUF_SIZE && server_tcb_ptr[sockfd]->usedBufLen >= RECEIVE_BUF_SIZE - MAX_SEG_LEN){
			pthread_mutex_lock(server_tcb_ptr[sockfd]->bufMutex);
			memcpy(dst, server_tcb_ptr[sockfd]->recvBuf, server_tcb_ptr[sockfd]->usedBufLen);
			dst += server_tcb_ptr[sockfd]->usedBufLen;
			recv_len -= server_tcb_ptr[sockfd]->usedBufLen;
			server_tcb_ptr[sockfd]->usedBufLen = 0;
			pthread_mutex_unlock(server_tcb_ptr[sockfd]->bufMutex);
		}
		else{
			Log("[STCP Recv] Buffer size is not enough. Waiting....");
			sleep(RECVBUF_POLLING_INTERVAL);
		}
	}
	return 1;
}

// 关闭STCP服务器
//
// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//

int stcp_server_close(int sockfd) {
	// printf("[STCP CLOSE] sockfd = %d, state = %d\n", sockfd, server_tcb_ptr[sockfd]->state);
	fflush(stdout);
	while (1){
		if (server_tcb_ptr[sockfd]->state == CLOSED){
			break;
		}
	}
	free(server_tcb_ptr[sockfd]->recvBuf);
	free(server_tcb_ptr[sockfd]->bufMutex);
	free(server_tcb_ptr[sockfd]);
	server_tcb_ptr[sockfd] = NULL;
	Log("[STCP CLOSE] server close succeeded");
	--tcb_cnt;
	if (tcb_cnt == 0){
		pthread_cancel(seghandler_tid);
	}
	return 1;
	Log("[STCP CLOSE] server close failed");
	return -1;
}

// 处理进入段的线程
//
// 这是由stcp_server_init()启动的线程. 它处理所有来自客户端的进入数据. seghandler被设计为一个调用sip_recvseg()的无穷循环, 
// 如果sip_recvseg()失败, 则说明重叠网络连接已关闭, 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作.
// 请查看服务端FSM以了解更多细节.
//

void *seghandler(void* arg) {
	char msg[1500];
	// stcp_hdr_t * stcp_header = (stcp_hdr_t *) msg;
	seg_t *seg_info = (seg_t *) msg; 
	while(1){
		memset(msg, 0, sizeof(char) * 1500);
		int srcID;
		int ret = sip_recvseg(SON_conn, &srcID, seg_info);
		if(ret == 0){
			stcp_hdr_to_host_order(&(seg_info->header));
			int i = 0;
			for (i = 0; i < MAX_TRANSPORT_CONNECTIONS; ++i){
				if(server_tcb_ptr[i] != NULL){
					if (server_tcb_ptr[i]->server_portNum == seg_info->header.dest_port /*&&
						server_tcb_ptr[i]->client_portNum == seg_info->header.src_port*/){
						/* dispatch and change FSM*/
						// printf("[seghandler in for loop] i = %d , is not NULL\n",i );
						switch(server_tcb_ptr[i]->state){
							case CLOSED: break;//Assert(0, "[Seghandler] Invalid FSM use usage");
							case LISTENING:{
								 if (seg_info->header.type == SYN){
								 	server_tcb_ptr[i]->state = CONNECTED; 
								 	server_tcb_ptr[i]->client_portNum = seg_info->header.src_port;
								 	server_tcb_ptr[i]->client_nodeID = srcID;
								 	//pthread_cond_signal(&cond_wait_connected);
								 	Log("[Seghandler LISTENING] Get client connect request");
								 	// printf("[Seghandler LISTENING] server_port = %d, client_port = %d\n", seg_info->header.dest_port, seg_info->header.src_port);
								 }
								 break;
							}
							case CONNECTED:{
								if (server_tcb_ptr[i]->client_portNum == seg_info->header.src_port && 
									seg_info->header.type == SYN){
									xstcp_packet_send(i, SYNACK, NULL, 0);
									server_tcb_ptr[i]->expect_seqNum = seg_info->header.seq_num;
									// printf("[Seghandler CONNECTED] server_port = %d, client_port = %d\n", seg_info->header.dest_port, seg_info->header.src_port);
									Log("[Seghandler CONNECTED] Receive SYN and Response SYNACK");
								}
								else if (server_tcb_ptr[i]->client_portNum == seg_info->header.src_port
									 && seg_info->header.type == DATA){
									Log("[Seghandler CONNECTED] Receive Data");
									ReceiveBufferSave(i, seg_info);
								}
								else if (server_tcb_ptr[i]->client_portNum == seg_info->header.src_port
									 && seg_info->header.type == FIN){
									xstcp_packet_send(i, FINACK, NULL, 0);
									Log("[Seghandler CONNECTED] Accept FIN and Response FINACK");
									server_tcb_ptr[i]->state = CLOSEWAIT;
									if (close_wait_tid == -1){
										pthread_attr_t attr;
										pthread_attr_init(&attr);
										pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
										pthread_create(&close_wait_tid, &attr, xstcp_close_wait, &i);
										pthread_attr_destroy(&attr);
										close_wait_tid = -1;
									}
								}
								break;
							}
							case CLOSEWAIT:{
								if (server_tcb_ptr[i]->client_portNum == seg_info->header.src_port &&
									 seg_info->header.type == FIN){
									xstcp_packet_send(i, FINACK, NULL, 0);
									Log("[Seghandler CLOSEWAIT] Accept FIN and Response FINACK");
									refresh_close_wait[i] = 1;
									pthread_cond_signal(&cond_close_wait[i]);
								}
								break;
							}
							default: Assert(0, "[Seghandler default] Invalid FSM");
						}
						break;
					}
				}
			}
			if (i == MAX_TRANSPORT_CONNECTIONS){
				printf("[Seghandler out for loop] i = %i\n", i);
				printf("[Seghandler out for loop] the server_portNum is 0x%x\n", seg_info->header.dest_port);
				Assert(0, "[Seghandler] No destination. A Message is lost.");
			}
		}
		else if (ret == -1){
			pthread_exit(NULL);
		}
	}
  	return 0;
}
