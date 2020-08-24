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
#include <ctype.h>
#include <string>
#include <iostream>
#include "constant.hpp"
using namespace std;
#define ERR_EXIT(a) { perror(a); exit(1); }

void client_recv(int conn_fd, char *buf, int size){
	int r = 0;
	while(r < size){
		int res = recv(conn_fd, buf + r, size - sizeof(char) * r, 0);
		r += res;
	}
}
void client_send(int conn_fd, char *buf, int size){
	int s = 0;
	while(s < size){
		int res = send(conn_fd, buf + s, size - sizeof(char) * s, 0);
		s += res;
	}
}

typedef struct{
	int state, size;
	FILE *fp;
} Client;

int initClient(char *ip, unsigned short port){
	int conn_fd = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in sockaddr;
	memset(&sockaddr, 0, sizeof(sockaddr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = inet_addr(ip);
	sockaddr.sin_port = htons(port);

	int tmp = 1;
	if(setsockopt(conn_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0){
		ERR_EXIT("setsockopt");
	}
	if(connect(conn_fd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0){
		ERR_EXIT("bind");
	}
	return conn_fd;
}

int main(int argc, char *argv[]){
	mkdir(CLIENT_DIR, 0777);
	chdir(CLIENT_DIR);

	char *ip = strtok(argv[1], ":");
	char *port = strtok(NULL, ":");
	int conn_fd = initClient(ip, atoi(port));

	char read_buf[1024] = "";
	char write_buf[1024] = "";
	char read_cmd[1024] = "";
	char write_cmd[1024] = "";

	Client client;
	string inp;
	while(1){
		printf("$ ");
		char cmd_buf[1024] = "";
		cin.getline(cmd_buf, 1020);
		string cmd = string(cmd_buf);
		char todo[1024] = "", file_name[1024] = "";
		sscanf(cmd_buf, "%s %s", todo, file_name);
		if(strcmp(todo, "ls") == 0){
			if(strlen(file_name) != 0){
				puts("Command format error.");
				continue;
			}
			client_send(conn_fd, cmd_buf, 1024);
			bzero(read_buf, sizeof(read_buf));
			client_recv(conn_fd, read_buf, 1024);
			printf("%s\n", read_buf);
		}
		else if(strcmp(todo, "get") == 0){
			if(strlen(file_name) == 0){
				puts("Command format error.");
				continue;
			}

			client_send(conn_fd, cmd_buf, 1024);
			client.size = 0;

			bzero(read_cmd, sizeof(read_cmd));
			client_recv(conn_fd, read_cmd, 1024);

			if(!isdigit(read_cmd[0])){
				puts(read_cmd);
				continue;
			}

			client.fp = fopen(file_name, "wb");
			client.size = atoi(read_cmd);
			while(client.size > 0){
				bzero(read_buf, sizeof(read_buf));
				client_recv(conn_fd, read_buf, 1024);
				fwrite(read_buf, sizeof(char), min(1024, client.size), client.fp);
				client.size -= min(client.size, 1024);
			}
			fflush(client.fp);
			fclose(client.fp);
		}
		else if(strcmp(todo, "put") == 0){
			client.size = 0;
			client.fp = fopen(file_name, "rb");
			if(strlen(file_name) == 0){
				puts("Command format error.");
			}
			else if(client.fp == NULL){
				printf("The %s doesn't exist.\n", file_name);
			}
			else {
				client_send(conn_fd, cmd_buf, 1024);

				fseek(client.fp, 0L, SEEK_END);
				client.size = ftell(client.fp);
				fseek(client.fp, 0L, SEEK_SET);
				
				bzero(write_cmd, sizeof(write_cmd));
				sprintf(write_cmd, "%1024d", client.size);
				client_send(conn_fd, write_cmd, 1024);

				while(client.size > 0){
					fprintf(stderr, "size: %d\n", client.size);
					bzero(write_buf, sizeof(write_buf));
					fread(write_buf, sizeof(char), min(1024, client.size), client.fp);
					client_send(conn_fd, write_buf, 1024);
					client.size -= min(client.size, 1024);
				}
				fclose(client.fp);
			}
		}
		else {
			puts("Command not found.");
		}
	}
}
