
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "dvtable.h"
#include "../common/debug.h"
#include "../common/constants.h"
#include "../topology/topology.h"

//这个函数动态创建距离矢量表.
//距离矢量表包含n+1个条目, 其中n是这个节点的邻居数,剩下1个是这个节点本身.
//距离矢量表中的每个条目是一个dv_t结构,它包含一个源节点ID和一个有N个dv_entry_t结构的数组, 其中N是重叠网络中节点总数.
//每个dv_entry_t包含一个目的节点地址和从该源节点到该目的节点的链路代价.
//距离矢量表也在这个函数中初始化.从这个节点到其邻居的链路代价使用提取自topology.dat文件中的直接链路代价初始化.
//其他链路代价被初始化为INFINITE_COST.
//该函数返回动态创建的距离矢量表.
dv_t* dvtable_create() {
	int nr_nbr = topology_getNbrNum();
	int nr_node = topology_getNodeNum();
	int *nbr_arr = topology_getNbrArray();
	int *node_arr = topology_getNodeArray();

	dv_t *dvt = (dv_t*)malloc((nr_nbr + 1) * sizeof(dv_t));
	Assert(dvt != NULL, "Creating distance vector table failed!");

	// item 0 is myself
	dvt[0].nodeID = topology_getMyNodeID();
	dv_entry_t *dv_self = (dv_entry_t*)malloc(nr_node * sizeof(dv_entry_t));
	Assert(dv_self != NULL, "Creating dv_entry failed!");
	for(int j = 0; j < nr_node; j ++) {
		dv_self[j].nodeID = node_arr[j];
		dv_self[j].cost = topology_getCost(dvt[0].nodeID, node_arr[j]);
	}
	dvt[0].dvEntry = dv_self;

	// item 1... are neighbours
	for(int i = 0; i < nr_nbr; i ++) {
		dvt[i + 1].nodeID = nbr_arr[i];
		dv_entry_t *dv_ety = (dv_entry_t*)malloc(nr_node * sizeof(dv_entry_t));
		Assert(dv_ety != NULL, "Creating dv_entry failed!");
		for(int j = 0; j < nr_node; j ++) {
			dv_ety[j].nodeID = node_arr[j];
			dv_ety[j].cost = INFINITE_COST;
		}
		dvt[i + 1].dvEntry = dv_ety;
	}

	free(nbr_arr);
	free(node_arr);

	return dvt;
}

//这个函数删除距离矢量表.
//它释放所有为距离矢量表动态分配的内存.
void dvtable_destroy(dv_t* dvtable) {
	int nr_nbr = topology_getNbrNum();
	for(int i = 0; i <= nr_nbr; i ++) {
		free(dvtable[i].dvEntry);
	}

	free(dvtable);
}

//这个函数设置距离矢量表中2个节点之间的链路代价.
//如果这2个节点在表中发现了,并且链路代价也被成功设置了,就返回1,否则返回-1.
int dvtable_setcost(dv_t* dvtable,int fromNodeID,int toNodeID, unsigned int cost) {
	int nr_nbr = topology_getNbrNum();
	int nr_node = topology_getNodeNum();

	for(int i = 0; i <= nr_nbr; i ++) {
		if(dvtable[i].nodeID == fromNodeID) {
			for(int j = 0; j < nr_node; j ++) {
				if(dvtable[i].dvEntry[j].nodeID == toNodeID) {
					dvtable[i].dvEntry[j].cost = cost;
					return 1;
				}
			}
			return -1;
		}
	}

	return -1;
}

//这个函数返回距离矢量表中2个节点之间的链路代价.
//如果这2个节点在表中发现了,就返回链路代价,否则返回INFINITE_COST.
unsigned int dvtable_getcost(dv_t* dvtable, int fromNodeID, int toNodeID) {
	int nr_nbr = topology_getNbrNum();
	int nr_node = topology_getNodeNum();

	for(int i = 0; i <= nr_nbr; i ++) {
		if(dvtable[i].nodeID == fromNodeID) {
			for(int j = 0; j < nr_node; j ++) {
				if(dvtable[i].dvEntry[j].nodeID == toNodeID) {
					return dvtable[i].dvEntry[j].cost;
				}
			}
		}
	}

	return INFINITE_COST;
}

//这个函数打印距离矢量表的内容.
void dvtable_print(dv_t* dvtable) {
	int nr_nbr = topology_getNbrNum();
	int nr_node = topology_getNodeNum();
	int *node_arr = topology_getNodeArray();

	printf("%8s  ", "");
	for(int i = 0; i < nr_node; i ++) {
		printf("%8d  ", node_arr[i]);
	}
	printf("\n");

	for(int i = 0; i <= nr_nbr; i ++) {
		printf("%8d  ", dvtable[i].nodeID);
		for(int j = 0; j < nr_node; j ++) {
			printf("%8d  ", dvtable[i].dvEntry[j].cost);
		}
		printf("\n");
	}

	free(node_arr);
}
