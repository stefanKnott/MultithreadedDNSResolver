/*Written by Stefan Knott
 *October 2014
*/

#include "multi-lookup.h"

/* Globals to be shared by all threads */
pthread_cond_t empty, fill;
pthread_mutex_t mutex, write_mutex, cntr_mutex;

queue basket;
char *output_file_name;
int num_urls;
int open_requests = 0;
FILE *output_file = NULL;

/* Producer -- inserts urls into shared queue */
void *inputUrls(void* file_descriptor)
{
	FILE* input_file;
	char** file_name = file_descriptor;
	char url[MAX_NAME_LENGTH];
	char *url_ptr;

	input_file = fopen(*file_name, "r");
	
	if(!input_file)
	{
		pthread_mutex_lock(&cntr_mutex);
		open_requests--;
		pthread_mutex_unlock(&cntr_mutex);
		fprintf(stderr, "error opening input file\n");
		return NULL;
	}

	while(fscanf(input_file, INPUTFS, url) != EOF)
	{
		pthread_mutex_lock(&mutex);	
		while(queue_is_full(&basket))
		{
			pthread_cond_wait(&empty, &mutex);
		}

		if(!queue_is_full(&basket))
		{
			url_ptr = malloc(sizeof(url));
			strcpy(url_ptr, url);	
			if(queue_push(&basket, url_ptr) == QUEUE_FAILURE)
			{
				fprintf(stderr, "queue push failed\n");
			}
			pthread_cond_signal(&fill);					
			url_ptr = NULL;
			free(url_ptr);
		}
		
		pthread_mutex_unlock(&mutex);
	}	
	
	fclose(input_file);	

	pthread_mutex_lock(&cntr_mutex);	
	open_requests--;
	pthread_mutex_unlock(&cntr_mutex);
		
	return NULL;
}

/* Consumer -- pops urls from queue and looks up their given IP addresses using dnslookup() */
void *outputUrls()
{
	int queue_empty = 1;	
	char firstipstr[MAX_IP_LENGTH];
	char *hostname;

	while(!queue_empty || open_requests)
	{
		pthread_mutex_lock(&mutex);
	
		while(queue_is_empty(&basket) && open_requests > 0)
		{
			pthread_cond_wait(&fill, &mutex);
		}

		if(!(queue_empty = queue_is_empty(&basket)))
		{
			hostname = queue_pop(&basket);
			pthread_cond_signal(&empty);
			
			if(hostname == NULL)
			{
				fprintf(stderr, "queue pop failed\n");
				break;
			}
			else
			{
				queue_empty = queue_is_empty(&basket);
			}
		}

		pthread_mutex_unlock(&mutex);	
		
		if(dnslookup(hostname, firstipstr, sizeof(firstipstr)) == UTIL_FAILURE)
		{	
			fprintf(stderr, "dnslookup error\n");
			pthread_mutex_lock(&write_mutex);	
			fprintf(output_file, "%s, DNS ERROR\n", hostname);			
			pthread_mutex_unlock(&write_mutex);
			break;
		}
				
		pthread_mutex_lock(&write_mutex);	
		fprintf(output_file, "%s, %s\n", hostname, firstipstr);			
		pthread_mutex_unlock(&write_mutex);

	}
	return NULL;
}

int main(int argc, char* argv[])
{	

	int num_cores = sysconf(_SC_NPROCESSORS_ONLN); 
	//Set number of resolver threads equal to number of cores on machine
	int num_res_threads = num_cores >= MIN_RESOLVER_THREADS ? num_cores : MIN_RESOLVER_THREADS;
	int num_req_threads;
	output_file = fopen(argv[(argc-1)], "w");

	pthread_t reqThreads[MAX_REQUESTER_THREADS];
	pthread_t resThreads[MAX_RESOLVER_THREADS];	


	/* Init mutexes, cond vars and queue */
	if(pthread_mutex_init(&mutex, NULL) || pthread_mutex_init(&write_mutex, NULL) || pthread_mutex_init(&cntr_mutex, NULL))
	{
		fprintf(stderr, "error initializing mutexes");
	}

	if(pthread_cond_init(&fill, NULL) || pthread_cond_init(&empty, NULL))
	{
		fprintf(stderr, "error initializing condition variables");
	}

	if(queue_init(&basket, QUEUE_SIZE) == QUEUE_FAILURE)
	{
		fprintf(stderr, "error initializing queue");
	}

	int rc, i;

	num_req_threads = argc - 2;
	open_requests = num_req_threads;

	/* Init threads */
	for(i = 0; i < num_req_threads; i++)
	{
		rc = pthread_create(&(reqThreads[i]), NULL, inputUrls, &(argv[i + 1]));
		if(rc)
		{
			printf("ERROR: return code from pthread_create() is %d\n", rc);
			exit(EXIT_FAILURE);
		}
	}

	for(i = 0; i < num_res_threads; i++)
	{
		rc = pthread_create(&(resThreads[i]), NULL, outputUrls, &output_file);
		if(rc)
		{
			printf("ERROR: return code from pthread_create() is %d\n", rc);
			exit(EXIT_FAILURE); 
	        }
	}

	/* Join threads */
	for(i = 0; i < num_req_threads; i++)
	{
		pthread_join(reqThreads[i], NULL);
	}

	for(i = 0; i < num_res_threads; i++)
	{
		pthread_join(resThreads[i], NULL);
	}
	
	/* Dealloc memory given to mutexs, condvars, queue and output file*/
	if(pthread_mutex_destroy(&mutex) || pthread_mutex_destroy(&cntr_mutex) || pthread_mutex_destroy(&write_mutex))
	{
		fprintf(stderr, "error destroying mutexes");
	}

	if(pthread_cond_destroy(&empty) || pthread_cond_destroy(&fill))
	{
		fprintf(stderr, "error destorying condition variables");
	}

	queue_cleanup(&basket);
	fclose(output_file);
	
	return 0;
}
