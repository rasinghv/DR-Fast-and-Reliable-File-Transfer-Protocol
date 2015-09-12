/* A simple server in the internet domain using TCP
   The port number is passed as an argument */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "packetHeader.h"

#define RECV_LEN 1500
#define DATA_LEN 1400
#define IP_ADDR "127.0.0.1"
#define SEND_PORT 32000
#define RECV_PORT 32001
#define PACK_LEN 1400
#define HEAD_LEN 8
char *serverIPtoSendFileBack;

int fileSize = 0;

int *a;

typedef struct nackList{
	int num;
	struct nackList *next;
}nackList;

nackList *head = NULL;
nackList *tail = NULL;

int sockfd, portno;
socklen_t fromlen;
struct sockaddr_in serv_addr, from;
FILE *writefd;

int killParent = 0;
char *addr; //holds the pointer of the file returned by mmap
struct sockaddr_in servaddr; //the client sending socket structure
size_t fileLength; //fileLength of the file 

int maxSeqNo = 0;
int maxSeqNoSeen = 0;
int teardown = 0;
char *map;  /* mmapped array of int's */

void error(const char *msg){
	perror(msg);
	exit(1);
}

//Function to send a packet 
void sendPacket(int socket,customPacket* pack, size_t len, struct sockaddr_in to, socklen_t tolen){
	sendto(socket,(char*)pack,len,0,(struct sockaddr *)&to,tolen);
}
void sendPacketChar(int socket,nackFile* pack, size_t len, struct sockaddr_in to, socklen_t tolen){
	sendto(socket,(char*)pack,len,0,(struct sockaddr *)&to,tolen);
}


void handle_error(const char *msg){ 
	perror(msg);
	exit(EXIT_FAILURE);
}

int sender(char *filename);
void *recieveNACK(void *arg){
	socklen_t fromlen;
	unsigned char buffer[PACK_LEN+HEAD_LEN];
	//struct sockaddr_in serv_addr, from;
	struct sockaddr_in from;
	int n;
	fromlen = sizeof(struct sockaddr_in);
	seqACK *acks;
	customPacket *packet;
	packet=(customPacket* )malloc(sizeof(customPacket));
	while (1) {
		n = recvfrom(sockfd,buffer,PACK_LEN,0,NULL,NULL);//from,&fromlen);
		if (n < 0)  handle_error("recvfrom");

		if(n==1){
			killParent = 1;
			//printf("tear down");
			int t;
			customPacket *packet;
			packet=(customPacket* )malloc(sizeof(customPacket));
			//send Last packet again
			for(t=0;t<7;t++){
				packet->sequenceNo=maxSeqNo;
				packet->len=fileLength - (maxSeqNo-1)*1400;
				memcpy(packet->data,addr+1400*(maxSeqNo-1),packet->len);
				sendPacket(sockfd, packet, packet->len + HEAD_LEN, servaddr, sizeof(servaddr));
			}
			for(t=0;t<10;t++)
				sendto(sockfd,"t",1,0,(struct sockaddr *)&servaddr,sizeof(servaddr));

			break;
		}
		else {
			acks=(seqACK*)buffer;
			customPacket *packet;
			packet=(customPacket* )malloc(sizeof(customPacket));
			int j = 0;
			//printf("The ack packet lenght %d\n",acks->length);
			for(j=0; j<acks->length; j++){
				if (acks->seqNo[j]==0)continue;
				packet->sequenceNo = acks->seqNo[j];
				int pack_len = 0;
				if(acks->seqNo[j]*1400>(int)fileLength){
					pack_len = fileLength - (acks->seqNo[j]-1)*1400;
					packet->len = pack_len;
				}
				else{
					packet->len = 1400;
				}
				memcpy(packet->data,addr+1400*(acks->seqNo[j]-1),packet->len);
				sendPacket(sockfd, packet, packet->len + HEAD_LEN, servaddr, sizeof(servaddr));
			}

		}
		fflush(stdout);
	}
	return NULL;
}        



