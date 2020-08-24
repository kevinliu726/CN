#include "opencv2/opencv.hpp"
#include <iostream>
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <net/if.h>
#include <unistd.h> 
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <signal.h>
#include <time.h>

#define BUFF_SIZE 1024
#define COMMAND 1024
using namespace std;
using namespace cv;

void func_sig_pipe(int signo){
    // ignore it
    return;
}

int client_send(int localSocket, char write_buf[], int len){
    int sent = 0;
    while (sent < len){
        int s = send(localSocket, write_buf + sent, len - sent, 0);
        if (s == -1) return -1;
        sent += s;
    }
    return 0;
}

int client_recv(int localSocket, char read_buf[], int len){
    int recd = 0;
    while(recd < len){
        int r = recv(localSocket, read_buf + recd, len - recd, 0);
        if (r == 0) return -1;
        recd += r;
    }
    return 0;
}

void client_rs(int mode, int localSocket, char read_com[], char write_com[], int size){
    // client recv command
    if (mode == 1){
        bzero(read_com, size);
        if (recv(localSocket, read_com, size, 0) == 0){
            printf("Server died!\n");
            _exit(0);
        }
    }
    // client send command
    else if (mode == 2){
        if (send(localSocket, write_com, size, 0) == -1){
            printf("Server died!\n");
            _exit(0);
        }
    }
    return;
}

