/*
***************************************************************
This program is used to simulate the "Stooge Farmer Problem".
***************************************************************
 source file:
    LarryCurlyMoe.c

 compile:
    make

 usage:
    ./LCM <MaxUnfilledHoles>
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <stdlib.h>

size_t MAX_UNFILLED = 10;
size_t FILL_TARGET = 100;
size_t MOD = 3;
size_t stou(char *str) {
    size_t ret = 0;
    for (int i = 0; i < strlen(str); i++) {
        ret *= 10;
        ret += str[i] - '0';
    }
    return ret;
}

// semaphores
sem_t shovel;
sem_t empty_hole;
sem_t seed_hole;
int unfilled = 0;
int dig_idx = 1;
int seed_idx = 1;
int fill_idx = 1;

// Larry thread
void *Larry(void *arg) {
    while (1) {
        sleep(rand() % MOD);
        sem_wait(&shovel);
        if (unfilled < MAX_UNFILLED) {
            printf("Larry digs hole: %d.\n", dig_idx);
            unfilled++;
            sem_post(&empty_hole);
            dig_idx++;
        }
        sem_post(&shovel);

        if (dig_idx > FILL_TARGET) {
            break;
        }
    }
    return NULL;
}

// Curly thread
void *Curly(void *arg) {
    while (1) {
        sleep(rand() % MOD);
        sem_wait(&empty_hole);
        printf("Curly puts a seed in hole: %d\n", seed_idx);
        sem_post(&seed_hole);

        seed_idx++;

        if (seed_idx > FILL_TARGET) {
            break;
        }

    }
    return NULL;
}

// Moe thread
void *Moe(void *arg) {
    while (1) {
        sleep(rand() % MOD);
        sem_wait(&seed_hole);
	    sem_wait(&shovel);
        printf("Moe fills hole: %d\n", fill_idx);
        unfilled--;
        sem_post(&shovel);

        fill_idx++;

        if (fill_idx > FILL_TARGET) {
            break;
        }

    }
    return NULL;
}

int main(int argc, char *argv[]) {
    //check arguments
    if (argc != 2) {
        printf("Usage: ./LCM <MaxUnfilledHoles>\n");
        return 0;
    }
    for (int i = 0; i < strlen(argv[1]); i++) {
        if (argv[1][i] < '0' || argv[1][i] > '9') {
            printf("The argument must be a positive integer.\n");
            return 0;
        }
    }
    MAX_UNFILLED = stou(argv[1]);

    //set random seed
    srand(time(NULL));

    //print initial information
    printf("Maximum number of unfilled holes: %zu\n", MAX_UNFILLED);
    printf("Begin run\n");

    //initialize semaphores
    sem_init(&shovel, 0, 1);
    sem_init(&empty_hole, 0, 0);
    sem_init(&seed_hole, 0, 0);

    //initialize threads
    pthread_t larry, curly, moe;
    pthread_create(&larry, NULL, Larry, NULL);
    pthread_create(&curly, NULL, Curly, NULL);
    pthread_create(&moe, NULL, Moe, NULL);

    //wait for threads to exit
    pthread_join(larry, NULL);
    pthread_join(curly, NULL);
    pthread_join(moe, NULL);

    //destroy semaphores
    sem_destroy(&shovel);
    sem_destroy(&empty_hole);
    sem_destroy(&seed_hole);

    return 0;
}
