#include "mapreduce.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct mapper_config {
    struct map_reduce *mr;
    struct mapper_buffer *mapper;
};

struct reducer_config {
    int outfd;
    struct map_reduce *mr;
};

void *mapper_starter (void *arg) {
    struct mapper_config *config = arg;
    int *ret = malloc(sizeof(int));
    *ret = config->mr->map(config->mr, config->mapper->infd, config->mapper->id, config->mr->nthreads);

    pthread_mutex_lock(&config->mapper->buffer_lock);
    config->mapper->finished = 1;
    pthread_cond_signal(&config->mapper->more);
    pthread_mutex_unlock(&config->mapper->buffer_lock);

    free(config);
    return (void *)ret;
}


void *reducer_starter (void *arg) {
    struct reducer_config *config = arg;
    int *ret = malloc(sizeof(int));
    *ret = config->mr->reduce(config->mr, config->outfd, config->mr->nthreads);

    free(config);
    return (void *)ret;
}


struct mapper_buffer *create_mapper_buffer (int id, int buffer_size) {
    struct mapper_buffer *mb = malloc(sizeof(struct mapper_buffer));

    mb->id = id;
    mb->finished = 0;
    mb->count_entries = 0;

    mb->buf_head = 0;
    mb->buf_tail = 0;
    mb->buffer_size = buffer_size;
    mb->buffer = malloc(buffer_size);
    mb->count_entries = 0;
    mb->available = buffer_size;

    pthread_mutex_init(&mb->buffer_lock, NULL);
    pthread_cond_init(&mb->less, NULL);
    pthread_cond_init(&mb->more, NULL);

    return mb;
}

void mapper_buffer_destroy (struct mapper_buffer *mb) {
    pthread_mutex_destroy(&mb->buffer_lock);
    pthread_cond_destroy(&mb->less);
    pthread_cond_destroy(&mb->more);

    free(mb->buffer);

    free(mb);
}



struct map_reduce *mr_create(map_fn map, reduce_fn reduce, int threads, int buffer_size) {
    struct map_reduce *mr = malloc(sizeof(struct map_reduce));

    mr->map = map;
    mr->reduce = reduce;
    mr->nthreads = threads;
    mr->mappers = malloc(sizeof(struct mapper_buffer) * threads);

    for (int i = 0; i < threads; i++) {
        struct mapper_buffer *mb = create_mapper_buffer(i, buffer_size);
        mr->mappers[i] = mb;
    }

    return mr;
}


void mr_destroy(struct map_reduce *mr) {
    for (int i = 0; i < mr->nthreads; i++) {
        mapper_buffer_destroy(mr->mappers[i]);
    }
    free(mr->mappers);
}


int mr_start(struct map_reduce *mr, const char *inpath, const char *outpath) {

    for (int i = 0; i < mr->nthreads; i++) {
        FILE *infile = fopen(inpath, "r");

        int infd = fileno(infile);

        struct mapper_config *mconfig = malloc(sizeof(struct mapper_config));

        mconfig->mapper = mr->mappers[i];
        mconfig->mapper->infd = infd;
        mconfig->mr = mr;

        pthread_create(&mconfig->mapper->mapper_thread, NULL, mapper_starter, (void *) mconfig);
    }

    FILE *outfile = fopen(outpath, "w");

    int outfd = fileno(outfile);

    struct reducer_config *rconfig = malloc(sizeof(struct reducer_config));
    rconfig->outfd = outfd;
    rconfig->mr = mr;

    pthread_create(&mr->reducer_thread, NULL, reducer_starter, (void *) rconfig);

    return 0;
}


int mr_finish(struct map_reduce *mr) {
    int *mretptr, *rretptr;
    int mret, rret;

    char any_error = 0;
    for (int i = 0; i < mr->nthreads; i++) {
        pthread_join(mr->mappers[i]->mapper_thread, (void *)&mretptr);
        mret = *mretptr;
        any_error = any_error || mret;
    }
    pthread_join(mr->reducer_thread, (void *)&rretptr);

    rret = *rretptr;

    return mret || rret;
}

#define INT_SIZE sizeof(int)

void buffer_write (struct mapper_buffer *mb, void *_data, int bytes) {
    char *data = (char *) _data;

    mb->available -= bytes;
    int space_at_tail = mb->buffer_size - mb->buf_tail;

    int data_off = 0;

    if (space_at_tail < bytes) {
        if (space_at_tail > 0) {
            memcpy(&mb->buffer[mb->buf_tail], &data[data_off], space_at_tail);
        }
        data_off += space_at_tail;
        bytes -= space_at_tail;
        mb->buf_tail = 0;
    }

    memcpy(&mb->buffer[mb->buf_tail], &data[data_off], bytes);

    mb->buf_tail += bytes;
}

void buffer_read (struct mapper_buffer *mb, void *_data, int bytes) {
    char *data = (char *) _data;

    mb->available += bytes;
    int space_at_tail = mb->buffer_size - mb->buf_head;

    int data_off = 0;

    if (space_at_tail < bytes) {
        if (space_at_tail > 0) {
            memcpy(&data[data_off], &mb->buffer[mb->buf_head], space_at_tail);
        }
        data_off += space_at_tail;
        bytes -= space_at_tail;
        mb->buf_head = 0;
    }

    memcpy(&data[data_off], &mb->buffer[mb->buf_head], bytes);

    mb->buf_head += bytes;
}

int mr_produce(struct map_reduce *mr, int id, const struct kvpair *kv) {
    struct mapper_buffer *mb = mr->mappers[id];

    pthread_mutex_lock(&mb->buffer_lock);

    int entry_size = 2 * INT_SIZE + kv->keysz + kv->valuesz;

    if (entry_size > mb->buffer_size) {
        return -1;
    }

    while (mb->available < entry_size) {
        // printf("producer %d waiting\n", id);
        pthread_cond_wait(&mb->less, &mb->buffer_lock);
        // printf("producer %d resuming\n", id);
    }
    // printf("producer %d running\n", id);

    // write date to buffer
    buffer_write(mb, (void *) &kv->keysz, INT_SIZE);
    buffer_write(mb, kv->key, kv->keysz);
    buffer_write(mb, (void *) &kv->valuesz, INT_SIZE);
    buffer_write(mb, kv->value, kv->valuesz);

    mb->count_entries++;

    pthread_cond_signal(&mb->more);

    pthread_mutex_unlock(&mb->buffer_lock);
    // printf("producer %d exiting\n", id);

    return 1;
}


int mr_consume(struct map_reduce *mr, int id, struct kvpair *kv) {

    struct mapper_buffer *mb = mr->mappers[id];

    pthread_mutex_lock(&mb->buffer_lock);

    while (mb->count_entries <= 0 && !mb->finished) {
        // printf("consumer %d waiting\n", id);
        pthread_cond_wait(&mb->more, &mb->buffer_lock);
        // printf("consumer %d resuming\n", id);
    }
    // printf("consumer %d running\n", id);

    if (mb->count_entries == 0) { // then thread must be finished
        pthread_mutex_unlock(&mb->buffer_lock);
        return 0;
    }

    buffer_read(mb, (void *) &kv->keysz, INT_SIZE);
    buffer_read(mb, kv->key, kv->keysz);
    buffer_read(mb, (void *) &kv->valuesz, INT_SIZE);
    buffer_read(mb, kv->value, kv->valuesz);

    mb->count_entries--;

    pthread_cond_signal(&mb->less);

    pthread_mutex_unlock(&mb->buffer_lock);
    // printf("consumer %d exiting\n", id);


    return 1;
}
