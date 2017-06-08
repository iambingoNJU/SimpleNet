//文件名: topology/topology.c
//
//描述: 这个文件实现一些用于解析拓扑文件的辅助函数 
//
//创建日期: 2015年

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h> 
#include <sys/types.h>    
#include <sys/ioctl.h>
//#include <net/if.h>
#include <linux/if.h>   
#include "../son/neighbortable.h"
#include "topology.h"
#include "../common/constants.h"
#include "../common/debug.h"



static int NodeNum = 0;
static int TopoNum = 0;

static NodeInfo_t *NodeArray = NULL;
static TopoInfo_t *TopoArray = NULL;
char Local_IP[32];

// 仅仅用于统计结点个数
struct IpList{
	struct in_addr ip;
	struct IpList *next;
};

static struct IpList * IpListHead = NULL;
static int IpListNum = 0;
int IpList_Init(){
	IpListHead = malloc(sizeof(struct IpList));
	if (IpListHead == NULL){
		Assert(0, "[SON IpList] Malloc Wrong.");
	}
	IpListHead->ip.s_addr = 0;
	IpListHead->next = NULL;
	IpListNum = 0;
	return 0;
}
int IpList_Insert(struct in_addr ip){
	struct IpList *cur = malloc(sizeof(struct IpList));
	if (cur == NULL){
		Assert(0, "[SON IpList] Malloc Wrong.");
	}
	cur->ip = ip;
	cur->next = IpListHead;
	IpListHead = cur;
	++IpListNum;
	return 0;
}
int IpList_Find(struct in_addr ip){
	struct IpList *cur = IpListHead;
	while(cur != NULL){
		if (cur->ip.s_addr == ip.s_addr && cur->ip.s_addr != 0){
			return 1;
		}
		cur = cur->next;
	}
	if (cur == NULL)
		return 0;
	return 0;
}


int IpList_GetNum(){
	return IpListNum;
}
int IpList_Free(){
	struct IpList *cur = IpListHead; 
	struct IpList *ptr = IpListHead;
	for(cur = ptr; cur != NULL;){
		ptr = ptr->next;
		free(cur);
		cur = ptr;
	}
	return 0;
}

static void get_local_ip(){
	char * ip;
	int sockfd, interface;
	struct ifreq buf[INET_ADDRSTRLEN];
	struct ifconf ifc;
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if(sockfd == -1){
		perror("socket error");
		exit(1);
	}
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = (void *)buf;
	if(!ioctl(sockfd, SIOCGIFCONF,  (char *)&ifc)){
		interface = ifc.ifc_len/sizeof(struct ifreq);
		 while (interface > 0){
		 	--interface;
            if (!(ioctl(sockfd, SIOCGIFADDR, (char *)&buf[interface]))){
                ip = (inet_ntoa(((struct sockaddr_in*)(&buf[interface].ifr_addr))->sin_addr));
                printf("local ip:%s\n", ip);
            }
            else{
            	Log("Get local ip error!!!");
            	exit(1);
            }
	    }
	}
	else{
		Log("Get local ip error!!!");
		exit(1);
	}
	close(sockfd);
	for(int i = 0; i < ifc.ifc_len/sizeof(struct ifreq); ++i){
		ip = (inet_ntoa(((struct sockaddr_in*)(&buf[i].ifr_addr))->sin_addr));
		if(strcmp(ip, "127.0.0.1")){
			//Local_IP = ip;
			strcpy(Local_IP, ip);
			printf("choose ip: %s\n", Local_IP);
			break;
		}
	}
	return ;
}


