#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <crypt.h>

#include "bruteforce.h"
#include "threadpool.h"

/* Test Strings for quick reference to copy
MD5 no salt key = '1' - $5$$wnhoDxNUJlXycXBX3TFylvsfBC5B3//3SrCpC50dqE8
*/

/*Begin prototypes*/
void increment_first_digit(char *);
void decrypt_task(tpool_t, void*);
/*End protoypes*/

typedef struct data {
    char *encrypted_hash;
    char test_string[10]; //keeping the length of strings small to test
    int test_string_length; //current length of string being tested
} data_t;

void increment_first_digit(char *old_string) {
    int digit = (int) old_string[0];
    char new_val = (char) ++digit;
    old_string[0] = new_val;
}

void decrypt_task(tpool_t pool, void* args) {
    data_t *data = (data_t*) args;

    //copy the strings to make it thread safe, otherwise we are potentially using strings in data through multiple threads
    char test_string[10];
    strcpy(test_string, data->test_string);
    char *hash = malloc(strlen(data->encrypted_hash) * sizeof(char));
    strcpy(hash, data->encrypted_hash);
    printf("Current test string=%s -- crypt val=%s -- expected hash=%s\n", test_string, crypt(test_string, hash), hash);
    if (strcmp(crypt(test_string, hash), hash) == 0) {
        puts("match\n");
        return;
    }
    puts("no match\n");
}

void bruteforce(unsigned int num_threads, const char *encrypted_hash) {
    tpool_t pool;
    pool = tpool_create(num_threads);
    data_t *data = malloc(sizeof(struct data));
    data->encrypted_hash = malloc(strlen(encrypted_hash) * sizeof(char));
    strcpy(data->encrypted_hash, encrypted_hash);
    data->test_string[0] = '0';
    data->test_string_length = 1;
    for(int i = 0; i <= 9; i++) {
        tpool_schedule_task(pool, decrypt_task, (void*) data);
        increment_first_digit(data->test_string);
    }
    tpool_join(pool);
}
