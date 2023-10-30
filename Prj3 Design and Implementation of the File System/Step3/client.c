/*
***************************************************************
This program is used to simulate the client operation.
***************************************************************
 source file:
    client.c

 compile:
    make

 usage:
    ./client <FSPort>
*/
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

#define dest_ip "127.0.0.1"
#define MAX_CMD_LEN 1024

//check if the string is a number
int is_nature_number(char *str) {
    for(int i = 0; i < strlen(str); i++) {
        if(str[i] < '0' || str[i] > '9') {
            return 0;
        }
    }
    return 1;
}

//convert string to unsigned int
int stoN(char *str) {
    int res = 0;
    for(int i = 0; i < strlen(str); i++) {
        res = res * 10 + (str[i] - '0');
    }
    return res;
}

int main(int argc, char *argv[]) {
    //check
    if(argc != 2 || !is_nature_number(argv[1])) {
        printf("Usage: ./client <FSPort>\n");
        return 0;
    }
    int fs_port = stoN(argv[1]);

    //connect to disk
    struct sockaddr_in dest_addr;
    int sockfd_between_fs_and_client = socket(AF_INET, SOCK_STREAM, 0);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(fs_port);
    dest_addr.sin_addr.s_addr = inet_addr(dest_ip);
    bzero(&(dest_addr.sin_zero), 8);

    //try to connect to disk
    int connect_trys = 0;
    while(connect_trys < 5) {
        if (connect(sockfd_between_fs_and_client, (struct sockaddr *) &dest_addr, sizeof(struct sockaddr)) == -1) {
            if(connect_trys == 4) {
                printf("Connect failed, please set up file system.\n");
                return 0;
            } else {
                connect_trys++;
                printf("Retry...\n");
                sleep(2);
            }
        } else {
            printf("Connect to disk successfully\n");
            break;
        }
    }

    char cmd[MAX_CMD_LEN];
    while(1) {
        printf("\033[1;32m>\033[0m ");
        memset(cmd, 0, sizeof(cmd));
        fgets(cmd, MAX_CMD_LEN, stdin);
        cmd[strlen(cmd) - 1] = '\0';
        if(cmd[0] == 'e') {
            strcpy(cmd, "exit");
            send(sockfd_between_fs_and_client, cmd, MAX_CMD_LEN, 0);
            close(sockfd_between_fs_and_client);
            printf("Exit\n");
            break;
        }
        //send command to file system
        send(sockfd_between_fs_and_client, cmd, MAX_CMD_LEN, 0);

        char buf[MAX_CMD_LEN];
        memset(buf, 0, sizeof(buf));
        recv(sockfd_between_fs_and_client, buf, MAX_CMD_LEN, 0);
        printf("%s", buf);
    }

    return 0;
}