int main(int argc, char *argv[])
{
    char IP[1024] = "", port[1024] = "", paste[1024] = "", *ptr;
    strcpy(paste, argv[1]);
    ptr = strchr(paste, ':');
    strncpy(IP, paste, (ptr - paste));
    strncpy(port, ptr + 1, strlen(paste) - (ptr - paste + 1));
    // printf("%d %s %s\n", (ptr - paste), IP, port);

    int localSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (localSocket == -1){
        printf("Fail to create a socket.\n");
        return 0;
    }

    struct sockaddr_in info;
    bzero(&info, sizeof(info));
    info.sin_family = PF_INET;
    info.sin_addr.s_addr = inet_addr(IP);
    info.sin_port = htons(atoi(port));

    int err = connect(localSocket, (struct sockaddr *)&info, sizeof(info));
    if (err == -1){
        printf("Connection error\n");
        return 0;
    }

    struct sigaction sig_pipe;
    sig_pipe.sa_handler = func_sig_pipe;
    sig_pipe.sa_flags = 0;
    sig_pipe.sa_flags |= SA_RESTART;
    sigemptyset(&sig_pipe.sa_mask);
    sigaction(SIGPIPE, &sig_pipe, NULL);

    mkdir("./client_dir", 0777);
    
    char read_buf[BUFF_SIZE] = "";
    char write_buf[BUFF_SIZE] = "";
    char read_com[COMMAND] = "";
    char write_com[COMMAND] = "";

    while (1){
	    char *ptr = NULL;
	    size_t len = 0;
        getline(&ptr, &len, stdin);

        char todo[1024] = "", filen[1024] = "";
        sscanf(ptr, "%s %s", todo, filen);

        if (strcmp(todo, "ls") == 0){
            if (strlen(filen) != 0)
                cout << "Command format error." << endl;

            else{
                bzero(write_com, sizeof(write_com));
                strcpy(write_com, ptr);
                client_rs(2, localSocket, read_com, write_com, COMMAND);
                client_rs(1, localSocket, read_com, write_com, COMMAND);

                bzero(read_buf, sizeof(read_buf));
                if (client_recv(localSocket, read_buf, atoi(read_com)) == -1){
                    printf("Server died!\n");
                    return -1;
                }
                
                read_buf[atoi(read_com)] = '\0';
                printf("%s", read_buf);
            }
        }

        else if (strcmp(todo, "put") == 0){
            char upload_file[1024];
            sprintf(upload_file, "./client_dir/%s", filen);
            FILE *fp = fopen(upload_file, "r");
            
            if (strlen(filen) == 0)
                printf("Command format error.\n");
            else if (fp == NULL)
                printf("The %s doesn’t exist.\n", filen);
            else{
                bzero(write_com, sizeof(write_com));
                strcpy(write_com, ptr);
                client_rs(2, localSocket, read_com, write_com, COMMAND);

                fseek(fp, 0, SEEK_END);
                int file_len = ftell(fp);
                fseek(fp, 0, SEEK_SET);
                // send file size to server first
                bzero(write_com, sizeof(write_com));
                snprintf(write_com, sizeof(write_com), "%d", file_len);
                client_rs(2, localSocket, read_com, write_com, COMMAND);

                int cur_len = 0;
                while (cur_len < file_len){
                    int to_send = BUFF_SIZE - 1;
                    if ((file_len - cur_len) < (BUFF_SIZE - 1))
                        to_send = file_len - cur_len;

                    bzero(write_buf, sizeof(write_buf));
                    fread(write_buf, sizeof(char), to_send, fp);
                    if (client_send(localSocket, write_buf, to_send) == -1){
                        printf("Server died!\n");
                        return -1;
                    }
                    cur_len += to_send;
                }
                fclose(fp);
            }
        }

        else if (strcmp(todo, "get") == 0){
            if (strlen(filen) == 0){
                printf("Command format error.\n");
                continue;
            }
            
            bzero(write_com, sizeof(write_com));
            strcpy(write_com, ptr);
            client_rs(2, localSocket, read_com, write_com, COMMAND);
            client_rs(1, localSocket, read_com, write_com, COMMAND);

            char str_cmp[1024] = "";
            sprintf(str_cmp, "The %s doesn’t exist.\n", filen);
            if (strcmp(read_com, str_cmp) == 0){
                printf("%s", read_com);
                continue;
            }

            char create_file[1024];
            sprintf(create_file, "./client_dir/%s", filen);
            FILE *fp = fopen(create_file, "w");
            int cur_len = 0, file_len = atoi(read_com);

            while (cur_len < file_len){
                int to_recv = BUFF_SIZE - 1;
                if ((file_len - cur_len) < (BUFF_SIZE - 1))
                    to_recv = file_len - cur_len;
                        
                bzero(read_buf, sizeof(read_buf));
                if (client_recv(localSocket, read_buf, to_recv) == -1){
                    printf("Server died!\n");
                    return -1;
                }
                fwrite(read_buf, 1, strlen(read_buf), fp);
                cur_len += to_recv;
            }
            fclose(fp);
        }

        else if (strcmp(todo, "play") == 0){
            char *mpg_ptr = strstr(filen, ".mpg");
            if (strlen(filen) == 0){
                printf("Command format error.\n");
                continue;
            }
            if (mpg_ptr == NULL){
                printf("The %s is not a mpg file.\n", filen);
                continue;
            }

            bzero(write_com, sizeof(write_com));
            strcpy(write_com, ptr);
            client_rs(2, localSocket, read_com, write_com, COMMAND);
            client_rs(1, localSocket, read_com, write_com, COMMAND);

            char str_cmp[1024] = "";
            sprintf(str_cmp, "The %s doesn’t exist.\n", filen);
            if (strcmp(read_com, str_cmp) == 0){
                printf("%s", read_com);
                continue;
            }      
            
            // recv width from server
            int width = atoi(read_com);
            // recv height from server
            client_rs(1, localSocket, read_com, write_com, COMMAND);
            int height = atoi(read_com);
                    
            Mat imgClient;
            imgClient = Mat::zeros(height, width, CV_8UC3);
            // ensure the memory is continuous (for efficiency issue.)
            if (!imgClient.isContinuous())
                imgClient = imgClient.clone();
            
            bzero(write_com, sizeof(write_com));
            strcpy(write_com, "Ready");
            client_rs(2, localSocket, read_com, write_com, COMMAND);
            client_rs(1, localSocket, read_com, write_com, COMMAND);
            
            while (strcmp(read_com, "EOF") != 0){
                // allocate a buffer to load the frame (there would be 2 buffers in the world of the Internet)
                int imgSize = atoi(read_com);
                uchar buffer[imgSize];
                bzero(buffer, sizeof(buffer));

                int recd = 0;
                while (recd < imgSize){
                    int r = recv(localSocket, buffer + recd, imgSize - recd, 0);
                    if (r == 0){
                        printf("Server died!\n");
                        _exit(0);
                    }
                    recd += r;
                }
            
                // copy a frame from buffer to the container on client
                uchar *iptr = imgClient.data;
                memcpy(iptr, buffer, imgSize);

                imshow("video", imgClient);
                // Press ESC on keyboard to exit
                // notice: this part is necessary due to openCV's design.
                // waitKey means a delay to get the next frame.
                char c = (char)waitKey(33.3333);
                if (c == 27){
                    bzero(write_com, sizeof(write_com));
                    strcpy(write_com, "ESC");
                    client_rs(2, localSocket, read_com, write_com, COMMAND);
                    break;
                }

                bzero(write_com, sizeof(write_com));
                strcpy(write_com, "Ready");
                client_rs(2, localSocket, read_com, write_com, COMMAND);
                client_rs(1, localSocket, read_com, write_com, COMMAND);
            }        
	        destroyAllWindows();
        }

        else
            printf("Command not found.\n");
    }
      
    close(localSocket);
    cout << "Socket closed" << endl;
    
    return 0;
}