int topology_init(){
	FILE *fp = fopen(TopologyDataPath, "r");
	if(fp == NULL)
		Assert(0, "[Topo] Fopen Topo data Error");
	char name1[MAXNAMELEN], name2[MAXNAMELEN];
	char NodeIP1[20], NodeIP2[20];
	unsigned int linkcost;
	IpList_Init();
	//仅仅用于统计结点个数与Topo关系个数
	while(fscanf(fp, "%s %s %s %s %u", name1, NodeIP1, name2, NodeIP2, &linkcost) == 5){
		++TopoNum;
		struct in_addr ip1, ip2;
		ip1.s_addr = inet_addr(NodeIP1);
		ip2.s_addr = inet_addr(NodeIP2);
		if (!IpList_Find(ip1))
			IpList_Insert(ip1);
		if (!IpList_Find(ip2))
			IpList_Insert(ip2);
	}
	NodeNum = IpList_GetNum();
	fseek(fp, 0, 0);

	NodeArray = (struct NodeInfo *) malloc(sizeof(struct NodeInfo) * NodeNum);
	TopoArray = (struct TopoInfo *) malloc(sizeof(struct TopoInfo) * TopoNum);
	int Nodeidx = 0, Topoidx = 0;
	while(fscanf(fp ,"%s %s %s %s %u", name1, NodeIP1, name2, NodeIP2, &linkcost) == 5){
		struct in_addr ip1, ip2;
		ip1.s_addr = inet_addr(NodeIP1);
		ip2.s_addr = inet_addr(NodeIP2);
		// Node 结点插入
		int node1_i, node2_i;
		for(node1_i = 0; node1_i < Nodeidx; ++node1_i){
			if(NodeArray[node1_i].addr.s_addr == ip1.s_addr)
				break;
		}
		if(node1_i == Nodeidx){
			NodeArray[Nodeidx].addr.s_addr = ip1.s_addr;
			strcpy(NodeArray[Nodeidx].name, name1);
			++Nodeidx;
		}
		for(node2_i = 0; node2_i < Nodeidx; ++node2_i){
			if(NodeArray[node2_i].addr.s_addr == ip2.s_addr)
				break;
		}
		if(node2_i == Nodeidx){
			NodeArray[Nodeidx].addr.s_addr = ip2.s_addr;
			strcpy(NodeArray[Nodeidx].name, name2);
			++Nodeidx;
		}
		// Topo结点插入
		TopoArray[Topoidx].node1_idx = node1_i;
		TopoArray[Topoidx].node2_idx = node2_i;
		TopoArray[Topoidx].linkcost = linkcost;
		++Topoidx;
	}
	IpList_Free();
	fclose(fp);
	get_local_ip();
	printf("MyNode ID: %d\n", topology_getMyNodeID());
	return 0;
}


int topology_stop(){
	free(NodeArray);
	free(TopoArray);
	return 0;
}

//这个函数返回指定主机的节点ID.
//节点ID是节点IP地址最后8位表示的整数.
//例如, 一个节点的IP地址为202.119.32.12, 它的节点ID就是12.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromname(char* hostname){
	for(int i = 0; i < NodeNum; ++i){
		if(!strcmp(hostname, NodeArray[i].name)){
			return (NodeArray[i].addr.s_addr >> 24) & 0xff;
		}
	}
	return -1;
}

//这个函数返回指定的IP地址的节点ID.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromip(struct in_addr* addr){
	for(int i = 0; i < NodeNum; ++i){
		if(addr->s_addr == NodeArray[i].addr.s_addr){
			return (NodeArray[i].addr.s_addr >> 24) & 0xff;
		}
	}
  	return -1;
}

