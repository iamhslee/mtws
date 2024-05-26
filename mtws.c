// 2024-1 Operating Systems (ITP30002) - HW #3
// File  : mtws.c
// Author: Hyunseo Lee (22100600) <hslee@handong.ac.kr>

/* MACROS */
#define _GNU_SOURCE
#define MAX_BUFFER_SIZE 100

/* HEADERS */
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* TYPE DEFINITIONS */
typedef struct {
    char *buffer[MAX_BUFFER_SIZE];
    int size;
    int front;
    int rear;
    int count;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} bounded_buffer_t;

/* FUNCTION PROTOTYPES */
void *producer(void *param);
void *consumer(void *param);
void search_directory(const char *directory, bounded_buffer_t *buffer);
int search_word_in_file(const char *file_path, const char *word);

/* GLOBAL VARIABLES */
bounded_buffer_t bounded_buffer;
int num_threads;
char *search_word;
volatile int done = 0;
int total_found = 0;
int total_files = 0;
pthread_mutex_t total_mutex;

/* MAIN FUNCTION */
int main(int argc, char *argv[]) {
    int buffer_size = 0;
    char *directory = NULL;

    // Parsing command line arguments
    int opt;
    while ((opt = getopt(argc, argv, "b:t:d:w:")) != -1) {
        switch (opt) {
            case 'b':
                buffer_size = atoi(optarg);
                break;
            case 't':
                num_threads = atoi(optarg);
                break;
            case 'd':
                directory = optarg;
                break;
            case 'w':
                search_word = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s\n  -b : bounded bufer size\n  -t : number of threads searching word\n  -d : search directory\n  -w : search word\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (buffer_size <= 0 || num_threads <= 0 || !directory || !search_word) {
        fprintf(stderr, "Usage: %s\n  -b : bounded bufer size\n  -t : number of threads searching word\n  -d : search directory\n  -w : search word\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    printf("Buffer Size = %d, Num Threads = %d, Directory = %s, Search Word = %s\n", buffer_size, num_threads, directory, search_word);

    // Initialize bounded buffer
    bounded_buffer.size = buffer_size;
    bounded_buffer.front = 0;
    bounded_buffer.rear = 0;
    bounded_buffer.count = 0;
    pthread_mutex_init(&bounded_buffer.mutex, NULL);
    pthread_cond_init(&bounded_buffer.not_empty, NULL);
    pthread_cond_init(&bounded_buffer.not_full, NULL);
    pthread_mutex_init(&total_mutex, NULL);

    // Create producer thread
    pthread_t producer_thread;
    pthread_create(&producer_thread, NULL, producer, directory);

    // Create consumer threads
    pthread_t *consumer_threads = malloc(sizeof(pthread_t) * num_threads);
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&consumer_threads[i], NULL, consumer, NULL);
        printf("[Thread %ld] started searching '%s'...\n", (long)consumer_threads[i], search_word);
    }

    // Wait for producer thread to finish
    pthread_join(producer_thread, NULL);
    done = 1;

    // Signal all consumer threads to exit
    pthread_cond_broadcast(&bounded_buffer.not_empty);

    // Wait for all consumer threads to finish
    for (int i = 0; i < num_threads; i++) {
        pthread_join(consumer_threads[i], NULL);
    }

    // Print total results
    printf("Total found = %d (Num files = %d)\n", total_found, total_files);

    // Cleanup
    free(consumer_threads);
    pthread_mutex_destroy(&bounded_buffer.mutex);
    pthread_cond_destroy(&bounded_buffer.not_empty);
    pthread_cond_destroy(&bounded_buffer.not_full);
    pthread_mutex_destroy(&total_mutex);

    return 0;
}

void *producer(void *param) {
    char *directory = (char *)param;
    search_directory(directory, &bounded_buffer);
    return NULL;
}

void search_directory(const char *directory, bounded_buffer_t *buffer) {
    struct dirent *entry;
    DIR *dp = opendir(directory);

    if (dp == NULL) {
        perror("opendir");
        return;
    }

    while ((entry = readdir(dp))) {
        if (entry->d_type == DT_DIR) {
            char path[1024];
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                continue;
            snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
            search_directory(path, buffer);
        } else {
            char *file_path = malloc(1024);
            snprintf(file_path, 1024, "%s/%s", directory, entry->d_name);

            pthread_mutex_lock(&buffer->mutex);
            while (buffer->count == buffer->size) {
                pthread_cond_wait(&buffer->not_full, &buffer->mutex);
            }

            buffer->buffer[buffer->rear] = file_path;
            buffer->rear = (buffer->rear + 1) % buffer->size;
            buffer->count++;
            pthread_cond_signal(&buffer->not_empty);
            pthread_mutex_unlock(&buffer->mutex);
        }
    }

    closedir(dp);
}

void *consumer(void *param) {
    while (1) {
        pthread_mutex_lock(&bounded_buffer.mutex);
        while (bounded_buffer.count == 0 && !done) {
            pthread_cond_wait(&bounded_buffer.not_empty, &bounded_buffer.mutex);
        }

        if (bounded_buffer.count == 0 && done) {
            pthread_mutex_unlock(&bounded_buffer.mutex);
            break;
        }

        char *file_path = bounded_buffer.buffer[bounded_buffer.front];
        bounded_buffer.front = (bounded_buffer.front + 1) % bounded_buffer.size;
        bounded_buffer.count--;

        pthread_cond_signal(&bounded_buffer.not_full);
        pthread_mutex_unlock(&bounded_buffer.mutex);

        int found_count = search_word_in_file(file_path, search_word);

        pthread_mutex_lock(&total_mutex);
        total_found += found_count;
        total_files++;
        pthread_mutex_unlock(&total_mutex);

        printf("[Thread %ld-%d] %s : %d found\n", (long)pthread_self(), total_files, file_path, found_count);

        free(file_path);
    }

    return NULL;
}

int search_word_in_file(const char *file_path, const char *word) {
    FILE *file = fopen(file_path, "r");
    if (!file) {
        perror("fopen");
        return 0;
    }

    int count = 0;
    size_t len = strlen(word);
    char *line = NULL;
    size_t linecap = 0;

    while (getline(&line, &linecap, file) != -1) {
        for (char *p = line; (p = strcasestr(p, word)) != NULL; p++) {
            count++;
        }
    }

    free(line);
    fclose(file);
    return count;
}
