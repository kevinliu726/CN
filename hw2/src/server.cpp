#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string>
#include <iostream>

using namespace std;
#define ERR_EXIT(a) { perror(a); exit(1); }
#define max_fd 1024

void server_recv(int fd, char *buf, int size){
	int r = 0;
	while(r < size){
		int res = recv(fd, buf + r, size - sizeof(char) * r, 0);
		r += res;
	}
}
void server_send(int fd, char *buf, int size){
	int s = 0;
	while(s < size){
		int res = send(fd, buf + s, size - sizeof(char) * s, 0);
		s += res;
	}
}

typedef struct{
	int state, size;
	FILE *fp;
} Client;

void init_client(Client client[], int fd){
	client[fd].state = 0;
	client[fd].size = 0;
}

int initServer(unsigned short port){
	int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in sockaddr;
	memset(&sockaddr, 0, sizeof(sockaddr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	sockaddr.sin_port = htons(port);

	int tmp = 1;
	if(setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0){
		ERR_EXIT("setsockopt");
	}
	if(bind(listen_fd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0){
		ERR_EXIT("bind");
	}
	if(listen(listen_fd, 1024) < 0){
		ERR_EXIT("listen");
	}
	return listen_fd;
}

int main(int argc, char *argv[]){
	mkdir("server_dir", 0777);
	chdir("server_dir");
	int listen_fd = initServer(atoi(argv[1]));
	Client client[1024];
	client[listen_fd].state = 1;

	struct sockaddr_in cli_addr;
	fd_set read_set, write_set, r_master, w_master;
	FD_ZERO(&read_set);
	FD_ZERO(&write_set);
	FD_ZERO(&r_master);
	FD_ZERO(&w_master);
	FD_SET(listen_fd, &r_master);

	char read_buf[1024] = "";
	char write_buf[1024] = "";
	char read_cmd[1024] = "";
	char write_cmd[1024] = "";

	printf("Socket fd: %d\n", listen_fd);
	while(1){
		memcpy(&read_set, &r_master, sizeof(r_master));
		memcpy(&write_set, &w_master, sizeof(w_master));
		puts("Selecting...");
		select(1024, &read_set, &write_set, NULL, NULL);
		for(int i = 3; i < max_fd; i++){
			int fd = i;
			if(FD_ISSET(i, &read_set)){
				if(i == listen_fd){
					int addr_sz = sizeof(cli_addr);
					int client_fd = accept(listen_fd, (struct sockaddr*)&cli_addr, (socklen_t*)&addr_sz);
					if(client_fd < 0){
						if(errno == EINTR || errno == EAGAIN) continue;
						ERR_EXIT("accpet");
					}
					printf("getting connect from %s on fd %d.\n", inet_ntoa(cli_addr.sin_addr), client_fd);
					init_client(client, i);
					client[client_fd].state = 1;
					FD_SET(client_fd, &r_master);
				}
				else {
					if(client[i].state == 1){
						bzero(read_cmd, sizeof(read_cmd));
						server_recv(fd, read_cmd, 1024);
						printf("The command from %d is %s\n", fd, read_cmd);
						char todo[1024] = "", file_name[1024] = "";
						sscanf(read_cmd, "%s %s", todo, file_name);

						if(strcmp(todo,"ls")  == 0){
							getcwd(file_name, 1024);
							client[i].state = 5;
							char ls_buf[1024] = "";
							struct dirent *dir;
							DIR *d = opendir(file_name);
							while((dir = readdir(d)) != NULL){
								strcat(ls_buf, dir->d_name);
								strcat(ls_buf, " ");
							}
							server_send(fd, ls_buf, 1024);
						}
						else if(strcmp(todo, "get") == 0){
							printf("file_name: %s\n", file_name);
							client[i].fp = fopen(file_name, "rb");
							if(client[i].fp == NULL){
								puts("File doesn't exist");
								bzero(write_cmd, sizeof(write_cmd));
								sprintf(write_cmd, "The '%s' doesn't exist.", file_name);
								server_send(fd, write_cmd, 1024);
								continue;
							}
							client[i].state = 4;
							fseek(client[i].fp, 0L, SEEK_END);
							client[i].size = ftell(client[i].fp);
							fseek(client[i].fp, 0L, SEEK_SET);
							bzero(write_cmd, sizeof(write_cmd));
							sprintf(write_cmd, "%1024d", client[i].size);
							server_send(fd, write_cmd, 1024);
							FD_CLR(fd, &r_master);
							FD_SET(fd, &w_master);
						}
						else if(strcmp(todo, "put") == 0){
							client[i].state = 2;
							client[i].fp = fopen(file_name, "wb");
							bzero(read_cmd, sizeof(read_cmd));
							server_recv(fd, read_cmd, 1024);
							client[i].size = atoi(read_cmd);
							FD_SET(fd,&r_master);
						}
					}
					else if(client[i].state == 2){
						bzero(read_buf, sizeof(read_buf));
						server_recv(fd, read_buf, 1024);
						fwrite(read_buf, sizeof(char), min(1024, client[i].size), client[i].fp);
						client[i].size -= min(1024, client[i].size);
						if(client[i].size == 0){
							printf("Finish uploading from %d\n", fd);
							client[i].state = 1;
							fclose(client[i].fp);
						}
					}
					else if(client[i].state == 5){
						client[i].state = 1;
					}
				}
			}
			if(FD_ISSET(i, &write_set)){
				if(client[i].state == 4){
					bzero(write_buf, sizeof(write_buf));
					fread(write_buf, sizeof(char), min(1024, client[i].size), client[i].fp);
					server_send(fd, write_buf, 1024);
					client[i].size -= min(1024, client[i].size);
					if(client[i].size == 0){
						client[i].state = 1;
						FD_CLR(fd, &w_master);
						FD_SET(fd, &r_master);
						printf("Finish downloading to %d\n", fd);
						fclose(client[i].fp);
					}
				}
			}
		}
	}
}