//这个函数返回本机的节点ID
//如果不能获取本机的节点ID, 返回-1.
int topology_getMyNodeID(){
	struct in_addr local;
	local.s_addr = inet_addr(Local_IP);
	return (local.s_addr >> 24) & 0xff;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回邻居数.
int topology_getNbrNum(){
	struct in_addr local;
	local.s_addr = inet_addr(Local_IP);
	int cnt = 0;
	for(int i = 0; i < TopoNum; ++i){
		if( NodeArray[TopoArray[i].node1_idx].addr.s_addr == local.s_addr || 
			NodeArray[TopoArray[i].node2_idx].addr.s_addr == local.s_addr){
			++cnt;
		}
	}
	return cnt;
}

int topology_getNbrNumWithBiggerID(){
	int cnt = 0;
	struct in_addr local;
	local.s_addr = inet_addr(Local_IP);
	for(int i = 0; i < TopoNum; ++i){
		if(NodeArray[TopoArray[i].node1_idx].addr.s_addr == local.s_addr && 
			((local.s_addr >> 24) & 0xff) < ((NodeArray[TopoArray[i].node2_idx].addr.s_addr >> 24 ) & 0xff) ){
			++cnt;
		}
		else if (NodeArray[TopoArray[i].node2_idx].addr.s_addr == local.s_addr && 
			((local.s_addr >> 24) & 0xff) < ((NodeArray[TopoArray[i].node1_idx].addr.s_addr >> 24 ) & 0xff) ){
			++cnt;
		}
	}
  	return cnt;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回重叠网络中的总节点数.
int topology_getNodeNum(){
	return NodeNum;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含重叠网络中所有节点的ID. 
int* topology_getNodeArray(){
	int *IdArray = (int *) malloc(sizeof(int) * NodeNum);
	for(int i = 0; i < NodeNum; ++i){
		IdArray[i] = (NodeArray[i].addr.s_addr >> 24) & 0xff;
	}
  	return IdArray;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含所有邻居的节点ID.  
int* topology_getNbrArray(){
	struct in_addr local;
	local.s_addr = inet_addr(Local_IP);
	int ididx = 0;
	int *IdArray = (int *) malloc(sizeof(int) * topology_getNbrNum());
	for(int i = 0; i < TopoNum; ++i){
		if(NodeArray[TopoArray[i].node1_idx].addr.s_addr == local.s_addr){
			IdArray[ididx] = (NodeArray[TopoArray[i].node2_idx].addr.s_addr >> 24) & 0xff;
			++ididx;
		}
		else if (NodeArray[TopoArray[i].node2_idx].addr.s_addr == local.s_addr){
			IdArray[ididx] = (NodeArray[TopoArray[i].node1_idx].addr.s_addr >> 24) & 0xff;
			++ididx;
		}
	}
  	return IdArray;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回指定两个节点之间的直接链路代价. 
//如果指定两个节点之间没有直接链路, 返回INFINITE_COST.
unsigned int topology_getCost(int fromNodeID, int toNodeID){
	for(int i = 0; i < TopoNum; ++i){
		if( ( ((NodeArray[TopoArray[i].node1_idx].addr.s_addr >> 24) & 0xff) == fromNodeID && 
			  ((NodeArray[TopoArray[i].node2_idx].addr.s_addr >> 24) & 0xff) == toNodeID) ||
			( ((NodeArray[TopoArray[i].node1_idx].addr.s_addr >> 24) & 0xff) == toNodeID && 
			  ((NodeArray[TopoArray[i].node2_idx].addr.s_addr >> 24) & 0xff) == fromNodeID)	)
			return TopoArray[i].linkcost;
	}
  	return INFINITE_COST;
}

struct neighborentry *topology_getNbrEntry(){
	struct in_addr local;
	local.s_addr = inet_addr(Local_IP);
	int ididx = 0;
	nbr_entry_t *NbrArray = (nbr_entry_t *) malloc(sizeof(struct neighborentry) * topology_getNbrNum());
	for(int i = 0; i < TopoNum; ++i){
		if(NodeArray[TopoArray[i].node1_idx].addr.s_addr == local.s_addr){
			NbrArray[ididx].nodeID = (NodeArray[TopoArray[i].node2_idx].addr.s_addr >> 24) & 0xff;
			NbrArray[ididx].nodeIP = NodeArray[TopoArray[i].node2_idx].addr.s_addr;
			NbrArray[ididx].conn = -1;
			NbrArray[ididx].state = SON_CLOSED;
			++ididx;
		}
		else if (NodeArray[TopoArray[i].node2_idx].addr.s_addr == local.s_addr){
			NbrArray[ididx].nodeID = (NodeArray[TopoArray[i].node1_idx].addr.s_addr >> 24) & 0xff;
			NbrArray[ididx].nodeIP = NodeArray[TopoArray[i].node1_idx].addr.s_addr;
			NbrArray[ididx].conn = -1;
			NbrArray[ididx].state = SON_CLOSED;
			++ididx;
		}
	}
  	return NbrArray;
}
