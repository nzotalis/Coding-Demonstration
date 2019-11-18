/******************************************************************************
 * Your implementation of the MapReduce framework API.
 *
 * Other than this comment, you are free to modify this file as much as you
 * want, as long as you implement the API specified in the header file.
 *
 * Note: where the specification talks about the "caller", this is the program
 * which is not your code.  If the caller is required to do something, that
 * means your code may assume it has been done.
 ******************************************************************************/

#include "mapreduce.h"
#include "stdlib.h"
#include "stdio.h"
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/resource.h>

//Create timevals for performance evaluation
struct timeval start, end;

struct buffer_struct{
	unsigned long list_size;
	struct node *list_head;
};

struct node{
	uint32_t key_size;
	uint32_t data_size;
	void* value;
	void* key;

	struct node *next;
};

struct map_reduce *mr_create(map_fn map, reduce_fn reduce, int threads, int buffer_size) {
	//Get the start time
	gettimeofday(&start, NULL);
	//Intialize all arrays and variables

	//If there are 0 threads or negative buffer size this is an error.
	if (threads < 1 || buffer_size < 0) return NULL;

	struct map_reduce *mr = malloc(sizeof(struct map_reduce));
		
	mr->map = map;
	mr->reduce = reduce;
	mr->threads = threads;
	mr->buffer_size = (unsigned long) buffer_size;

	mr->locks = (pthread_mutex_t**)malloc((threads + 1) * sizeof(pthread_mutex_t*));
	for (int i = 0; i < threads + 1; i++){
		pthread_mutex_t* lock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(lock, NULL);
		mr->locks[i] = lock;
	}

	mr->thread_list = (pthread_t*)malloc((threads + 1) * sizeof(pthread_t));

	mr->buffers = (struct buffer_struct*)malloc(threads * sizeof(struct buffer_struct));

	for (int i = 0; i < threads; i++){
		struct buffer_struct *s = malloc(sizeof(struct buffer_struct));
		s->list_size = 0;
		s->list_head = NULL;
		mr->buffers[i] = *s;
	}

	mr->finished = (int*)malloc((threads + 1) * sizeof(int));
	for (int i = 0; i < mr->threads + 1; i++){
		mr->finished[i] = 0;
	}

	mr->fds = (int*)malloc((threads + 1) * sizeof(int));

	mr->retvals = (int*)malloc((threads + 1) * sizeof(int));

	return mr;
	
}

void mr_destroy(struct map_reduce *mr) {
	//Free all resources used in mr_create	
	
	for (int i = 0; i < mr->threads + 1; i++){
		free(mr->locks[i]);
		close(mr->fds[i]);
	}

	
	free(mr->buffers);
	free(mr->finished);
	free(mr->thread_list);
	free(mr->locks);
	free(mr->fds);
	free(mr);
	
}

//Struct for holding map/reduce args
struct arg_struct{
	struct map_reduce *mr;
	int infd;
	int outfd;
	int id;
	int nmaps;
	int inout;
};

void* mr_wrapper(void *a){
	//This is the function to initialize a thread

	struct arg_struct *args = (struct arg_struct *) a;
	if (args->inout == 0){
		//0 means start a mapper thread
		int ret;

		//Run mapper for this thread
		ret = (*(args->mr->map))(args->mr, args->infd, args->id, args->nmaps);
		
		//Set finished flag for this thread to 1
		pthread_mutex_lock(args->mr->locks[args->id]);
		args->mr->finished[args->id] = 1;
		//Store the return value of map
		args->mr->retvals[args->id] = ret;
		pthread_mutex_unlock(args->mr->locks[args->id]);
		pthread_exit(NULL);
	}
	else{
		//1 means start a reducer thread
		int ret;

		//Run reducer for this thread
		ret = (*(args->mr->reduce))(args->mr, args->outfd, args->nmaps);

		//Set finished flag for this thread to 1
		pthread_mutex_lock(args->mr->locks[args->id]);
		args->mr->finished[args->id] = 1;
		//Store the return value of reduce
		args->mr->retvals[args->id] = ret;
		pthread_mutex_unlock(args->mr->locks[args->id]);
		pthread_exit(NULL);
	}

	return 0;
}

int mr_start(struct map_reduce *mr, const char *inpath, const char *outpath){
	//Create and store the out file descriptor
	int out = open(outpath, O_RDWR | O_TRUNC | O_CREAT, S_IRUSR | S_IWUSR);
	mr->fds[mr->threads] = out;

	if (out == -1) return -1;

	int in;

	//Create the map threads
	for (int i = 0; i < mr->threads; i++){

		//Create and store the in file descriptor
		in = open(inpath, O_RDONLY, S_IRUSR | S_IWUSR);
		mr->fds[i] = in;
		if (in == -1) return -1;
		
		struct arg_struct *args = malloc(sizeof(struct arg_struct));
		args->mr = mr;
		args->infd = in;
		args->outfd = out;
		args->id = i;
		args->nmaps = mr->threads;
		args->inout = 0;

		//Start a map thread
		if (pthread_create(&mr->thread_list[i], NULL, mr_wrapper, args) != 0) return -1; 
		
	}

	//Creating the reduce thread
	struct arg_struct *args = malloc(sizeof(struct arg_struct));
	args->mr = mr;
	args->infd = in;
	args->outfd = out;
	args->id = mr->threads;
	args->nmaps = mr->threads;
	args->inout = 1;

	//Start a reduce thread
	if (pthread_create(&mr->thread_list[mr->threads], NULL, mr_wrapper, args) != 0) return -1;
	return 0;
}

