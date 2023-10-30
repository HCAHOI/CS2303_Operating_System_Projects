/*
************************************************************************************************
This program can compute the matrix multiplication with multi threads, based on the thread pool.
************************************************************************************************
 source file:
    multi.c

 compile:
    make

 usage:
  ./multi
        In this way, the program will read the matrix from the file "data.in" and compute the matrix multiplication.
        Then the result will be written to the file "data.out".
  ./multi [size]
        In this way, the program will generate a random matrix with given size to the file "random.in", and compute the
        matrix multiplication. Then the result will be written to the file "data.out".
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "data_generator.h"
#include "thread_pool.h"

#define THREAD_NUM 4

struct m_matrix {
    int **A;
    int **B;
    int **C;
};


struct Args {
    struct m_matrix *m_matrix;
    int row;
    int col;
    int size;
};


void compute(struct Args *args) {
    int sum = 0;
    for (int i = 0; i < args->size; ++i) {
        sum += args->m_matrix->A[args->row][i] * args->m_matrix->B[i][args->col];
    }
    args->m_matrix->C[args->row][args->col] = sum;
}

int main(int argc, char *argv[]) {
    if(argc != 1 && argc != 2) {
        printf("Usage: ./multi for read data from file \"data.in\"\n");
        printf("Usage: ./multi [size] for random matrix");
    }

    //read matrix size
    int size = 0;
    FILE *fp;

    if(argc == 1) {
        //read data from file "data.in"
        fp = fopen("data.in", "r");
    } else {
        for(int i = 0; i < strlen(argv[1]); i++) {
            if(argv[1][i] < '0' || argv[1][i] > '9') {
                printf("The matrix size must be a positive integer\n");
                return 1;
            }
        }
        size = stoi(argv[1]);
        gen(size);
        //read data from file "random.in"
        fp = fopen("random.in", "r");
    }

    if (fp == NULL) {
        printf("Error opening file");
        return 1;
    }

    //read n
    fscanf(fp, "%d", &size);

    //check newline
    char ch;
    ch = fgetc(fp);
    if (ch != '\n') {
        printf("No size at the beginning of file\n");
        return 1;
    }

    //init matrixs
    struct m_matrix matrixs;
    matrixs.A = malloc(size * sizeof(int *));
    matrixs.B = malloc(size * sizeof(int *));
    matrixs.C = malloc(size * sizeof(int *));
    for (int i = 0; i < size; ++i) {
        matrixs.A[i] = malloc(size * sizeof(int));
        matrixs.B[i] = malloc(size * sizeof(int));
        matrixs.C[i] = malloc(size * sizeof(int));
    }

    //init args
    struct Args **args = malloc(size * sizeof(struct Args *));
    for (int i = 0; i < size; ++i) {
        args[i] = malloc(size * sizeof(struct Args));
    }

    //read matrix A
    for(int i = 0; i < size; i++) {
        for(int j = 0; j < size; j++) {
            fscanf(fp, "%d", &matrixs.A[i][j]);
        }
        ch = fgetc(fp);
        if(ch != '\n') {
            printf("The matrix is not square\n");
            return 1;
        }
    }

    //read matrix B
    for(int i = 0; i < size; i++) {
        for(int j = 0; j < size; j++) {
            fscanf(fp, "%d", &matrixs.B[i][j]);
        }
        ch = fgetc(fp);
        if(ch != '\n') {
            printf("The matrix is not square\n");
            return 1;
        }
    }

    ch = fgetc(fp);
    if(ch != EOF) {
        printf("The matrix is not square\n");
        return 1;
    }

    fclose(fp);

    //init thread pool
    pool_t *pool = pool_create(THREAD_NUM);

    //compute
    clock_t st = clock();
    for (int i = 0; i < size; ++i)
    {
        for (int j = 0; j < size; ++j)
        {
            args[i][j].m_matrix = &matrixs;
            args[i][j].row = i;
            args[i][j].col = j;
            args[i][j].size = size;
            task_push(pool, (void *)compute, (void *)&args[i][j]);
        }
    }
    pool_wait(pool);
    clock_t ed = clock();

    //write matrix C to file
    if(argc == 1)
        fp = fopen("data.out", "w");
    else
        fp = fopen("random.out", "w");
    for(int i = 0; i < size; i++) {
        for(int j = 0; j < size; j++) {
            fprintf(fp, "%d ", matrixs.C[i][j]);
        }
        fprintf(fp, "\n");
    }
    fclose(fp);

    //free memory
    for (int i = 0; i < size; i++)
    {
        free(matrixs.A[i]);
        free(matrixs.B[i]);
        free(matrixs.C[i]);
        free(args[i]);
    }
    free(matrixs.A);
    free(matrixs.B);
    free(matrixs.C);
    free(args);
    pool_destroy(pool);
    printf("multi-thread algorithm time: %f\n", (double)(ed - st) / CLOCKS_PER_SEC);
    return 0;
}