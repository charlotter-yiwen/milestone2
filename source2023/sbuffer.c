/**
 * \author {AUTHOR}
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "pthread.h"
#include "sbuffer.h"

pthread_mutex_t mutex;
pthread_cond_t condvar;
bool end_of_stream;

/**
 * basic node for the buffer, these nodes are linked together to create the buffer
 */
typedef struct sbuffer_node {
    struct sbuffer_node *next;  /**< a pointer to the next node*/
    sensor_data_t data;         /**< a structure containing the data */
} sbuffer_node_t;

/**
 * a structure to keep track of the buffer
 */
struct sbuffer {
    sbuffer_node_t *head;       /**< a pointer to the first node in the buffer */
    sbuffer_node_t *tail;       /**< a pointer to the last node in the buffer */
};

int sbuffer_init(sbuffer_t **buffer) {
    *buffer = malloc(sizeof(sbuffer_t));
    if (*buffer == NULL) return SBUFFER_FAILURE;
    (*buffer)->head = NULL;
    (*buffer)->tail = NULL;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&condvar, NULL);

    end_of_stream = false;

    return SBUFFER_SUCCESS;
}

int sbuffer_free(sbuffer_t **buffer) {
    sbuffer_node_t *dummy;
    if ((buffer == NULL) || (*buffer == NULL)) {
        return SBUFFER_FAILURE;
    }
    while ((*buffer)->head) {
        dummy = (*buffer)->head;
        (*buffer)->head = (*buffer)->head->next;
        free(dummy);
    }
    free(*buffer);
    *buffer = NULL;
    return SBUFFER_SUCCESS;
}

int sbuffer_remove(sbuffer_t *buffer, sensor_data_t *data) {
    sbuffer_node_t *dummy;
    if (buffer == NULL) return SBUFFER_FAILURE;
    pthread_mutex_lock(&mutex);
    while(buffer->head == NULL && !end_of_stream)
        pthread_cond_wait(&condvar,&mutex);
    if(buffer->head == NULL) {
        pthread_mutex_unlock(&mutex);
        return SBUFFER_NO_DATA;
    }
    *data = buffer->head->data;
    dummy = buffer->head;
    if (buffer->head == buffer->tail) // buffer has only one node
    {
        buffer->head = buffer->tail = NULL;
    } else  // buffer has many nodes empty
    {
        buffer->head = buffer->head->next;
    }
    free(dummy);
    pthread_mutex_unlock(&mutex);
    return SBUFFER_SUCCESS;
}

int sbuffer_insert(sbuffer_t *buffer, sensor_data_t *data) {
    sbuffer_node_t *dummy;
    if (buffer == NULL) return SBUFFER_FAILURE;
    dummy = malloc(sizeof(sbuffer_node_t));
    if (dummy == NULL) return SBUFFER_FAILURE;
    dummy->data = *data;
    dummy->next = NULL;
    pthread_mutex_lock(&mutex);
    if (buffer->tail == NULL) // buffer empty (buffer->head should also be NULL
    {
        buffer->head = buffer->tail = dummy;
    } else // buffer not empty
    {
        buffer->tail->next = dummy;
        buffer->tail = buffer->tail->next;
    }
//    printf("insert data: sensor id = %hu - temperature = %g - timestamp = %ld\n", dummy->data.id, dummy->data.value, (long int) dummy->data.ts);
    if(data->id == 0){
        end_of_stream = true;
    }
    else{
        end_of_stream = false;
    }
    pthread_cond_broadcast(&condvar);
    pthread_mutex_unlock(&mutex);
    return SBUFFER_SUCCESS;
}