int mr_finish(struct map_reduce *mr) {
	//Wait for all threads to finish
	for (int i = 0; i < (mr->threads) + 1; i++){
		pthread_join(mr->thread_list[i], NULL);
	}
	//If any thread didn't return 0, return -1 for error.
	for (int i = 0; i < (mr->threads) + 1; i++){
		if (mr->retvals[i] != 0){
			return -1;
		}
	}

	gettimeofday(&end, NULL);
	//printf("Time in microseconds: %ld\n", (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec));
	return 0;
}

int mr_produce(struct map_reduce *mr, int id, const struct kvpair *kv) {
	//If this kv pair couldn't fit into the buffer or the reduce thread has finished, this is an error
	if (kv->keysz + kv->valuesz + sizeof(struct node) > mr->buffer_size || mr->finished[mr->threads] == 1){
		return -1;
	}

	
	//Wait for space to become available in the buffer
	while((unsigned long) (mr->buffers[id].list_size + kv->keysz + kv->valuesz + (unsigned long) sizeof(struct node)) > mr->buffer_size){

	}

	pthread_mutex_lock(mr->locks[id]);

	//Prepare the key and value fields
	void* key = malloc(kv->keysz);
	memcpy(key, kv->key, kv->keysz);

	void* value = malloc(kv->valuesz);
	memcpy(value, kv->value, kv->valuesz);

	//pthread_mutex_unlock(mr->locks[id]);

	//Create a node to add to the buffer
	struct node *spidey = (struct node*)malloc(sizeof(struct node));
	spidey->key_size = kv->keysz;
	spidey->data_size = kv->valuesz;
	spidey->value = value;
	spidey->key = key;

	//pthread_mutex_lock(mr->locks[id]);
	
	//Attach node to the end of buffer
	if (mr->buffers[id].list_head == NULL){
		spidey->next = NULL;
		mr->buffers[id].list_head = spidey;
		mr->buffers[id].list_size += (unsigned long) (spidey->key_size + spidey->data_size + sizeof(struct node));
	}
	else{
		struct node* temp = mr->buffers[id].list_head;
		while(temp->next != NULL){
			temp = temp->next;
		}
		temp->next = spidey;
		spidey->next = NULL;
		mr->buffers[id].list_size += (unsigned long) (spidey->key_size + spidey->data_size + sizeof(struct node));
	}

	pthread_mutex_unlock(mr->locks[id]);
	
	return 1;
}

int mr_consume(struct map_reduce *mr, int id, struct kvpair *kv) {
	//If the map thread is done and the buffer is empty, there is nothing to do so return 0
	if (mr->finished[id] == 1 && mr->buffers[id].list_head == NULL){
		return 0;
	}
	
	while(mr->buffers[id].list_head == NULL){
		//pthread_mutex_lock(mr->locks[id]);
		//If while waiting the map thread finishes, return 0.
		if (mr->finished[id] == 1){
			return 0;
		}
		//pthread_mutex_unlock(mr->locks[id]);
	}
	
	pthread_mutex_lock(mr->locks[id]);

	//Read from the buffer into the kv struct
	kv->keysz = mr->buffers[id].list_head->key_size;
	kv->valuesz = mr->buffers[id].list_head->data_size;

	kv->key = mr->buffers[id].list_head->key;
	kv->value = mr->buffers[id].list_head->value;

	//Pop the head off the list.
	if (mr->buffers[id].list_head->next == NULL){
		mr->buffers[id].list_size -= (unsigned long) (mr->buffers[id].list_head->key_size + mr->buffers[id].list_head->data_size + (unsigned long) sizeof(struct node));
		if (mr->buffers[id].list_size > 10 * mr->buffer_size){
			mr->buffers[id].list_size = 0;
		} 
		free(mr->buffers[id].list_head);
		mr->buffers[id].list_head = NULL;
	}
	else{
		struct node *temp = mr->buffers[id].list_head;

		mr->buffers[id].list_head = temp->next;

		mr->buffers[id].list_size -= (unsigned long) (mr->buffers[id].list_head->key_size + mr->buffers[id].list_head->data_size + (unsigned long) sizeof(struct node));	

		if (mr->buffers[id].list_size > 10 * mr->buffer_size){
			mr->buffers[id].list_size = 0;
		} 
	
		free(temp);
	}

	pthread_mutex_unlock(mr->locks[id]);

	return 1;
}
