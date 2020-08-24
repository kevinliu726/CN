#include "opencv2/opencv.hpp"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <iostream>
using namespace cv;
using namespace std;

typedef struct{
	int length;
	int seqNumber;
	int ackNumber;
	int fin;
	int syn;
	int ack;
} header;

typedef struct{
	header head;
	char data[4096];
} segment;

void setIP(char *dst, char *src){
    if (strcmp(src, "0.0.0.0") == 0 || strcmp(src, "local") == 0 || strcmp(src, "localhost") == 0)
        sscanf("127.0.0.1", "%s", dst);
    else
        sscanf(src, "%s", dst);
    return;
}

void init_data(segment *seg){
    seg->head.fin = 0;
    seg->head.syn = 0;
    seg->head.ack = 0;
    strcpy(seg->data, "\0");
    return;
}

int main(int argc, char* argv[]){
    char receiverIP[64], agentIP[64];
    int receiver_port, agent_port;
    segment Data, ACK;
    
    if (argc != 5){
        fprintf(stderr, "format : %s <receiver IP> <agent IP> <receiver port> <agent port>\n", argv[0]);
        fprintf(stderr, "example : ./receiver local local 8889 8888\n");
        exit(1);
    }
    else{
        setIP(receiverIP, argv[1]);
        setIP(agentIP, argv[2]);
        
        sscanf(argv[3], "%d", &receiver_port);
        sscanf(argv[4], "%d", &agent_port);
    }

	/* Create UDP socket */
    int receiversocket = socket(PF_INET, SOCK_DGRAM, 0);
    int sock_fd = receiversocket;
    int intg = 1;
    if(setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&intg, sizeof(intg)) < 0){
        perror("SOCKOPT");
    }
    struct sockaddr_in receiver, agent, tmp_addr;
    /* Configure settings in receiver struct */
    receiver.sin_family = AF_INET;
    receiver.sin_port = htons(receiver_port);
    receiver.sin_addr.s_addr = inet_addr(receiverIP);
    memset(receiver.sin_zero, '\0', sizeof(receiver.sin_zero));
    
    /* Configure settings in agent struct */
    agent.sin_family = AF_INET;
    agent.sin_port = htons(agent_port);
    agent.sin_addr.s_addr = inet_addr(agentIP);
    memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));

    /* bind socket */
    bind(receiversocket, (struct sockaddr*) &receiver, sizeof(receiver));

	/* Initialize size variable to be used later on */
    socklen_t agent_size = sizeof(agent);
    socklen_t tmp_size = sizeof(tmp_addr);

    printf("receiver info: ip = %s port = %d\n", receiverIP, receiver_port);
    Mat img;
	init_data(&Data);
	recvfrom(receiversocket, &Data, sizeof(Data), 0, (struct sockaddr *)&tmp_addr, &tmp_size);
    printf("recv    data    #%d\n", Data.head.seqNumber);

    init_data(&ACK);
    ACK.head.ack = ACK.head.ackNumber = 1;
    sendto(receiversocket, &ACK, sizeof(ACK), 0, (struct sockaddr *)&agent, agent_size);
    printf("send    ack     #%d\n", ACK.head.ackNumber);

	int width, height;
	sscanf(Data.data, "%d %d", &height, &width);
    img = Mat::zeros(height, width, CV_8UC3);
    if (!img.isContinuous())
        img = img.clone();

    int imgSize = img.total() * img.elemSize(), fptr = 0, id = 2;
    while(!ACK.head.fin){
        while(fptr < imgSize){
            recvfrom(receiversocket, &Data, sizeof(Data), 0, (struct sockaddr *)&tmp_addr, &tmp_size);
            printf("recv    data    #%d\n", Data.head.seqNumber);
            int copy_size = Data.head.length;
            if(Data.head.seqNumber > id) ACK.head.ackNumber = id - 1;
            else {
                ACK.head.ackNumber = id++;
                memcpy(img.data + fptr, Data.data, copy_size);
                fptr += copy_size;
            }
            sendto(receiversocket, &ACK, sizeof(ACK), 0, (struct sockaddr *)&agent, agent_size);
            printf("send    ack     #%d\n", ACK.head.ackNumber);
        }
        fptr = 0;
        recvfrom(receiversocket, &Data, sizeof(Data), 0, (struct sockaddr *)&tmp_addr, &tmp_size);
        printf("drop    data    #%d\n", Data.head.seqNumber);
        printf("flush\n");
        imshow("Video", img);
        char c = (char)waitKey(33.3333);
        if(c == 27) break;
        if(Data.head.fin){
            ACK.head.fin = 1;
            sendto(receiversocket, &ACK, sizeof(ACK), 0, (struct sockaddr *)&agent, agent_size);

            printf("recv    fin\n");
            printf("send    finack\n");
            break;
        }
    }
    destroyAllWindows();
    return 0;
}
