#ifndef PACKET_H
#define PACKET_H
#include <stdio.h>
#define SEQ_PACK 350
typedef struct 
{
        int sequenceNo;
	    int len;
	    char data[1400];
} customPacket;

//from server to client
typedef struct
{

	int length;
	int seqNo[SEQ_PACK];
} seqACK;

typedef struct 
{
	int data;
        int chunckSize;	
} nackFile;

#endif
