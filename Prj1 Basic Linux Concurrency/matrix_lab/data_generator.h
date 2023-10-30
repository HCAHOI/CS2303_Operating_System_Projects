/*
**********************************************************************************
This library can generate a random matrix with given size to the file "random.in".
**********************************************************************************
 source file:
    data_generator.c

 usage:
    import this library to your code, and call the function gen(size).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

//convert string to int
int stoi(char *str)
{
    int res = 0;
    for (int i = 0; str[i] != '\0'; ++i)
        res = res * 10 + str[i] - '0';
    return res;
}

void gen(int n) {
    // generate a random matrix to file "random.in"
    srand(time(NULL));

    int **A = (int **)malloc(n * sizeof(int *));

    for (int i = 0; i < n; i++) {
        A[i] = (int *)malloc(n * sizeof(int));
        for (int j = 0; j < n; j++) {
            A[i][j] = rand() % 10;
        }
    }

    int **B = (int **)malloc(n * sizeof(int *));

    for (int i = 0; i < n; i++) {
        B[i] = (int *)malloc(n * sizeof(int));
        for (int j = 0; j < n; j++) {
            B[i][j] = rand() % 10;
        }
    }

    //write
    FILE *fp;
    fp = fopen("random.in", "w");
    if (fp == NULL) {
        printf("Error opening file");
        return;
    }

    fprintf(fp, "%d\n", n);
    for(int i = 0; i < n; i++) {
        fprintf(fp, "%d", A[i][0]);
        for (int j = 1; j < n; j++) {
            fprintf(fp, " %d", A[i][j]);
        }
        fprintf(fp, "\n");
    }

    for(int i = 0; i < n; i++) {
        fprintf(fp, "%d", B[i][0]);
        for (int j = 1; j < n; j++) {
            fprintf(fp, " %d", B[i][j]);
        }
        fprintf(fp, "\n");
    }

    fclose(fp);

    //free
    for (int i = 0; i < n; i++) {
        free(A[i]);
        free(B[i]);
    }
    free(A);
    free(B);
}