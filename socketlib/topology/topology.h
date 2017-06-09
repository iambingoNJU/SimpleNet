//文件名: topology/topology.h
//
//描述: 这个文件声明一些用于解析拓扑文件的辅助函数 
//
//创建日期: 2015年

#ifndef TOPOLOGY_H 
#define TOPOLOGY_H
#include <netdb.h>

#define TopologyDataPath "/home/bingo/Desktop/lab08/socketlib/topology/topology.dat"
// #define Local_IP "192.168.229.153"
#define MAXNAMELEN 50
#define IPLen 20
// #define INFINITE_COST 0xffffffff
extern char Local_IP[];
typedef struct NodeInfo{
	char name[MAXNAMELEN];
	struct in_addr addr;
} NodeInfo_t;

typedef struct TopoInfo{
	int node1_idx;
	int node2_idx;
	unsigned int linkcost;
} TopoInfo_t;

int topology_init();
int topology_stop();


//这个函数返回指定主机的节点ID.
//节点ID是节点IP地址最后8位表示的整数.
//例如, 一个节点的IP地址为202.119.32.12, 它的节点ID就是12.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromname(char* hostname); 

//这个函数返回指定的IP地址的节点ID.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromip(struct in_addr* addr);

//这个函数返回本机的节点ID
//如果不能获取本机的节点ID, 返回-1.
int topology_getMyNodeID();

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回邻居数.
int topology_getNbrNum(); 

int topology_getNbrNumWithBiggerID();

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回重叠网络中的总节点数. 
int topology_getNodeNum();

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含重叠网络中所有节点的ID.  
int* topology_getNodeArray(); 

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含所有邻居的节点ID.  
int* topology_getNbrArray(); 

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回指定两个节点之间的直接链路代价. 
//如果指定两个节点之间没有直接链路, 返回INFINITE_COST.
unsigned int topology_getCost(int fromNodeID, int toNodeID);

struct neighborentry *topology_getNbrEntry();
#endif