void* sendNack(void *arg){
	int old = 1;
	int new = 1;
	nackList *temp;
	nackList *tempnext;
	while(1){
		old = new;
		//kapil --- it was new =maxSeqNoSeen
		if(new == maxSeqNoSeen){
			new += 1000;
			if(new >= maxSeqNo)
				new = maxSeqNo;
		}
		else
			new = maxSeqNoSeen;
		int i;
		for(i = old; i <= new; i++){
			if(head == NULL && a[i] == 0){
				//printf("creating Head %d\n",i);
				head = (nackList*)malloc(sizeof(nackList));
				head -> num = i;
				head -> next = NULL;
				tail = head;
			}
			else{
				if(a[i] == 0){
					// printf("adding missing sequence to list: %d\n ",i );
					temp = (nackList*)malloc(sizeof(nackList));
					tail -> next = temp;
					temp -> num = i;
					temp -> next = NULL;
					tail = temp;
				}
			}
		}
		nackList *temp1; 
		temp1=head;
		/*printf("Printling list\n");
	        while(temp1!=NULL){
			printf("%d ",temp1->num);
			temp1=temp1->next;
		}*/
		temp = head;
		nackList *prev = NULL;
		seqACK *nackPacket = (seqACK *)malloc(sizeof(seqACK));    
		nackPacket->length = 0;
		while(temp != NULL){
			if(temp -> next == NULL && head==temp && a[temp->num] == 1){
				// printf("Deleting head\n");
				free(head);
				tail = NULL;
				head = NULL;
				temp = NULL;
			}
			else if(temp -> next == NULL && a[temp->num] == 1){
				//printf("Deleting tail %d\n",temp->num);
				free(temp);
				tail = prev;
				tail->next = NULL;
				temp = NULL;
			}
			else if(a[temp->num]==1){
				//printf("deleting element %d\n",temp->num);
				tempnext = temp -> next;
				temp -> num = tempnext -> num;
				temp -> next = tempnext -> next;
				if(tail == tempnext) tail = temp;
				free(tempnext);
			}
			else{
				if( nackPacket->length < 340 ){
					nackPacket->seqNo[nackPacket -> length] = temp->num;
					nackPacket -> length++;
				}
				if( nackPacket->length== 340 ){
					//send i to sender
					sendto(sockfd,(char*)nackPacket,sizeof(seqACK), 0,(struct sockaddr *) &from,fromlen);
					nackPacket->length = 0;
					memset(nackPacket->seqNo,0,340*sizeof(int));
				}
				prev = temp;
				temp = temp -> next;
			}
		}
		if (nackPacket->length != 0) 
			sendto(sockfd,(char*)nackPacket,sizeof(seqACK), 0,(struct sockaddr *) &from,fromlen);
		free(nackPacket);
		//more stronger
		if (head == NULL && old == maxSeqNo ){
			//sleep(10);
			//printf("Tearing down...................\n");
			teardown = 1;
			/*afor(i = 0; i <= maxSeqNo; i++)
				if(a[i]==0)
			 */
			int k=0;
			for(;k<10;k++){
				sendto(sockfd,"F",1, 0,(struct sockaddr *) &from,fromlen);
			}
			//printf("setting teardown %d\n",teardown);
			printf("FILE RECEIVED\n");
			pthread_exit(0);
		}
		sleep(2.37);
	}
}

