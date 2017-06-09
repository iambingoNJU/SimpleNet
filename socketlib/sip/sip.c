//文件名: sip/sip.c
//
//描述: 这个文件实现SIP进程  
//
//创建日期: 2015年

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

#include "../common/debug.h"
#include "../common/constants.h"
#include "../common/pkt.h"
#include "../common/seg.h"
#include "../topology/topology.h"
#include "sip.h"
#include "nbrcosttable.h"
#include "dvtable.h"
#include "routingtable.h"

//SIP层等待这段时间让SIP路由协议建立路由路径. 
#define SIP_WAITTIME 60

/**************************************************************/
//声明全局变量
/**************************************************************/
int son_conn = -1; 		//到重叠网络的连接
int stcp_conn;			//到STCP的连接

nbr_cost_entry_t* nct;					//邻居代价表
dv_t* dv;								//距离矢量表
pthread_mutex_t* dv_mutex;				//距离矢量表互斥量
routingtable_t* routingtable;			//路由表
pthread_mutex_t* routingtable_mutex;	//路由表互斥量

pthread_t pkt_handler_thread = -1; 
pthread_t routeupdate_thread = -1;

static pthread_mutex_t son_conn_mutex = PTHREAD_MUTEX_INITIALIZER;

void sip_hdr_to_network_order(sip_hdr_t *piphdr) {
	piphdr->src_nodeID = htonl(piphdr->src_nodeID);
	piphdr->dest_nodeID = htonl(piphdr->dest_nodeID);
	piphdr->type = htons(piphdr->type);
	piphdr->length = htons(piphdr->length);
}

void sip_hdr_to_host_order(sip_hdr_t *piphdr) {
	piphdr->src_nodeID = ntohl(piphdr->src_nodeID);
	piphdr->dest_nodeID = ntohl(piphdr->dest_nodeID);
	piphdr->type = ntohs(piphdr->type);
	piphdr->length = ntohs(piphdr->length);
}

/**************************************************************/
//实现SIP的函数
/**************************************************************/

//SIP进程使用这个函数连接到本地SON进程的端口SON_PORT.
//成功时返回连接描述符, 否则返回-1.
int connectToSON() { 
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	Assert(sockfd > 0, "create socket failed!");

	struct sockaddr_in localaddr;
	memset(&localaddr, 0, sizeof(localaddr));
	localaddr.sin_family = AF_INET;
	localaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	localaddr.sin_port = htons(SON_PORT);

	if(connect(sockfd, (struct sockaddr*)&localaddr, sizeof(localaddr)) == -1) {
		Log("SIP connects SON failed!");
		return -1;
	} else {
		Log("SIP connects SON succeeded!");
		return sockfd;
	}
}

//这个线程每隔ROUTEUPDATE_INTERVAL时间发送路由更新报文.路由更新报文包含这个节点
//的距离矢量.广播是通过设置SIP报文头中的dest_nodeID为BROADCAST_NODEID,并通过son_sendpkt()发送报文来完成的.
void* routeupdate_daemon(void* arg) {
	sip_pkt_t pkt;
	memset(&pkt, 0, sizeof(pkt));

	pkt_routeupdate_t update_msg;
	memset(&update_msg, 0, sizeof(update_msg));
	update_msg.entryNum = topology_getNodeNum();
	Assert(dv[0].nodeID == topology_getMyNodeID(), "dv item 0 should be myself DV!");
	for(int i = 0; i < update_msg.entryNum; i ++) {
		update_msg.entry[i].nodeID = dv[0].dvEntry[i].nodeID;
		update_msg.entry[i].cost = dv[0].dvEntry[i].cost;
	}

	pkt.header.src_nodeID = topology_getMyNodeID();
	pkt.header.dest_nodeID = BROADCAST_NODEID;
	pkt.header.type = ROUTE_UPDATE;
	pkt.header.length = sizeof(update_msg.entryNum) + update_msg.entryNum * sizeof(update_msg.entry[0]);
	memcpy(pkt.data, (char*)&update_msg, pkt.header.length);

	sip_hdr_to_network_order(&pkt.header);

	while(1) {
		pthread_mutex_lock(&son_conn_mutex);
		if(son_sendpkt(BROADCAST_NODEID, &pkt, son_conn) != 1) {
			Log("Sending packet failed.");
			break;
		}
		pthread_mutex_unlock(&son_conn_mutex);

		Log("Sending UPDATE ROUTE message!!!");
		sleep(ROUTEUPDATE_INTERVAL);
	}

	Log("routeupdate daemon thread exits.");
	pthread_exit(NULL);

	return NULL;
}

