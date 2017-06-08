
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "nbrcosttable.h"
#include "../common/debug.h"
#include "../common/constants.h"
#include "../topology/topology.h"

//这个函数动态创建邻居代价表并使用邻居节点ID和直接链路代价初始化该表.
//邻居的节点ID和直接链路代价提取自文件topology.dat. 
nbr_cost_entry_t* nbrcosttable_create() {
	// TODO
  return 0;
}

//这个函数删除邻居代价表.
//它释放所有用于邻居代价表的动态分配内存.
void nbrcosttable_destroy(nbr_cost_entry_t* nct) {
	nbr_cost_entry_t *tmp = NULL;
	while(nct != NULL) {
		tmp = nct;
		nct = nct->next;
		free(tmp);
	}

	Log("Neighbour cost table was destroyed.");
}

//这个函数用于获取邻居的直接链路代价.
//如果邻居节点在表中发现,就返回直接链路代价.否则返回INFINITE_COST.
unsigned int nbrcosttable_getcost(nbr_cost_entry_t* nct, int nodeID) {
	while(nct != NULL) {
		if(nct->nodeID == nodeID) {
			return nct->cost;
		}
		nct = nct->next;
	}

	return INFINITE_COST;
}

//这个函数打印邻居代价表的内容.
void nbrcosttable_print(nbr_cost_entry_t* nct) {
	int cnt = 0;
	printf("%5s - %6s - %4s\n", "ENTRY", "nodeID", "cost");
	while(nct != NULL) {
		printf("%5d - %6d - %4d\n", ++cnt, nct->nodeID, nct->cost);
		nct = nct->next;
	}
}
