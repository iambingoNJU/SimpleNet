//文件名: son/neighbortable.c
//
//描述: 这个文件实现用于邻居表的API
//
//创建日期: 2015年

#include "neighbortable.h"
#include "../topology/topology.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

//这个函数首先动态创建一个邻居表. 然后解析文件topology/topology.dat, 填充所有条目中的nodeID和nodeIP字段, 将conn字段初始化为-1.
//返回创建的邻居表.
nbr_entry_t* nt_create(){
	nbr_entry_t *nt = topology_getNbrEntry();
	return nt;
}

//这个函数删除一个邻居表. 它关闭所有连接, 释放所有动态分配的内存.
void nt_destroy(nbr_entry_t* nt){
	int NbrNum = topology_getNbrNum();
	for(int i = 0; i < NbrNum; ++i){
		close(nt[i].conn);
	}
	free(nt);
  	return;
}

//这个函数为邻居表中指定的邻居节点条目分配一个TCP连接. 如果分配成功, 返回1, 否则返回-1.
int nt_addconn(nbr_entry_t* nt, int nodeID, int conn){
	int NbrNum = topology_getNbrNum();
	for(int i = 0; i < NbrNum; ++i){
		if(nt[i].nodeID == nodeID){
			nt[i].conn = conn; 
			nt[i].state = SON_CONNECTED; 
			return 1;
		}
	}
  	return -1;
}