int main(int argc, char *argv[]){
	int servPort = 0;
	char *fileToRead,*receiveFilename;
	serverIPtoSendFileBack=NULL;
	fileToRead=NULL;
	receiveFilename=NULL;
	int c;
	while ((c = getopt (argc, argv, "i:p:r:h")) != -1){
		switch (c)
		{
		case 'i':
			serverIPtoSendFileBack = optarg;
			break;
		case 'p':
			servPort = atoi(optarg);
			break;
		case 'r':
			receiveFilename = optarg;
			break;
		case 'h':
			fprintf(stderr, "<executable name> -i <client ip to send back to server> -p <port number> -r <receive filename> \n", optopt);
			exit(0);
		case '?':
		    if (optopt == 'i')
				fprintf(stderr, "Option -%c requires an argument\n", optopt);
			else if (optopt == 'p')
				fprintf(stderr, "Option -%c requires an argument\n", optopt);
			else if (optopt == 'r')
				fprintf(stderr, "Option -%c requires an argument\n", optopt);
		default:
			printf("Enter Proper Options\n");
			exit(0);
		}
	}
	if(servPort==0 ||receiveFilename==NULL || serverIPtoSendFileBack==NULL){
		fprintf(stderr, "<executable name> -f <file to transfer> -i <client ip to send back to server> -p <port number> -r <receive filename> \n", optopt);
		exit(0);
	}
	if(servPort==0){
		fprintf(stderr, "please enter port number of the server \n", optopt);
		exit(0);
	}
	if(receiveFilename==NULL){
		fprintf(stderr, "please enter the receive filename \n", optopt);
		exit(0);
	}
	if(serverIPtoSendFileBack==NULL){
		fprintf(stderr, "please enter the ip of the server \n", optopt);
		exit(0);
	}
	//
	unsigned char buffer[RECV_LEN];
	int n;

	//char *filenameToReceive=argv[2];
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");
	int sock_buf_size=1598029824;
	setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (char*)&sock_buf_size, sizeof(sock_buf_size));
	setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char*)&sock_buf_size, sizeof(sock_buf_size));
	bzero((char *) &serv_addr, sizeof(serv_addr));
	portno = atoi(argv[1]);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(servPort);
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR on binding");
	fromlen = sizeof(struct sockaddr_in);
	customPacket *packet;
	packet=(customPacket* )malloc(sizeof(customPacket));
	writefd = fopen(receiveFilename, "w");
	if (writefd == NULL) {
		perror("Error opening file for writing");
		exit(EXIT_FAILURE);
	}
	pthread_t handleNackThread;
	/*int err = pthread_create(&handleNackThread, NULL, &sendNack, NULL);
    	if (err != 0)
        	printf("\ncan't create thread :[%s]", strerror(err));*/
	int x=59919;
	while (1) {
		if(teardown)
			break;
		n = recvfrom(sockfd,buffer,RECV_LEN,0,(struct sockaddr *)&from,&fromlen);
		if(n == 1)
			break;
		if(strcmp("finished sending",buffer)==0){

		}
		if(n == 4 && fileSize == 0){
			fileSize = ((nackFile*)buffer)->data;
			fileLength=fileSize;
			//chunck_size=filesizefromclient->chunckSize;
			if(fileSize % 1400 == 0 )
				maxSeqNo = fileSize / 1400;
			else
				maxSeqNo = (fileSize / 1400) + 1;
			a = (int *)calloc((maxSeqNo+1),sizeof(int));
			a[0] = 1;

			int err = pthread_create(&handleNackThread, NULL, &sendNack, NULL);
			if (err != 0)
				printf("\ncan't create thread :[%s]", strerror(err));

		} 
		else if(n > 4){
			memcpy(packet, (customPacket*)buffer, n);
			if(maxSeqNoSeen < packet->sequenceNo)
				maxSeqNoSeen = packet->sequenceNo;;
			int addressToAdd = 0;
			addressToAdd = (packet -> sequenceNo - 1)*1400;
			//memcpy(map+addressToAdd,(char*)packet->data,packet->len);
			if(a[packet->sequenceNo]==0){
				fseek(writefd,addressToAdd,SEEK_SET);
				fwrite(packet->data,packet->len,1,writefd);
				fflush(writefd);
				a[packet -> sequenceNo] = 1;
			}
			if((packet->sequenceNo) >x){
				//printf("Send Next Chunck\n");
				//sendto(sockfd,"send next chunck",17, 0,(struct sockaddr *) &from,fromlen);

				x+=35000;
			}
		}
	}
	//printf(" Coming .......\n");

	//(void) pthread_join(handleNackThread, NULL);
	int j;
	for(j = 0; j< 10; j++)
		sendto(sockfd,"r",1, 0,(struct sockaddr *) &from,fromlen);
	//if (munmap(map, fileSize) == -1) 
	//    perror("Error un-mmapping the file");
	fflush(writefd);
	fclose(writefd);
	sleep(1);
	close(sockfd);
	free(a);
	//call return code here
	//sleep(4);
	//client code

	sleep(1);
	printf("SENDING FILE BACK TO CLIENT %s\n",serverIPtoSendFileBack);
	//char *filename ="copycata.bin";
	sender(receiveFilename);

	exit(0);
}



