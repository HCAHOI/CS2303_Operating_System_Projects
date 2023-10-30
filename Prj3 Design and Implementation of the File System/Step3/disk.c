/*
***************************************************************
This program is used to simulate the disk operation.
***************************************************************
 source file:
    disk.c

 compile:
    make

 usage:
    ./disk <#cylinders> <#sector per cylinder> <track-to-track delay> <#disk-storage-filename> <DiskPort>
*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>
#include <time.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/mman.h>

int cylinder_num, sector_per_cylinder, track_to_track_delay;
char *disk_storage_filename;
int fs_port;

#define SECTOR_SIZE 256
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

//parse command
int parse_cmd(char *cmd, char **argv) {
    int argc = 0;
    char *token = strtok(cmd, " ");
    while(token != NULL) {
        argv[argc] = token;
        argc++;
        token = strtok(NULL, " ");
    }
    return argc;
}

//simulate disk delay
void delay(int cur, int next) {
    int delay_time = abs(cur - next) * track_to_track_delay;
    usleep(delay_time);
}

int main(int argc, char *argv[]) {
    //check argc
    if (argc != 6) {
        printf("Usage: ./disk <#cylinders> <#sector per cylinder> <track-to-track delay> <#disk-storage-filename> <DiskPort>\n");
        return 0;
    }
    //check argv
    if (!is_nature_number(argv[1]) || !is_nature_number(argv[2]) || !is_nature_number(argv[3]) || !is_nature_number(argv[5])) {
        printf("Only positive number args are supported.\n");
        return 0;
    }
    //get args
    cylinder_num = stoN(argv[1]);
    sector_per_cylinder = stoN(argv[2]);
    track_to_track_delay = stoN(argv[3]);
    disk_storage_filename = argv[4];
    fs_port = stoN(argv[5]);
    //check args
    if(cylinder_num * sector_per_cylinder < 8) {
        printf("Error: the number sectors must be at least 8 to initialize the file system.\n");
        return 0;
    }

    //create disk log
    FILE *log = fopen("disk.log", "w+");
    if (log == NULL) {
        printf("Error: cannot create disk log file.\n");
        return 0;
    }

    //create socket
    int sockfd;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error opening socket");
        exit(2);
    }
    //bind socket
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(fs_port);
    //bind and check error
    int bind_status = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr));
    if (bind_status < 0) {
        perror("Error binding socket");
        exit(2);
    }
    //listen
    int listen_status = listen(sockfd, 5);
    if (listen_status < 0) {
        perror("Error listening");
        exit(2);
    }
    printf("Disk is listening on port %d...\n", fs_port);
    //accept
    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    int newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
    if (newsockfd < 0) {
        perror("Error accepting");
        exit(2);
    }
    printf("Disk is connected by file system.\n");

    //send the number of cylinders and sectors per cylinder to file system
    char msg[MAX_CMD_LEN];
    memset(msg, 0, MAX_CMD_LEN);
    sprintf(msg, "%d %d", cylinder_num, sector_per_cylinder);
    send(newsockfd, msg, MAX_CMD_LEN, 0);
    printf("Disk sent the number of cylinders and sectors per cylinder to file system.\n");

    bool disk_storage_file_exist = false;
    FILE *dsf = NULL;
    //create disk storage file
    if(access(disk_storage_filename, F_OK) != -1) {
        //file exists
        printf("Disk storage file already exists.\n");
        //send status code to file system
        memset(msg, 0, MAX_CMD_LEN);
        sprintf(msg, "%d", 1);
        send(newsockfd, msg, MAX_CMD_LEN, 0);
        printf("Disk sent status code to file system.\n");
        disk_storage_file_exist = true;
    } else {
        //file doesn't exist
        //send status code to file system
        memset(msg, 0, MAX_CMD_LEN);
        sprintf(msg, "%d", 0);
        send(newsockfd, msg, MAX_CMD_LEN, 0);
        printf("Disk sent status code to file system.\n");
        disk_storage_file_exist = false;
    }

    //receive request
    while(1) {
        bool valid_request = true;
        int cur_cylinder = 0;
        char op;
        //clean newsockfd buffer
        recv(newsockfd, &op, 1, 0);

        switch (op) {
            case 'R': {
                dsf = fopen(disk_storage_filename, "r");
                int cylinder, sector;

                recv(newsockfd, &cylinder, sizeof(int), 0);
                recv(newsockfd, &sector, sizeof(int), 0);

                printf("Disk received request: %c %d %d\n", op, cylinder, sector);
                //delay
                delay(cur_cylinder, cylinder);
                if (cylinder < 0 || cylinder >= cylinder_num || sector < 0 || sector >= sector_per_cylinder) {
                    fprintf(log, "NO\n");
                    break;
                }
                //print file
                fseek(dsf, cylinder * sector_per_cylinder * SECTOR_SIZE + sector * SECTOR_SIZE, 0);
                char buf[SECTOR_SIZE];
                memset(buf, 0, SECTOR_SIZE);
                fread(buf, sizeof(char), SECTOR_SIZE, dsf);
                //send sector to file system
                send(newsockfd, buf, SECTOR_SIZE, 0);
//                for(int i = 0; i < SECTOR_SIZE; i++) {
//                    printf("%d", buf[i]);
//                }
//                printf("\n");
                fprintf(log, "YES: %s\n", buf);
                fclose(dsf);
                break;
            }
            case 'W': {
                int cylinder, sector;
                if(disk_storage_file_exist == false) {
                    dsf = fopen(disk_storage_filename, "w+");
                    disk_storage_file_exist = true;
                } else {
                    dsf = fopen(disk_storage_filename, "r+");
                }
                //check
                recv(newsockfd, &cylinder, sizeof(int), 0);
                recv(newsockfd, &sector, sizeof(int), 0);

                printf("Disk received request: %c %d %d\n", op, cylinder, sector);

                delay(cur_cylinder, cylinder);
                cur_cylinder = cylinder;
                if (cylinder < 0 || cylinder >= cylinder_num || sector < 0 || sector >= sector_per_cylinder) {
                    printf("error args: cylinder or sector out of range\n");
                    valid_request = false;
                    break;
                }
                //write remain args
                fseek(dsf, cylinder * sector_per_cylinder * SECTOR_SIZE + sector * SECTOR_SIZE, 0);

                char buf[SECTOR_SIZE];
                memset(buf, 0, SECTOR_SIZE);
                recv(newsockfd, buf, SECTOR_SIZE, 0);

                fwrite(buf, sizeof(char), SECTOR_SIZE, dsf);
//                for(int i = 0; i < SECTOR_SIZE; i++) {
//                    printf("%d", buf[i]);
//                }
//                printf("\n");
                fprintf(log, "YES\n");
                fclose(dsf);
                break;
            }
            case 'E': {
                //exit
                printf("Goodbye!\n");
                fprintf(log, "Goodbye!\n");
                fclose(log);
                close(newsockfd);
                return 0;
            }
            default: {
                valid_request = false;
                break;
            }
        }

        if (!valid_request) {
            printf("Error: invalid request.\n");
            fprintf(log, "NO\n");
        }
    }

}
