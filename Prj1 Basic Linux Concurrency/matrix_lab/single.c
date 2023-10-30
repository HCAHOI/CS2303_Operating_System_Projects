/*
***********************************************************************
This program can compute the matrix multiplication with single thread.
***********************************************************************
 source file:
    single.c

 compile:
    make

 usage:
  ./single
        In this way, the program will read the matrix from the file "data.in" and compute the matrix multiplication.
        Then the result will be written to the file "data.out".
  ./single [size]
        In this way, the program will generate a random matrix with given size to the file "random.in", and compute
        the matrix multiplication. Then the result will be written to the file "data.out".
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include "data_generator.h"

void mul(int **A, int **B, int **C, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            C[i][j] = 0;
            for (int k = 0; k < n; k++) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if(argc != 1) {
        printf("No arguments needed\n");
        printf("Usage: ./single\n");
        return 1;
    }

    int size;
    clock_t st, ed;
    int** A, ** B, ** C;
    FILE *fp;

    fp = fopen("data.in", "r");
    if (fp == NULL) {
        printf("Error opening file\n");
        return 1;
    }

    //read size
    fscanf(fp, "%d", &size);

    //check newline
    char ch;
    ch = fgetc(fp);
    if (ch != '\n') {
        printf("No size at the beginning of file\n");
        return 1;
    }

    //allocate memory
    A = (int**)malloc(sizeof(int*) * size);
    B = (int**)malloc(sizeof(int*) * size);
    C = (int**)malloc(sizeof(int*) * size);
    for (int i = 0; i < size; i++) {
        A[i] = (int*)malloc(sizeof(int) * size);
        B[i] = (int*)malloc(sizeof(int) * size);
        C[i] = (int*)malloc(sizeof(int) * size);
    }

    //read matrix
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            fscanf(fp, "%d", &A[i][j]);
        }
        ch = fgetc(fp);
        if(ch != '\n') {
            printf("The matrix is not square\n");
            return 1;
        }
    }

    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            fscanf(fp, "%d", &B[i][j]);
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

    //origin matrix multiplication
    st = clock();
    mul(A, B, C, size);
    ed = clock();
    printf("Single thread time: %f\n", (double)(ed - st) / CLOCKS_PER_SEC);

    //write data to file "data.out"
    fp = fopen("data.out", "w");

    //write matrix C
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            fprintf(fp, "%d ", C[i][j]);
        }
        fprintf(fp, "\n");
    }

    fclose(fp);

    //free memory
    for (int i = 0; i < size; i++) {
        free(A[i]);
        free(B[i]);
        free(C[i]);
    }
    free(A);
    free(B);
    free(C);

    return 0;
}