int sender(char *filename){   
	//declare packet structure!
	int fd;
	off_t offset, pa_offset;
	struct stat sb;
	sockfd = socket(AF_INET,SOCK_DGRAM,0);
	int sock_buf_size = 1598029824;
	setsockopt(sockfd,SOL_SOCKET,SO_SNDBUF,(char *)&sock_buf_size,sizeof(sock_buf_size));
	setsockopt(sockfd,SOL_SOCKET,SO_RCVBUF,(char *)&sock_buf_size,sizeof(sock_buf_size));
	bzero(&servaddr,sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(serverIPtoSendFileBack);

	servaddr.sin_port = htons(1234);

	fd = open(filename, O_RDONLY);
	if (fd == -1)
		handle_error("open");
	if (fstat(fd, &sb) == -1)           /* To obtain file size */
		handle_error("fstat");
	offset = 0;
	//printf("%d \n",sb.st_size):
	pa_offset = offset & ~(sysconf(_SC_PAGE_SIZE) - 1);
	/* offset for mmap() must be page aligned */
	if (offset >= sb.st_size) {
		//printf("%d %d\n",offset,sb.st_size);
		fprintf(stderr, "offset is past end of file\n");
		exit(EXIT_FAILURE);
	}

	fileLength = sb.st_size;

	addr = mmap(NULL, fileLength + offset - pa_offset, PROT_READ, MAP_PRIVATE, fd, pa_offset);
	if (addr == MAP_FAILED)
		handle_error("mmap");

	customPacket *packet;
	packet =(customPacket *)malloc(sizeof(customPacket));
	int i = 0;
	int seq = 1;
	int initial = 0;
	i = 0;

	int x= 146118;
	pthread_t recvThread;
	pthread_create(&recvThread,NULL,recieveNACK,NULL);
	while(i<(int)fileLength){
		memset(packet->data,0,PACK_LEN);
		//printf("%d\n",seq);
		initial = i;
		packet->sequenceNo=seq;
		if(i<(int)(fileLength)-PACK_LEN){
			i = i + PACK_LEN;
			packet->len=PACK_LEN;
			memcpy(packet->data,addr+initial,PACK_LEN);
			//printf("packet sent %d\n",seq);
			sendPacket(sockfd,packet,PACK_LEN+HEAD_LEN,servaddr,sizeof(servaddr));
		}
		else{
			i = fileLength;
			packet->len=fileLength - initial;
			//printf("packet lenght for the final----%d  %d  %d",packet->len,fileLength,initial);
			memcpy(packet->data,addr+initial,packet->len);
			sendPacket(sockfd,packet,packet->len+HEAD_LEN,servaddr,sizeof(servaddr));
		}
		seq++;
		if(seq%x==0){
			sleep(1);
		}

	}
	int p = 0;
	for(p=0;p<5;p++){
		//sleep(1);
		char *send = "finished sending"	;
		//sendto(sockfd,send,strlen(send),0,(struct sockaddr *)&servaddr,sizeof(servaddr));
	}
	(void) pthread_join(recvThread, NULL);

	printf("FILE SENT\n");
	exit(EXIT_SUCCESS);
}

