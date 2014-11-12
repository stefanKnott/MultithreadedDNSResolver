#ifndef MULTI_LOOKUP_H
#define MULTI_LOOKUP_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include "util.h"
#include "queue.h"

#define QUEUE_SIZE 50
#define MAX_INPUT_FILES 10
#define MAX_RESOLVER_THREADS 10
#define MIN_RESOLVER_THREADS 2
#define MAX_REQUESTER_THREADS 10
#define MAX_NAME_LENGTH 1025
#define MAX_IP_LENGTH INET6_ADDRSTRLEN
#define INPUTFS "%1024s"

/*
Function inputs urls into the shared queue from each threads respective input file
*/
void *inputUrls(void* file_descriptor);

/*
Function outputs urls from the shared queue and their respective ip addressed into a text file
*/

void* outputUrls();

#endif
