//
// 文件名: seg.c

// 描述: 这个文件包含用于发送和接收STCP段的接口sip_sendseg() and sip_rcvseg(), 及其支持函数的实现. 
//
// 创建日期: 2015年
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <errno.h>

#include "seg.h"
#include "debug.h"
#include "lib-socket.h"



//
//
//  用于客户端和服务器的SIP API 
//  =======================================
//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: sip_sendseg()和sip_recvseg()是由网络层提供的服务, 即SIP提供给STCP.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//


// 通过重叠网络(在本实验中，是一个TCP连接)发送STCP段. 因为TCP以字节流形式发送数据, 
// 为了通过重叠网络TCP连接发送STCP段, 你需要在传输STCP段时，在它的开头和结尾加上分隔符. 
// 即首先发送表明一个段开始的特殊字符"!&"; 然后发送seg_t; 最后发送表明一个段结束的特殊字符"!#".  
// 成功时返回1, 失败时返回-1. sip_sendseg()首先使用send()发送两个字符, 然后使用send()发送seg_t,
// 最后使用send()发送表明段结束的两个字符.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int sip_sendseg(int connection, int dest_nodeID, seg_t* segPtr) {
	Assert(connection >= 0, "Invalid connection!");
	Assert(segPtr != NULL, "SegPtr in NULL!");

	int ret = 1;

	sendseg_arg_t seg_arg;
	memset(&seg_arg, 0, sizeof(seg_arg));
	seg_arg.nodeID = htonl(dest_nodeID);
	memcpy(&(seg_arg.seg), segPtr, sizeof(seg_t));

	int len = sizeof(seg_arg.nodeID) + sizeof(seg_arg.seg.header) + ntohs(seg_arg.seg.header.length);
	if(tcp_send_data(connection, (char*)&seg_arg, len) != 1) {
		Log("Sending segment error!");
		ret = -1;
	}

	Log("len = %d", len);

	return ret;
}

// 通过重叠网络(在本实验中，是一个TCP连接)接收STCP段. 我们建议你使用recv()一次接收一个字节.
// 你需要查找"!&", 然后是seg_t, 最后是"!#". 这实际上需要你实现一个搜索的FSM, 可以考虑使用如下所示的FSM.
// SEGSTART1 -- 起点 
// SEGSTART2 -- 接收到'!', 期待'&' 
// SEGRECV -- 接收到'&', 开始接收数据
// SEGSTOP1 -- 接收到'!', 期待'#'以结束数据的接收
// 这里的假设是"!&"和"!#"不会出现在段的数据部分(虽然相当受限, 但实现会简单很多).
// 你应该以字符的方式一次读取一个字节, 将数据部分拷贝到缓冲区中返回给调用者.
//
// 注意: 还有一种处理方式可以允许"!&"和"!#"出现在段首部或段的数据部分. 具体处理方式是首先确保读取到!&，然后
// 直接读取定长的STCP段首部, 不考虑其中的特殊字符, 然后按照首部中的长度读取段数据, 最后确保以!#结尾.
//
// 注意: 在你剖析了一个STCP段之后,  你需要调用seglost()来模拟网络中数据包的丢失. 
// 在sip_recvseg()的下面是seglost()的代码.
// 
// 如果段丢失了, 就返回1, 否则返回0.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// 
int sip_recvseg(int connection, int* src_nodeID, seg_t* segPtr) {
	Assert(connection >= 0, "Invalid connection!");
	Assert(segPtr, "SegPtr is NULL!");

	sendseg_arg_t seg_arg;
	memset(&seg_arg, 0, sizeof(seg_arg));

	int recv_len = tcp_recv_data(connection, (char*)&seg_arg, sizeof(seg_arg));

	if(recv_len == -1) {
		Log("Can't receive data!");
		return -1;
	} else {
		Log("recv_len = %d, it should be checked!!!", recv_len);
		Assert(recv_len == (sizeof(seg_arg.nodeID) + sizeof(seg_arg.seg.header) + ntohs(seg_arg.seg.header.length)), "Unmatched recv_len!");
		*src_nodeID = ntohl(seg_arg.nodeID);
		Log("src_node: %d.\n", *src_nodeID);
		memcpy(segPtr, &(seg_arg.seg), recv_len - sizeof(seg_arg.nodeID));
		return 1;
		/*
		if (seglost(segPtr) == 1) {
			Log("[Seg recv] Seg is Lost, seq_num = %d.", ntohl(segPtr->header.seq_num));
			return 1;
		} else {
			if (checkchecksum(segPtr) == -1) {
				Log("[Seg recv] Seg is corrupted, seq_num = %d.", ntohl(segPtr->header.seq_num));
				return 1;
			} else {
				//Log("[Seg recv] src_port = 0x%x, dest_port = 0x%x", segPtr->header.src_port, segPtr->header.dest_port);
				return 0;
			}
		}
		*/
	}
}

