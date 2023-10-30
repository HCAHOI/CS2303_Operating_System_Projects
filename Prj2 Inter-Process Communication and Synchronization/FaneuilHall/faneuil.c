/*
***************************************************************
This program is used to simulate the "Faneuil Hall Problem"
***************************************************************
 source file:
    faneuil.c

 compile:
    make

 usage:
    ./faneuil
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#define IMMIGRANT_TARGET 10
#define MOD 3

sem_t immigrant_confirmed;
sem_t immigrant_sit_down;

int immigrant_idx = 1;
int judge_idx = 1;
int spectator_idx = 1;

bool judge_in_hall = false;

//thread function
void* Immigrant(void *arg) {
    while (1) {
        sleep(rand() % MOD);

        //wait for judge to leave
        while(judge_in_hall) {
            sleep(1);
        }

        printf("Immigrant %d come in\n", immigrant_idx);
        sleep(rand() % MOD);
        printf("Immigrant %d check in\n", immigrant_idx);
        sleep(rand() % MOD);
        printf("Immigrant %d sit down\n", immigrant_idx);
        sem_post(&immigrant_sit_down);

        //wait judge confirm
        sleep(rand() % MOD);
        sem_wait(&immigrant_confirmed);
        printf("Immigrant %d swear\n", immigrant_idx);
        sleep(rand() % MOD);
        printf("Immigrant %d get Certificate\n", immigrant_idx);

        sleep(rand() % MOD);
        //wait no judge
        while(judge_in_hall) {
            sleep(1);
        }

        printf("Immigrant %d leave\n", immigrant_idx);

        if(immigrant_idx == IMMIGRANT_TARGET) {
            break;
        }

        immigrant_idx++;
    }
    return NULL;
}

void* Judge(void *arg) {
    while (1) {
        sleep(rand() % MOD);
        //come in
        sem_wait(&immigrant_sit_down);
        judge_in_hall = true;
        printf("Judge %d come in\n", judge_idx);

        //confirm
        sleep(rand() % MOD);
        printf("Judge %d confirm\n", judge_idx);
        sem_post(&immigrant_confirmed);

        //leave
        sleep(rand() % MOD);
        printf("Judge %d leave\n", judge_idx);
        judge_in_hall = false;

        if(immigrant_idx == IMMIGRANT_TARGET) {
            break;
        }

        judge_idx++;
    }
    return NULL;
}


void* Spectator(void *arg) {
    while (1) {
        sleep(rand() % MOD);
        //wait no judge
        while(judge_in_hall) {
            sleep(1);
        }

        //come in
        printf("Spectator %d come in\n", spectator_idx);

        //spectate
        sleep(rand() % MOD);
        printf("Spectator %d spectate\n", spectator_idx);

        //wait judge leave
        sleep(rand() % MOD);
        while(judge_in_hall) {
            sleep(1);
        }

        //leave
        printf("Spectator %d leave\n", spectator_idx);

        if(immigrant_idx == IMMIGRANT_TARGET) {
            break;
        }

        spectator_idx++;
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    if(argc != 1) {
        printf("Usage: ./faneuil\n");
        return 0;
    }

    printf("Begin run\n");

    // init semaphores
    sem_init(&immigrant_confirmed, 0, 0);
    sem_init(&immigrant_sit_down, 0, 0);

    // init random seed
    srand(time(NULL));

    // init threads
    pthread_t immigrant, judge, spectator;
    pthread_create(&immigrant, NULL, Immigrant, NULL);
    pthread_create(&judge, NULL, Judge, NULL);
    pthread_create(&spectator, NULL, Spectator, NULL);

    //wait for thread to exit
    pthread_join(immigrant, NULL);
    pthread_join(judge, NULL);
    pthread_join(spectator, NULL);

    // destroy semaphores
    sem_destroy(&immigrant_confirmed);
    sem_destroy(&immigrant_sit_down);


    return 0;
}