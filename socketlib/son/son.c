//文件名: son/son.c
//
//描述: 这个文件实现SON进程
//SON进程首先连接到所有邻居, 然后启动listen_to_neighbor线程, 每个该线程持续接收来自一个邻居的进入报文, 并将该报文转发给SIP进程. 
//然后SON进程等待来自SIP进程的连接. 在与SIP进程建立连接之后, SON进程持续接收来自SIP进程的sendpkt_arg_t结构, 并将接收到的报文发送到重叠网络中.  
//
//创建日期: 2015年

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <sys/utsname.h>
#include <assert.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "../common/debug.h"
#include "son.h"
#include "../topology/topology.h"
#include "neighbortable.h"

//你应该在这个时间段内启动所有重叠网络节点上的SON进程
#define SON_START_DELAY 30

/**************************************************************/
//声明全局变量
/**************************************************************/

//将邻居表声明为一个全局变量 
nbr_entry_t* nt; 
//将与SIP进程之间的TCP连接声明为一个全局变量
int sip_conn; 

static pthread_mutex_t sip_conn_mutex = PTHREAD_MUTEX_INITIALIZER;

/**************************************************************/
//实现重叠网络函数
/**************************************************************/

// 这个线程打开TCP端口CONNECTION_PORT, 等待节点ID比自己大的所有邻居的进入连接,
// 在所有进入连接都建立后, 这个线程终止. 
void* waitNbrs(void* arg) {
	int NbrNum = topology_getNbrNumWithBiggerID();
	int connected = NbrNum;
	// int local_ID = topology_getMyNodeID();

	int listenfd, connfd;
	socklen_t clilen = sizeof(struct sockaddr);
	struct sockaddr_in servaddr, cliaddr;
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(CONNECTION_PORT);
	listenfd = socket(AF_INET, SOCK_STREAM, 0);

	bind(listenfd, (struct sockaddr *) &servaddr, sizeof(struct sockaddr));
	listen(listenfd, NbrNum);
	while(connected != 0){
		connfd = accept(listenfd, (struct sockaddr *)&cliaddr, &clilen);
		printf("cliaddr.sin_addr  = %s\n", inet_ntoa(cliaddr.sin_addr));
		printf("cliaddr.sin_addr.s_addr = 0x%x\n", cliaddr.sin_addr.s_addr);
		int ret = nt_addconn(nt, (cliaddr.sin_addr.s_addr >> 24) & 0xff, connfd);
		if (ret == -1){
			Assert(0, "Accept connection is Error.");
		}
		--connected;
	}
	pthread_exit(NULL);
}

// 这个函数连接到节点ID比自己小的所有邻居.
// 在所有外出连接都建立后, 返回1, 否则返回-1.
int connectNbrs() {
	//你需要编写这里的代码.
	int NbrNumWithLittleID = topology_getNbrNum() - topology_getNbrNumWithBiggerID();
	int connected = NbrNumWithLittleID;
	int local_ID = topology_getMyNodeID();
	int NbrNum = topology_getNbrNum();

	while(connected != 0){
		for(int i = 0; i < NbrNum; ++i){
			if(nt[i].nodeID < local_ID && nt[i].conn == -1){
				struct sockaddr_in servaddr;
				int sockfd;
				sockfd = socket(AF_INET, SOCK_STREAM, 0);

				memset(&servaddr, 0, sizeof(struct sockaddr_in));
				servaddr.sin_family = AF_INET;
				servaddr.sin_addr.s_addr = nt[i].nodeIP;
				servaddr.sin_port = htons(CONNECTION_PORT);
				printf("connect ip : %d\n", (nt[i].nodeIP >> 24) & 0xff);
				if(connect(sockfd, (struct sockaddr *) &servaddr, sizeof(struct sockaddr_in)) == -1){
					Assert(0, "ConnectNbrs Error.");
				}
				nt[i].conn = sockfd;
				nt[i].state = SON_CONNECTED;
				printf("connected 0x%x\n", nt[i].nodeIP);
				--connected;
			}
		}
	}
  	return 1;
}


void sip_hdr_to_network_order(sip_hdr_t *piphdr) {
	piphdr->src_nodeID = htonl(piphdr->src_nodeID);
	piphdr->dest_nodeID = htonl(piphdr->dest_nodeID);
	piphdr->type = htons(piphdr->type);
	piphdr->length = htons(piphdr->length);
}

void send_update_msg_to_SIP(int srcNode) {
	pkt_routeupdate_t update_msg;
	memset(&update_msg, 0, sizeof(update_msg));
	update_msg.entryNum = topology_getNodeNum();

	sip_pkt_t pkt;
	memset(&pkt, 0, sizeof(pkt));
	pkt.header.src_nodeID = srcNode;
	pkt.header.dest_nodeID = BROADCAST_NODEID;
	pkt.header.type = ROUTE_UPDATE;
	pkt.header.length = sizeof(update_msg.entryNum) + update_msg.entryNum * sizeof(update_msg.entry[0]);

	sip_hdr_to_network_order(&pkt.header);

	int *node_arr = topology_getNodeArray();
	for(int i = 0; i < update_msg.entryNum; i ++) {
		update_msg.entry[i].nodeID = node_arr[i];
		update_msg.entry[i].cost = INFINITE_COST;
	}

	memcpy(pkt.data, (char*)&update_msg, ntohs(pkt.header.length));

	forwardpktToSIP(&pkt, sip_conn);

	free(node_arr);
}

