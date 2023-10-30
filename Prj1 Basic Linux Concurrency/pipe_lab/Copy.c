/*
***************************************************************
This program is used to copy a file to another file using pipe.
***************************************************************
 source file:
    Copy.c

 compile:
    make

 usage:
  ./Copy <InputFile> <OutputFile> <BufferSize>
 */
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>
#include <time.h>

size_t stou(char *str) {
    int res = 0;
    for(int i = 0; i < strlen(str); i++) {
        res = res * 10 + (str[i] - '0');
    }
    return res;
}

int main(int argc, char *argv[]) {
    if(argc != 4) {
        printf("Wrong number of arguments!\n");
        printf("Usage: ./Copy <InputFile> <OutputFile> <BufferSize>\n");
        exit(-1);
    }
    for(int i = 0; i < strlen(argv[3]); i++) {
        if(argv[3][i] < '0' || argv[3][i] > '9') {
            printf("Buffer size must be a positive integer!\n");
            printf("Usage: ./Copy <InputFile> <OutputFile> <BufferSize>\n");
            exit(-1);
        }
    }

    //init

    size_t BUFFER_SIZE = stou(argv[3]);
    int pid;
    int mypipe[2];
    clock_t st, ed;
    double elapsed;

    //check if file exists
    FILE *check = fopen(argv[1], "r");
    if(check == NULL) {
        printf("Error occurred during opening file!\n");
        exit(-1);
    }
    fclose(check);

    st = clock();
    if(pipe(mypipe) == 0) {
        pid = fork();
        if(pid == -1) {
            //throw error
            printf("Error occurred during creating child process!\n");
            exit(-1);
        } else if (pid == 0) {
            //child process: read from pipe and write to file
            FILE *dest = fopen(argv[2], "w+");
            char *readBuf = (char *) malloc(BUFFER_SIZE * sizeof(char));
            close(mypipe[1]);
            if(dest == NULL) {
                printf("Error occurred during opening file!\n");
                fclose(dest);
                exit(-1);
            }

            //read from pipe and write to file
            while(read(mypipe[0], readBuf, BUFFER_SIZE) > 0) {
                fwrite(readBuf, sizeof(char), BUFFER_SIZE, dest);
            }
            close(mypipe[0]);
            printf("Finish write.\n");

            //exit
            fclose(dest);
            free(readBuf);
            exit(0);
        } else {
            //parent process: read from file and write to pipe
            FILE *src = fopen(argv[1], "r");
            char *writeBuf = (char *) malloc(BUFFER_SIZE * sizeof(char));
            close(mypipe[0]);
            //check
            if(src == NULL) {
                printf("Error occurred during opening file!\n");
                exit(-1);
            }

            //read from file and write to pipe
            memset(writeBuf, 0, BUFFER_SIZE * sizeof(char));
            while(fread(writeBuf, sizeof(char), BUFFER_SIZE, src) > 0) {
                write(mypipe[1], writeBuf, BUFFER_SIZE);
                memset(writeBuf, 0, BUFFER_SIZE * sizeof(char));
            }
            close(mypipe[1]);
            printf("Finish read.\n");

            //exit
            fclose(src);
            free(writeBuf);
            wait(NULL);
        }
    } else {
        //throw error
        printf("Error occurred during creating pipe!\n");
        return -1;
    }
    ed = clock();

    elapsed = (double)(ed - st) / CLOCKS_PER_SEC * 1000;
    printf("Elapsed time: %f ms\n", elapsed);
    return 0;
}
