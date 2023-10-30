/*
***************************************************************
This program is used to simulate the disk operation.
***************************************************************
 source file:
    disk.c

 compile:
    make

 usage:
    ./disk <#cylinders> <#sector per cylinder> <track-to-track delay> <#disk-storage-filename>
*/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>
#include <time.h>
#include <gdcache.h>
#include <slcurses.h>

int CYLINDER_NUM, SECTOR_PER_CYLINDER, TRACK_TO_TRACK_DELAY;
char *disk_storage_filename;

#define SECTOR_SIZE 256
#define MAX_CMD_LEN 1024
#define true 1
#define false 0

//check if the string is a number
int is_number(char *str) {
    for(int i = 0; i < strlen(str); i++) {
        if(str[i] < '0' || str[i] > '9') {
            return 0;
        }
    }
    return 1;
}

//convert string to unsigned int
int stoi(char *str) {
    int res = 0;
    for(int i = 0; i < strlen(str); i++) {
        res = res * 10 + (str[i] - '0');
    }
    return res;
}

//parse command
int parse_cmd(char *cmd, char **argv) {
    cmd[strlen(cmd) - 1] = '\0';
    int argc = 0;
    char *p = strtok(cmd, " \n");
    while (p != NULL) {
        argv[argc++] = p;
        p = strtok(NULL, " \n");
    }
    return argc;
}

//simulate disk delay
void delay(int cur, int next) {
    int delay_time = abs(cur - next) * TRACK_TO_TRACK_DELAY;
    usleep(delay_time);
}

int main(int argc, char *argv[]) {
    //check argc
    if (argc != 5) {
        printf("Usage: ./disk <#cylinders> <#sector per cylinder> <track-to-track delay> <#disk-storage-filename>\n");
        return 0;
    }
    //check argv
    if (!is_number(argv[1]) || !is_number(argv[2]) || !is_number(argv[3])) {
        printf("Only positive number args are supported.\n");
        return 0;
    }
    //get args
    CYLINDER_NUM = stoi(argv[1]);
    SECTOR_PER_CYLINDER = stoi(argv[2]);
    TRACK_TO_TRACK_DELAY = stoi(argv[3]);
    disk_storage_filename = argv[4];
    if(CYLINDER_NUM <= 0 || SECTOR_PER_CYLINDER <= 0 || TRACK_TO_TRACK_DELAY <= 0) {
        printf("Only positive number args are supported.\n");
        return 0;
    }
    FILE *dsf = NULL;

    bool file_exist = false;
    if(access(disk_storage_filename, F_OK) == 0) {
        file_exist = true;
    }

    //create disk log
    FILE *log = fopen("disk.log", "w+");
    if (log == NULL) {
        printf("Error: cannot create disk log file.\n");
        return 0;
    }

    int cur_max_location = 0;
    //update max location if file exist
    if(file_exist) {
        dsf = fopen(disk_storage_filename, "r");
        fseek(dsf, 0, SEEK_END);
        cur_max_location = ftell(dsf) / SECTOR_SIZE;
        fclose(dsf);
    }

    //receive request
    while(1) {
        bool valid_request = true;
        int cur_cylinder = 0;

        char cmd[MAX_CMD_LEN] = "";
        fflush(stdin);
        fgets(cmd, MAX_CMD_LEN, stdin);
        fflush(stdin);
        char *cur_argv[MAX_CMD_LEN];
        int cur_argc = parse_cmd(cmd, cur_argv);

        if (strlen(cur_argv[0]) == 1) {
            switch (cur_argv[0][0]) {
                case 'I': {
                    if (cur_argc != 1) {
                        valid_request = false;
                        break;
                    }
                    fprintf(log, "%d %d\n", CYLINDER_NUM, SECTOR_PER_CYLINDER);
                    break;
                }
                case 'R': {
                    if(!file_exist) {
                        valid_request = false;
                        break;
                    }
                    //open file
                    dsf = fopen(disk_storage_filename, "r");
                    int cylinder, sector;
                    //check
                    if (cur_argc != 3) {
                        valid_request = false;
                        break;
                    }
                    if (!is_number(cur_argv[1]) || !is_number(cur_argv[2])) {
                        valid_request = false;
                        break;
                    }
                    cylinder = stoi(cur_argv[1]);
                    sector = stoi(cur_argv[2]);
                    //delay
                    delay(cur_cylinder, cylinder);
                    cur_cylinder = cylinder;
                    if (cylinder < 0 || cylinder >= CYLINDER_NUM || sector < 0 || sector >= SECTOR_PER_CYLINDER || cylinder * SECTOR_PER_CYLINDER + sector > cur_max_location) {
                        valid_request = false;
                        break;
                    }
                    //print file
                    fseek(dsf, cylinder * SECTOR_PER_CYLINDER * SECTOR_SIZE + sector * SECTOR_SIZE, 0);
                    char buf[SECTOR_SIZE];
                    memset(buf, 0, sizeof(buf));
                    fread(buf, sizeof(char), SECTOR_SIZE, dsf);
                    if (strlen(buf) == 0) {
                        valid_request = false;
                        break;
                    }
                    printf("YES: %s\n", buf);
                    fprintf(log, "YES: %s\n", buf);
                    fclose(dsf);
                    break;
                }
                case 'W': {
                    int cylinder, sector;
                    //open file
                    if(!file_exist) {
                        dsf = fopen(disk_storage_filename, "w+");
                        file_exist = true;
                    } else {
                        dsf = fopen(disk_storage_filename, "r+");
                    }
                    //check
                    if (cur_argc < 4) {
                        valid_request = false;
                        break;
                    }
                    if (!is_number(cur_argv[1]) || !is_number(cur_argv[2])) {
                        valid_request = false;
                        break;
                    }
                    cylinder = stoi(cur_argv[1]);
                    sector = stoi(cur_argv[2]);//delay
                    delay(cur_cylinder, cylinder);
                    cur_cylinder = cylinder;
                    if (cylinder < 0 || cylinder >= CYLINDER_NUM || sector < 0 || sector >= SECTOR_PER_CYLINDER) {
                        valid_request = false;
                        break;
                    }
                    //update cur_max_location
                    if (cylinder * SECTOR_PER_CYLINDER + sector > cur_max_location) {
                        cur_max_location = cylinder * SECTOR_PER_CYLINDER + sector;
                    }
                    //write remain args
                    fseek(dsf, cylinder * SECTOR_PER_CYLINDER * SECTOR_SIZE + sector * SECTOR_SIZE, 0);
                    int cur_len = 0;
                    char buf[SECTOR_SIZE];
                    for (int i = 3; i < cur_argc; i++) {
                        int len = strlen(cur_argv[i]);
                        if (cur_len + len + 1 > SECTOR_SIZE) {
                            valid_request = false;
                            break;
                        }
                        //add space
                        if (i != 3) {
                            buf[cur_len++] = ' ';
                        }
                        strcpy(buf + cur_len, cur_argv[i]);
                        cur_len += len;
                    }
                    if(!valid_request) {
                        break;
                    }
                    fwrite(buf, sizeof(char), SECTOR_SIZE, dsf);
                    fprintf(log, "YES\n");
                    fclose(dsf);
                    break;
                }
                case 'E': {
                    //exit
                    fprintf(log, "Goodbye!\n");
                    fclose(log);
                    return 0;
                }
                default: {
                    valid_request = false;
                    break;
                }
            }
        } else {
            valid_request = false;
        }

        if (!valid_request) {
            printf("Error: invalid request.\n");
            fprintf(log, "NO\n");
        }
    }

}
