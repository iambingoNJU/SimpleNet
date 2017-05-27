//
// �ļ���: seg.h

// ����: ����ļ�����STCP�ζ���, �Լ����ڷ��ͺͽ���STCP�εĽӿ�sip_sendseg() and sip_rcvseg(), ����֧�ֺ�����ԭ��. 
//
// ��������: 2015��
//

#ifndef SEG_H
#define SEG_H

#include "constants.h"

//�����Ͷ���, ����STCP.
#define	SYN			1
#define	SYNACK		2
#define	FIN			3
#define	FINACK		4
#define	DATA		5
#define	DATAACK		6

//���ײ����� 

typedef struct stcp_hdr {
	unsigned int src_port;        //Դ�˿ں�
	unsigned int dest_port;       //Ŀ�Ķ˿ں�
	unsigned int seq_num;         //���
	unsigned int ack_num;         //ȷ�Ϻ�
	unsigned short int length;    //�����ݳ���
	unsigned short int  type;     //������
	unsigned short int  rcv_win;  //��ʵ��δʹ��
	unsigned short int checksum;  //����ε�У���,��ʵ��δʹ��
} stcp_hdr_t;

//�ζ���

typedef struct segment {
	stcp_hdr_t header;
	char data[MAX_SEG_LEN];
	int data_len;
} seg_t;

//������SIP���̺�STCP����֮�佻�������ݽṹ.
//������һ���ڵ�ID��һ����. 
//��sip_sendseg()��˵, �ڵ�ID�Ƕε�Ŀ��ڵ�ID.
//��sip_recvseg()��˵, �ڵ�ID�Ƕε�Դ�ڵ�ID.
typedef struct sendsegargument {
	int nodeID;		//�ڵ�ID 
	seg_t seg;		//һ���� 
} sendseg_arg_t;

//
//  �ͻ��˺ͷ�������SIP API 
//  =======================================
//
//  �����������ṩ��ÿ���������õ�ԭ�Ͷ����ϸ��˵��, ����Щֻ��ָ���Ե�, ����ȫ���Ը����Լ����뷨����ƴ���.
//
//  ע��: sip_sendseg()��sip_recvseg()����������ṩ�ķ���, ��SIP�ṩ��STCP.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int sip_sendseg(int connection, seg_t* segPtr);

// ͨ���ص�����(�ڱ�ʵ���У���һ��TCP����)����STCP��. ��ΪTCP���ֽ�����ʽ��������, 
// Ϊ��ͨ���ص�����TCP���ӷ���STCP��, ����Ҫ�ڴ���STCP��ʱ�������Ŀ�ͷ�ͽ�β���Ϸָ���. 
// �����ȷ��ͱ���һ���ο�ʼ�������ַ�"!&"; Ȼ����seg_t; ����ͱ���һ���ν����������ַ�"!#".  
// �ɹ�ʱ����1, ʧ��ʱ����-1. sip_sendseg()����ʹ��send()���������ַ�, Ȼ��ʹ��send()����seg_t,
// ���ʹ��send()���ͱ����ν����������ַ�.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int sip_recvseg(int connection, seg_t* segPtr);

// ͨ���ص�����(�ڱ�ʵ���У���һ��TCP����)����STCP��. ���ǽ�����ʹ��recv()һ�ν���һ���ֽ�.
// ����Ҫ����"!&", Ȼ����seg_t, �����"!#". ��ʵ������Ҫ��ʵ��һ��������FSM, ���Կ���ʹ��������ʾ��FSM.
// SEGSTART1 -- ��� 
// SEGSTART2 -- ���յ�'!', �ڴ�'&' 
// SEGRECV -- ���յ�'&', ��ʼ��������
// SEGSTOP1 -- ���յ�'!', �ڴ�'#'�Խ������ݵĽ���
// ����ļ�����"!&"��"!#"��������ڶε����ݲ���(��Ȼ�൱����, ��ʵ�ֻ�򵥺ܶ�).
// ��Ӧ�����ַ��ķ�ʽһ�ζ�ȡһ���ֽ�, �����ݲ��ֿ������������з��ظ�������.
//
// ע��: ����һ�ִ���ʽ��������"!&"��"!#"�����ڶ��ײ���ε����ݲ���. ���崦��ʽ������ȷ����ȡ��!&��Ȼ��
// ֱ�Ӷ�ȡ������STCP���ײ�, ���������е������ַ�, Ȼ�����ײ��еĳ��ȶ�ȡ������, ���ȷ����!#��β.
//


//SIP����ʹ�����������������STCP���̵İ����μ���Ŀ�Ľڵ�ID��sendseg_arg_t�ṹ.
//����stcp_conn����STCP���̺�SIP����֮�����ӵ�TCP������.
//����ɹ����յ�sendseg_arg_t�ͷ���1, ���򷵻�-1.
int getsegToSend(int stcp_conn, int* dest_nodeID, seg_t* segPtr); 

//SIP����ʹ������������Ͱ����μ���Դ�ڵ�ID��sendseg_arg_t�ṹ��STCP����.
//����stcp_conn��STCP���̺�SIP����֮�����ӵ�TCP������.
//���sendseg_arg_t���ɹ����;ͷ���1, ���򷵻�-1.
int forwardsegToSTCP(int stcp_conn, int src_nodeID, seg_t* segPtr); 

// ע��: ����������һ��STCP��֮��,  ����Ҫ����seglost()��ģ�����������ݰ��Ķ�ʧ. 
// ������seglost()�Ĵ���.
// 
// ����ζ�ʧ��, �ͷ���1, ���򷵻�0. 
int seglost(seg_t* segPtr); 
// seglost��Դ����
// ����������seg.c��
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

//�����������ָ���ε�У���.
//У��ͼ��㸲�Ƕ��ײ��Ͷ�����. ��Ӧ�����Ƚ����ײ��е�У����ֶ�����, 
//������ݳ���Ϊ����, ���һ��ȫ����ֽ�������У���.
//У��ͼ���ʹ��1�Ĳ���.
unsigned short checksum(seg_t* segment);

//������������е�У���, ��ȷʱ����1, ����ʱ����-1.
int checkchecksum(seg_t* segment);

//used to change host order to network order
void stcp_hdr_to_network_order(stcp_hdr_t *phdr);

//used to change network order to host order
void stcp_hdr_to_host_order(stcp_hdr_t *phdr);

#endif
