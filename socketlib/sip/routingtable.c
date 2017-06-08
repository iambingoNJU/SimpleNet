
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "routingtable.h"
#include "../common/debug.h"
#include "../common/constants.h"
#include "../topology/topology.h"

//makehash()是由路由表使用的哈希函数.
//它将输入的目的节点ID作为哈希键,并返回针对这个目的节点ID的槽号作为哈希值.
int makehash(int node) {
	return node % MAX_ROUTINGTABLE_SLOTS;
}

//这个函数动态创建路由表.表中的所有条目都被初始化为NULL指针.
//然后对有直接链路的邻居,使用邻居本身作为下一跳节点创建路由条目,并插入到路由表中.
//该函数返回动态创建的路由表结构.
routingtable_t* routingtable_create() {
	routingtable_t *prt = (routingtable_t*)malloc(sizeof(routingtable_t));
	Assert(prt != NULL, "Creating routing table failed!");
	for(int i = 0; i < MAX_ROUTINGTABLE_SLOTS; i ++) {
		prt->hash[i] = NULL;
	}

	int nr_nbr = topology_getNbrNum();
	int *nbr_arr = topology_getNbrArray();
	for(int i = 0; i < nr_nbr; i ++) {
		routingtable_setnextnode(prt, nbr_arr[i], nbr_arr[i]);
	}
	free(nbr_arr);

	Log("Routing table was created.");
	return prt;
}

//这个函数删除路由表.
//所有为路由表动态分配的数据结构将被释放.
void routingtable_destroy(routingtable_t* routingtable) {
	routingtable_entry_t *rt_hdr, *rt_tmp = NULL;
	for(int i = 0; i < MAX_ROUTINGTABLE_SLOTS; i ++) {
		rt_hdr = routingtable->hash[i];
		while(rt_hdr != NULL) {
			rt_tmp = rt_hdr;
			rt_hdr = rt_hdr->next;
			free(rt_tmp);
		}
	}

	free(routingtable);
	Log("Routing table was destroyed!");
}

//这个函数使用给定的目的节点ID和下一跳节点ID更新路由表.
//如果给定目的节点的路由条目已经存在, 就更新已存在的路由条目.如果不存在, 就添加一条.
//路由表中的每个槽包含一个路由条目链表, 这是因为可能有冲突的哈希值存在(不同的哈希键, 即目的节点ID不同, 可能有相同的哈希值, 即槽号相同).
//为在哈希表中添加一个路由条目:
//首先使用哈希函数makehash()获得这个路由条目应被保存的槽号.
//然后将路由条目附加到该槽的链表中.
void routingtable_setnextnode(routingtable_t* routingtable, int destNodeID, int nextNodeID) {
	int idx = makehash(destNodeID);
	routingtable_entry_t *rt_hdr = routingtable->hash[idx];
	while(rt_hdr != NULL) {
		if(rt_hdr->destNodeID == destNodeID) { // already exists
			rt_hdr->nextNodeID = nextNodeID;
			Log("Update routing table item. destNodeID[%d]'s nextNodeID becomes %d.", rt_hdr->destNodeID, rt_hdr->nextNodeID);
			return;
		}
		rt_hdr = rt_hdr->next;
	}

	// doesn't exist, insert from head
	routingtable_entry_t *new_rt_entry = (routingtable_entry_t*)malloc(sizeof(routingtable_entry_t));
	Assert(new_rt_entry != NULL, "Creating new routing table entry failed!");
	new_rt_entry->destNodeID = destNodeID;
	new_rt_entry->nextNodeID = nextNodeID;

	new_rt_entry->next = routingtable->hash[idx];
	routingtable->hash[idx] = new_rt_entry;
	Log("Insert routing table item. destNodeID: %d, nextNodeID: %d", new_rt_entry->destNodeID, new_rt_entry->nextNodeID);
}

//这个函数在路由表中查找指定的目标节点ID.
//为找到一个目的节点的路由条目, 你应该首先使用哈希函数makehash()获得槽号,
//然后遍历该槽中的链表以搜索路由条目.如果发现destNodeID, 就返回针对这个目的节点的下一跳节点ID, 否则返回-1.
int routingtable_getnextnode(routingtable_t* routingtable, int destNodeID) {
	int idx = makehash(destNodeID);
	routingtable_entry_t *rt_hdr = routingtable->hash[idx];
	while(rt_hdr != NULL) {
		if(rt_hdr->destNodeID == destNodeID) {
			return rt_hdr->nextNodeID;
		}
		rt_hdr = rt_hdr->next;
	}

	return -1;
}

//这个函数打印路由表的内容
void routingtable_print(routingtable_t* routingtable) {
	int cnt = 0;
	routingtable_entry_t *rt_hdr;
	printf("%5s - %10s - %10s\n", "ENTRY", "destNodeID", "nextNodeID");
	for(int i = 0; i < MAX_ROUTINGTABLE_SLOTS; i ++) {
		rt_hdr = routingtable->hash[i];
		while(rt_hdr != NULL) {
			printf("%5d - %10d - %10d\n", ++cnt, rt_hdr->destNodeID, rt_hdr->nextNodeID);
			rt_hdr = rt_hdr->next;
		}
	}
}
