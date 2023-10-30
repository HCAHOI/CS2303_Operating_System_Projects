#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

int stoi(char *str)
{
    int res = 0;
    for (int i = 0; str[i] != '\0'; ++i)
        res = res * 10 + str[i] - '0';
    return res;
}

int main(int argc, char **argv)
{
    // generate a random matrix
    int n = stoi(argv[1]);
    srand(time(NULL));

    int **A = (int **)malloc(n * sizeof(int *));

    for (int i = 0; i < n; i++) {
        A[i] = (int *)malloc(n * sizeof(int));
        for (int j = 0; j < n; j++) {
            A[i][j] = rand() % 10;
        }
    }

    // print matrix A to file "data.in"
    FILE *fp;
    fp = fopen("src.txt", "w");
    if (fp == NULL) {
        printf("Error opening file");
        return 1;
    }

    // write n
    fprintf(fp, "%d\n", n);

    // write matrix A three times
    for(int k = 0; k < 3; k++) {
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                fprintf(fp, "%d ", A[i][j]);
            }
            fprintf(fp, "\n");
        }
    }

    fclose(fp);

    // free memory
    for (int i = 0; i < n; i++) {
        free(A[i]);
    }
    free(A);
    return 0;
}