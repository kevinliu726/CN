#include "opencv2/opencv.hpp"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>
#include <iostream>
using namespace std;
using namespace cv;

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
    memset(seg->data, 0, sizeof(seg->data));
    return;
}

int main(int argc, char* argv[]){
    char senderIP[64], agentIP[64], file_path[64];
    int sender_port, agent_port;
    segment Data;
    
    if (argc != 6){
        fprintf(stderr, "format : %s <sender IP> <agent IP> <sender port> <agent port> <path of source file>\n", argv[0]);
        fprintf(stderr, "example : ./sender local local 8887 8888 ./Data.mpg\n");
        exit(1);
    }
    else{
        setIP(senderIP, argv[1]);
        setIP(agentIP, argv[2]);
        
        sscanf(argv[3], "%d", &sender_port);
        sscanf(argv[4], "%d", &agent_port);
        sscanf(argv[5], "%s", file_path);
    }

    /* Create UDP socket */
    int sendersocket = socket(PF_INET, SOCK_DGRAM, 0);
    int sock_fd = sendersocket;
    int intg = 1;
    if(setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&intg, sizeof(intg)) < 0){
        perror("SOCKOPT");
    }
    struct sockaddr_in sender, agent, Data_addr;
    /* Configure settings in sender struct */
    sender.sin_family = AF_INET;
    sender.sin_port = htons(sender_port);
    sender.sin_addr.s_addr = inet_addr(senderIP);
    memset(sender.sin_zero, '\0', sizeof(sender.sin_zero));
    
    /* Configure settings in agent struct */
    agent.sin_family = AF_INET;
    agent.sin_port = htons(agent_port);
    agent.sin_addr.s_addr = inet_addr(agentIP);
    memset(agent.sin_zero, '\0', sizeof(agent.sin_zero));

    /* bind socket and set timeout*/
    bind(sendersocket, (struct sockaddr*) &sender, sizeof(sender));
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000;
    setsockopt(sendersocket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    /* Initialize size variable to be used later on */
    socklen_t agent_size = sizeof(agent);
    socklen_t Data_size = sizeof(Data_addr);

    printf("sender info: ip = %s port = %d\n", senderIP, sender_port);

    VideoCapture cap(file_path);
    Mat img;
    deque<segment> waitSent;
    deque<segment>::iterator it;
    it = waitSent.end();
    int window = 1, threshold = 16, id = 2, end = 0, win_idx = 0, ocnt = 0, fptr = 0;
    init_data(&Data);
    int h = cap.get(CV_CAP_PROP_FRAME_HEIGHT);
    int w = cap.get(CV_CAP_PROP_FRAME_WIDTH);
    img = Mat::zeros(h, w, CV_8UC3);
    if(!img.isContinuous())
        img = img.clone();
    
    int imgSize = img.total() * img.elemSize();
    cap >> img;
    sprintf(Data.data, "%d %d\n", img.rows, img.cols);
    Data.head.seqNumber = 1;
    waitSent.push_back(Data);
    sendto(sendersocket, &Data, sizeof(Data), 0, (struct sockaddr *)&agent, agent_size);
    printf("send    data    #%d,    winSize = %d\n", Data.head.seqNumber, window);
    it = waitSent.end();
    
    while(!end or !waitSent.empty()){
        init_data(&Data);
        while(win_idx < window and !(it == waitSent.end() and end)){
            if(it == waitSent.end()){
                Data.head.seqNumber = id ++;
                int dptr = 0;
                while(dptr < 4096){
                    if(fptr == imgSize){
                        cap >> img;
                        fptr = 0;
                        break;
                    }
                    if(img.empty()){
                        dptr = 0;
                        break;
                    }
                    int copy_size = min(imgSize - fptr, 4096 - dptr);
                    memcpy(Data.data + dptr, img.data + fptr, copy_size);
                    fptr += copy_size;
                    dptr += copy_size;
                }
                Data.head.length = dptr;
                if(Data.head.length == 0){
                    end = 1;
                    break;
                }
                waitSent.push_back(Data);
                int p = 0;
                while(p < sizeof(Data)){
                    int res = sendto(sendersocket, (&Data) + p, sizeof(Data) - p, 0, (struct sockaddr *)&agent, agent_size);
                    if(res == -1) res = 0;
                    p += res;
                }
                printf("send    data    #%d,    winSize = %d\n", Data.head.seqNumber, window);
                it = waitSent.end();
            } else {
                int p = 0;
                while(p < sizeof(*it)){
                    int res = sendto(sendersocket, &(*it) + p, sizeof(*it) - p, 0, (struct sockaddr *)&agent, agent_size);
                    if(res == -1) res = 0;
                    p += res;
                }
                printf("resend    data    #%d,    winSize = %d\n", (*it).head.seqNumber, window);
                ++it;
            }
            win_idx++;
        }
        win_idx = window;
        if(!waitSent.empty()){
            int res;
            int p = 0;
            while(p < sizeof(Data)){
                res = recvfrom(sendersocket, &Data, sizeof(Data), 0, (struct sockaddr *)&Data_addr, &Data_size);
                if(res == -1) break;
                p += res;
            }
            if(res == -1){
                threshold = max(window >> 1, 1);
                ocnt = 0;
                window = 1;
                win_idx = 0;
                it = waitSent.begin();
                printf("time    out,        threshold = %d\n", threshold);
                continue;
            }
            printf("recv    ack     #%d\n", Data.head.ackNumber);
            while(!waitSent.empty() and waitSent.front().head.seqNumber <= Data.head.ackNumber){
                waitSent.pop_front();
                win_idx--;
                if (window < threshold) window++;
                else ocnt++;
                if (ocnt == window){
                    window++;
                    ocnt = 0;
                }
            }
        }
    }
    init_data(&Data);
    Data.head.fin = 1;
    sendto(sendersocket, &Data, sizeof(Data), 0, (struct sockaddr *)&agent, agent_size);
    printf("send    fin\n");

    recvfrom(sendersocket, &Data, sizeof(Data), 0, (struct sockaddr *)&Data_addr, &Data_size);
    if (Data.head.fin == 1)
        printf("recv    finack\n");
    cap.release();
}
