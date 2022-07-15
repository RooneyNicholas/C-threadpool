#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <crypt.h>

#include "bruteforce.h"
#include "threadpool.h"

/* Test Strings for quick reference to copy
MD5 no salt -- key='1' - '$5$$wnhoDxNUJlXycXBX3TFylvsfBC5B3//3SrCpC50dqE8'
MD5 salt=12345 -- key=1234 -- $5$12345$aBMyVPBFcg0A2cuP8s6ESkeqSpRL6VI8G6BOXUE8JA2 --> given example in assignment
*/

/*Begin prototypes*/
void decrypt_task(tpool_t, void*);
/*End protoypes*/

typedef struct data {
    char *encrypted_hash;
    char test_string[10]; //keeping the length of strings small to test
    int test_string_length; //current length of string being tested
} data_t;

/*
Attempt to decrypt the hash, and if successful, print the string
If the test string does not match the hash, then append the digits 0-9, each as a new string, then schedule a new task
Avoids writing to the data_t args, to avoid any race condtions, keeping this function thread safe
*/
void decrypt_task(tpool_t pool, void* args) {
    data_t *data = (data_t*) args;
    // usleep(2000000);
    // printf("Current test string=%s -- crypt val=%s -- expected hash=%s\n", data->test_string, crypt(data->test_string, data->encrypted_hash), data->encrypted_hash );
    if (strcmp(crypt(data->test_string, data->encrypted_hash), data->encrypted_hash) == 0) {
        printf("%s\n", data->test_string);
        exit(0);
    }

    int string_length = data->test_string_length + 1;
    for(int i = 0; i <= 9 && string_length <= 10; i++) {
        data_t *new_data = malloc(sizeof(struct data));
        new_data->test_string_length = string_length;
        new_data->encrypted_hash = malloc( (strlen(data->encrypted_hash)+1) * sizeof(char));
        strcpy(new_data->encrypted_hash, data->encrypted_hash);
        strcpy(new_data->test_string, data->test_string);
        new_data->test_string[string_length - 1] = i + '0';
        // printf("new_data string=%s test_string_length val=%d\n", new_data->test_string, new_data->test_string_length);
        tpool_schedule_task(pool, decrypt_task, (void*) new_data);
    }
    free(args);
}

void bruteforce(unsigned int num_threads, const char *encrypted_hash) {
    tpool_t pool;
    pool = tpool_create(num_threads);
    for(int i = 0; i <= 9; i++) {
        data_t *data = malloc(sizeof(struct data));
        data->encrypted_hash = malloc( (strlen(encrypted_hash)+1) * sizeof(char));
        strcpy(data->encrypted_hash, encrypted_hash);
        data->test_string[0] = '\0';
        data->test_string_length = 0;
        tpool_schedule_task(pool, decrypt_task, (void*) data);
    }
    tpool_join(pool);
}
