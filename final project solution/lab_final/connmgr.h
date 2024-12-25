#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <pthread.h>
#include <string.h>

#include <stdbool.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

#include "config.h"
#include "lib/tcpsock.h"
#include "sbuffer.h"

//#define PORT 5678
#define MAX_CONN 3  // state the max. number of connections the server will handle before exiting

int connectionmgr_main(int port, sbuffer_t *buffer, pthread_mutex_t buffer_lock, pthread_cond_t condition);
void * connection_thread(void *client);