//每个listen_to_neighbor线程持续接收来自一个邻居的报文. 它将接收到的报文转发给SIP进程.
//所有的listen_to_neighbor线程都是在到邻居的TCP连接全部建立之后启动的. 
void* listen_to_neighbor(void* arg) {
	int idx = *(int *) arg;
	char msg[2000];
	int ret;
	sip_pkt_t *pkt = (sip_pkt_t *)msg;
	while(1){
		// ret = recv(nt[idx].conn, msg, 2000, 0);
		ret = recvpkt(pkt, nt[idx].conn);
		if(ret <= 0){
			printf("[listen_to_neighbor %d] recv info error. thread %d exit.\n", (nt[idx].nodeIP >> 24) & 0xff, idx);
			close(nt[idx].conn);
			nt[idx].state = SON_CLOSED;
			send_update_msg_to_SIP((nt[idx].nodeIP >> 24) & 0xff);
			pthread_exit(NULL);
		}

		if(sip_conn >= 0) {
			pthread_mutex_lock(&sip_conn_mutex);
			forwardpktToSIP(pkt, sip_conn);
			pthread_mutex_unlock(&sip_conn_mutex);
			printf("[thread %d] son recv message from neighbor %d.\n", idx, ntohl(pkt->header.src_nodeID));
		} else {
			printf("[thread %d] son recv message from neighbor %d. But local sip has not been constructed.\n", idx, ntohl(pkt->header.src_nodeID));
		}
	}
	return NULL;
}

//这个函数打开TCP端口SON_PORT, 等待来自本地SIP进程的进入连接. 
//在本地SIP进程连接之后, 这个函数持续接收来自SIP进程的sendpkt_arg_t结构, 并将报文发送到重叠网络中的下一跳. 
//如果下一跳的节点ID为BROADCAST_NODEID, 报文应发送到所有邻居节点.
void waitSIP() {
	int listenfd;
	socklen_t clilen;
	struct sockaddr_in servaddr;
	struct sockaddr cliaddr;
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SON_PORT);
	listenfd = socket(AF_INET, SOCK_STREAM, 0);

	bind(listenfd, (struct sockaddr *) &servaddr, sizeof(struct sockaddr));
	listen(listenfd, 1);
	sip_conn = accept(listenfd, &cliaddr, &clilen);


	sip_pkt_t	packet;
	int 		nextNode;
	int 		NbrNum = topology_getNbrNum();
	while(1){
		int ret = getpktToSend(&packet, &nextNode, sip_conn);
		if(ret != 1) {
			Log("getpktToSend failed.");
			return;
		}

		if(nextNode == BROADCAST_NODEID){
			for(int i =0; i < NbrNum; ++i){
				if(nt[i].state == SON_CONNECTED) {
					if(sendpkt(&packet, nt[i].conn) < 0)
						nt[i].state = SON_CLOSED;
				}
			}
		}
		else{
			for(int i = 0; i < NbrNum; ++i){
				if(nt[i].nodeID == nextNode){
					if(nt[i].state == SON_CONNECTED) {
						if(sendpkt(&packet, nt[i].conn) < 0)
							nt[i].state = SON_CLOSED;
					}
					break;
				}
			}
		}
		
	}
	return ;
}

//这个函数停止重叠网络, 当接收到信号SIGINT时, 该函数被调用.
//它关闭所有的连接, 释放所有动态分配的内存.
void son_stop() {
	close(sip_conn);
	nt_destroy(nt);
	topology_stop();
	//你需要编写这里的代码.
}


int main() {
	topology_init();
	//启动重叠网络初始化工作
	printf("Overlay network: Node %d initializing...\n",topology_getMyNodeID());	

	//创建一个邻居表
	nt = nt_create();
	//将sip_conn初始化为-1, 即还未与SIP进程连接
	sip_conn = -1;
	
	//注册一个信号句柄, 用于终止进程
	signal(SIGINT, son_stop);

	//打印所有邻居
	int nbrNum = topology_getNbrNum();
	int i;
	for(i=0;i<nbrNum;i++) {
		printf("Overlay network: neighbor %d:%d\n",i+1,nt[i].nodeID);
	}

	//启动waitNbrs线程, 等待节点ID比自己大的所有邻居的进入连接
	pthread_t waitNbrs_thread;
	pthread_create(&waitNbrs_thread,NULL,waitNbrs,(void*)0);

	//等待其他节点启动
	sleep(SON_START_DELAY);
	
	//连接到节点ID比自己小的所有邻居
	connectNbrs();

	//等待waitNbrs线程返回
	pthread_join(waitNbrs_thread,NULL);	

	//此时, 所有与邻居之间的连接都建立好了
	
	// ignore all SIGPIPE signal in threads
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGPIPE);
	if(pthread_sigmask(SIG_BLOCK, &set, NULL) != 0) {
		printf("pthread_sigmask error!\n");
	}

	//创建线程监听所有邻居
	for(i=0;i<nbrNum;i++) {
		int* idx = (int*)malloc(sizeof(int));
		*idx = i;
		pthread_t nbr_listen_thread;
		pthread_create(&nbr_listen_thread,NULL,listen_to_neighbor,(void*)idx);
	}
	printf("Overlay network: node initialized...\n");
	printf("Overlay network: waiting for connection from SIP process...\n");

	//等待来自SIP进程的连接
	waitSIP();
}