//这个线程处理来自SON进程的进入报文. 它通过调用son_recvpkt()接收来自SON进程的报文.
//如果报文是SIP报文,并且目的节点就是本节点,就转发报文给STCP进程. 如果目的节点不是本节点,
//就根据路由表转发报文给下一跳.如果报文是路由更新报文,就更新距离矢量表和路由表.
void* pkthandler(void* arg) {
	sip_pkt_t pkt;
	int myNodeID = topology_getMyNodeID();

	while(1) {
		if(son_recvpkt(&pkt, son_conn) != 1) {
			Log("[SIP] receiving son packet failed.");
			pthread_exit(NULL);
		}

		sip_hdr_to_host_order(&(pkt.header));

		if(pkt.header.dest_nodeID == BROADCAST_NODEID) {
			Assert(pkt.header.type == ROUTE_UPDATE, "Strange broadcast pakcet, but not ROUTE_UPDATE packet.");
			Log("SIP receiving route update packet.");
			pkt_routeupdate_t *update_msg = (pkt_routeupdate_t*)pkt.data;
			int srcNode = pkt.header.src_nodeID;
			// update dvtable item src_nodeID
			for(int i = 0; i < update_msg->entryNum; i ++) {
				dvtable_setcost(dv, srcNode, update_msg->entry[i].nodeID, update_msg->entry[i].cost); 
			}
			// update all dvtable item
			int nr_nbr = topology_getNbrNum();
			int nr_node = topology_getNodeNum();
			int idx = dvtable_getidx(dv, srcNode);
			for(int i = 0; i <= nr_nbr; i ++) {
				for(int j = 0; j < nr_node; j ++) {
					if(dv[i].dvEntry[j].cost > dv[i].dvEntry[idx].cost + dv[idx].dvEntry[j].cost) {
						dv[i].dvEntry[j].cost = dv[i].dvEntry[idx].cost + dv[idx].dvEntry[j].cost;
						if(i == 0) {
							routingtable_setnextnode(routingtable, dv[i].dvEntry[j].nodeID, srcNode);
						}
					}
				}
			}

		} else if(pkt.header.dest_nodeID == myNodeID) {
			Assert(pkt.header.type == SIP, "Strange packet to me, but not SIP packet.");
			if(stcp_conn > 0) {
				forwardsegToSTCP(stcp_conn, pkt.header.src_nodeID, (seg_t*)pkt.data);
				Log("SIP forwarding packet to STCP.");
			} else {
				Log("SIP receiving packet, but STCP connection hasn't beend constructed.");
			}
		} else {
			int nextNode = routingtable_getnextnode(routingtable, pkt.header.dest_nodeID);
			if(nextNode != -1) {
				sip_hdr_to_network_order(&(pkt.header));
				pthread_mutex_lock(&son_conn_mutex);
				if(son_sendpkt(nextNode, &pkt, son_conn) != 1) {
					Log("SIP forwarding pakcet failed!");
				} else {
					Log("SIP forwarding packet to %d", ntohl(pkt.header.dest_nodeID));
				}
				pthread_mutex_unlock(&son_conn_mutex);
			} else {
				Log("This packet is not desting to me, and I can't forward it out!!!");
			}
		}
	}

	Log("pkthandler thread exits.");
	pthread_exit(NULL);
}