//SIP进程使用这个函数接收来自STCP进程的包含段及其目的节点ID的sendseg_arg_t结构.
//参数stcp_conn是在STCP进程和SIP进程之间连接的TCP描述符.
//如果成功接收到sendseg_arg_t就返回1, 否则返回-1.
int getsegToSend(int stcp_conn, int* dest_nodeID, seg_t* segPtr) {
	Assert(stcp_conn >= 0, "Invalid stcp_conn!");
	Assert(segPtr != NULL, "segPtr is NULL!");
	Assert(dest_nodeID != NULL, "dest_nodeID is NULL!");

	sendseg_arg_t seg_arg;
	memset(&seg_arg, 0, sizeof(seg_arg));

	int recv_len = tcp_recv_data(stcp_conn, (char*)&seg_arg, sizeof(seg_arg));

	if(recv_len == -1) {
		Log("Can't receive data!");
		return -1;
	} else {
		Log("recv_len = %d, it should be checked!!!", recv_len);
		Assert(recv_len == (sizeof(seg_arg.nodeID) + sizeof(seg_arg.seg.header) + ntohs(seg_arg.seg.header.length)), "Unmatched recv_len!!!");
		*dest_nodeID = ntohl(seg_arg.nodeID);
		memcpy(segPtr, &(seg_arg.seg), recv_len - sizeof(seg_arg.nodeID));

		return 1;
	}
}

//SIP进程使用这个函数发送包含段及其源节点ID的sendseg_arg_t结构给STCP进程.
//参数stcp_conn是STCP进程和SIP进程之间连接的TCP描述符.
//如果sendseg_arg_t被成功发送就返回1, 否则返回-1.
int forwardsegToSTCP(int stcp_conn, int src_nodeID, seg_t* segPtr) {
	Assert(stcp_conn >= 0, "Invalid stcp_conn!");
	Assert(segPtr != NULL, "segPtr is NULL!");

	sendseg_arg_t seg_arg;
	memset(&seg_arg, 0, sizeof(seg_arg));
	seg_arg.nodeID = htonl(src_nodeID);
	memcpy(&(seg_arg.seg), segPtr, sizeof(seg_t));

	int len = (sizeof(seg_arg.nodeID) + sizeof(seg_arg.seg.header) + ntohs(seg_arg.seg.header.length));
	if(tcp_send_data(stcp_conn, (char*)&seg_arg, len) != 1) {
		Log("Sending segment error!");
		return -1;
	}

	Log("len = %d", len);

	return 1;
}

int seglost(seg_t* segPtr) {
	int random = rand() % 100;

	if(random < PKT_LOSS_RATE * 100) {
		//50%可能性丢失段
		if(rand() % 2 == 0) {
      		return 1;
		}
		//50%可能性是错误的校验和
		else {
			//获取数据长度
			int len = sizeof(stcp_hdr_t) + ntohs(segPtr->header.length);
			//获取要反转的随机位
			int errorbit = rand() % (len * 8);
			//反转该比特
			char* temp = (char*)segPtr;
			temp = temp + errorbit / 8;
			*temp = *temp ^ (1 << (errorbit % 8));

			return 0;
		}
	}

	return 0;
}



//这个函数计算指定段的校验和.
//校验和覆盖段首部和段数据. 你应该首先将段首部中的校验和字段清零, 
//如果数据长度为奇数, 添加一个全零的字节来计算校验和.
//校验和计算使用1的补码.
unsigned short checksum(seg_t* segment){
	unsigned int sum = 0;
	unsigned short *ptr = (unsigned short *) segment;
	int len = segment->header.length + sizeof(struct stcp_hdr);
	while(len > 1){
		sum += *(ptr++);
		len -= 2;
	}

	// if (len == 1){
	// 	sum += *(unsigned char *) ptr;
	// }
	if (len == 1){
		sum += (((unsigned short)*(unsigned char *) ptr) << 8);
	}
	while(sum >> 16){
		sum = (sum >> 16) + (sum & 0xffff);
	}

  	return (unsigned short)((~sum) & 0xffff);
}

//这个函数检查段中的校验和, 正确时返回1, 错误时返回-1
int checkchecksum(seg_t* segment){
	stcp_hdr_to_host_order(&(segment->header));
	unsigned short sum = checksum(segment);
	stcp_hdr_to_network_order(&(segment->header));
	//Log("checksum is  0x%x", sum);
	if (sum == 0) {
		return 1;
	} else {
		Log("Checksum error!");
	  	return -1;
	}
}


//used to change host order to network order
void stcp_hdr_to_network_order(stcp_hdr_t *phdr) {
	phdr->src_port = htonl(phdr->src_port);
	phdr->dest_port = htonl(phdr->dest_port);
	phdr->seq_num = htonl(phdr->seq_num);
	phdr->ack_num = htonl(phdr->ack_num);
	phdr->length = htons(phdr->length);
	phdr->type = htons(phdr->type);
	phdr->rcv_win = htons(phdr->rcv_win);
	phdr->checksum = htons(phdr->checksum);
}

//used to change network order to host order
void stcp_hdr_to_host_order(stcp_hdr_t *phdr) {
	phdr->src_port = ntohl(phdr->src_port);
	phdr->dest_port = ntohl(phdr->dest_port);
	phdr->seq_num = ntohl(phdr->seq_num);
	phdr->ack_num = ntohl(phdr->ack_num);
	phdr->length = ntohs(phdr->length);
	phdr->type = ntohs(phdr->type);
	phdr->rcv_win = ntohs(phdr->rcv_win);
	phdr->checksum = ntohs(phdr->checksum);
}
