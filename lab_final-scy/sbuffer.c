#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "pthread.h"
#include "sbuffer.h"

pthread_mutex_t mutex;
pthread_cond_t pthreadCond;
bool end_of_stream;

// Buffer node structure
typedef struct sbuffer_node {
    struct sbuffer_node *next;
    sensor_data_t data;
} sbuffer_node_t;

// Buffer structure
struct sbuffer {
    sbuffer_node_t *head;
    sbuffer_node_t *tail;
};

// Initialize the buffer
int sbuffer_init(sbuffer_t **buffer) {
    *buffer = malloc(sizeof(sbuffer_t));
    if (*buffer == NULL) return SBUFFER_FAILURE;
    (*buffer)->head = NULL;
    (*buffer)->tail = NULL;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&pthreadCond, NULL);

    end_of_stream = false;

    return SBUFFER_SUCCESS;
}

// Free the buffer memory
int sbuffer_free(sbuffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return SBUFFER_FAILURE;

    pthread_mutex_lock(&mutex);
    while ((*buffer)->head) {
        sbuffer_node_t *dummy = (*buffer)->head;
        (*buffer)->head = (*buffer)->head->next;
        free(dummy);
    }
    free(*buffer);
    *buffer = NULL;
    pthread_mutex_unlock(&mutex);

    return SBUFFER_SUCCESS;
}

// Remove data from the buffer
int sbuffer_remove(sbuffer_t *buffer, sensor_data_t *data) {
    if (buffer == NULL) return SBUFFER_FAILURE;

    pthread_mutex_lock(&mutex);
    while (buffer->head == NULL && !end_of_stream)
        pthread_cond_wait(&pthreadCond, &mutex);

    if (buffer->head == NULL) {
        pthread_mutex_unlock(&mutex);
        return SBUFFER_NO_DATA;
    }

    *data = buffer->head->data;
    sbuffer_node_t *dummy = buffer->head;

    if (buffer->head == buffer->tail) {
        buffer->head = buffer->tail = NULL;
    } else {
        buffer->head = buffer->head->next;
    }

    free(dummy);
    pthread_mutex_unlock(&mutex);

    return SBUFFER_SUCCESS;
}

// Insert data into the buffer
int sbuffer_insert(sbuffer_t *buffer, sensor_data_t *data) {

    if (buffer == NULL) return SBUFFER_FAILURE;

    pthread_mutex_lock(&mutex);

    sbuffer_node_t *dummy = malloc(sizeof(sbuffer_node_t));
    if (dummy == NULL) {
        pthread_mutex_unlock(&mutex);
        return SBUFFER_FAILURE;
    }

    dummy->data = *data;
    dummy->next = NULL;

    if (buffer->tail == NULL) {
        buffer->head = buffer->tail = dummy;
    } else {
        buffer->tail->next = dummy;
        buffer->tail = buffer->tail->next;
    }


    end_of_stream = (data->id == 0) ? true : false;
    pthread_cond_broadcast(&pthreadCond);
    pthread_mutex_unlock(&mutex);

    return SBUFFER_SUCCESS;
}