//这个函数终止SIP进程, 当SIP进程收到信号SIGINT时会调用这个函数. 
//它关闭所有连接, 释放所有动态分配的内存.
void sip_stop() {
	close(son_conn);
}

//这个函数打开端口SIP_PORT并等待来自本地STCP进程的TCP连接.
//在连接建立后, 这个函数从STCP进程处持续接收包含段及其目的节点ID的sendseg_arg_t. 
//接收的段被封装进数据报(一个段在一个数据报中), 然后使用son_sendpkt发送该报文到下一跳. 下一跳节点ID提取自路由表.
//当本地STCP进程断开连接时, 这个函数等待下一个STCP进程的连接.
void waitSTCP() {
	int listenfd = socket(AF_INET, SOCK_STREAM, 0);
	Assert(listenfd >= 0, "Creating listenfd error!");

	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(SIP_PORT);

	bind(listenfd, (struct sockaddr*)&servaddr, sizeof(struct sockaddr));

	listen(listenfd, 1);

	socklen_t clilen;
	struct sockaddr cliaddr;

	while(1) {
		stcp_conn = accept(listenfd, &cliaddr, &clilen);

		if(stcp_conn < 0) {
			perror("accept error");
		}
		Assert(stcp_conn > 0, "accept error!!!");

		Log("[SIP] STCP connected SIP!");

		int nextNode;
		seg_t seg;
		sip_pkt_t sip_pkt;

		while(1) {
			int ret = getsegToSend(stcp_conn, &nextNode, &seg);
			if(ret != 1) {
				Log("getsegToSend failed!");
				close(stcp_conn);
				stcp_conn = -1;
				break;
			}

			memset(&sip_pkt, 0, sizeof(sip_pkt));
			sip_pkt.header.src_nodeID = topology_getMyNodeID();
			sip_pkt.header.dest_nodeID = nextNode;
			sip_pkt.header.type = SIP;
			sip_pkt.header.length = sizeof(seg.header) + ntohs(seg.header.length);
			memcpy(&(sip_pkt.data), &seg, sip_pkt.header.length);

			sip_hdr_to_network_order(&sip_pkt.header);

			pthread_mutex_lock(&son_conn_mutex);
			if(son_sendpkt(nextNode, &sip_pkt, son_conn) != 1) {
				Log("SIP recv STCP data, but send to son failed.");
			}
			pthread_mutex_unlock(&son_conn_mutex);
		}
	}
}

int main(int argc, char *argv[]) {
	printf("SIP layer is starting, pls wait...\n");

	//初始化全局变量
	topology_init();
	nct = nbrcosttable_create();
	dv = dvtable_create();
	dv_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(dv_mutex,NULL);
	routingtable = routingtable_create();
	routingtable_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(routingtable_mutex,NULL);
	son_conn = -1;

	nbrcosttable_print(nct);
	dvtable_print(dv);
	routingtable_print(routingtable);

	//注册用于终止进程的信号句柄
	signal(SIGINT, sip_stop);

	//连接到本地SON进程 
	son_conn = connectToSON();
	if(son_conn<0) {
		printf("can't connect to SON process\n");
		exit(1);		
	}
	
	//启动线程处理来自SON进程的进入报文 
	pthread_t pkt_handler_thread; 
	pthread_create(&pkt_handler_thread,NULL,pkthandler,(void*)0);

	//启动路由更新线程 
	pthread_t routeupdate_thread;
	pthread_create(&routeupdate_thread,NULL,routeupdate_daemon,(void*)0);	

	printf("SIP layer is started...\n");
	printf("waiting for routes to be established\n");
	sleep(SIP_WAITTIME);
	routingtable_print(routingtable);

	//等待来自STCP进程的连接
	printf("waiting for connection from STCP process\n");
	waitSTCP(); 

}

