/**
 * \author {AUTHOR}
 */

#include <stdlib.h>
#include <stdio.h>
#include "sbuffer.h"

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
    pthread_mutex_t mutex;      /**< mutex for thread-safe access */
    pthread_cond_t cond;        /**< condition variable for synchronization */
    int is_closed;              /**< flag indicating if the buffer is closed */
};

int sbuffer_init(sbuffer_t **buffer) {
    *buffer = malloc(sizeof(sbuffer_t));
    if (*buffer == NULL) return SBUFFER_FAILURE;
    (*buffer)->head = NULL;
    (*buffer)->tail = NULL;
    (*buffer)->is_closed = 0;//add
    pthread_mutex_init(&(*buffer)->mutex, NULL);//mutex
    pthread_cond_init(&(*buffer)->cond, NULL);//condition v
    return SBUFFER_SUCCESS;
}


int sbuffer_free(sbuffer_t **buffer) {
    sbuffer_node_t *current = (*buffer)->head;
    while (current != NULL) {
        sbuffer_node_t *next = current->next;
        free(current);
        current = next;
    }
    pthread_mutex_destroy(&(*buffer)->mutex);//mutex free
    pthread_cond_destroy(&(*buffer)->cond);//condition free
    free(*buffer);
    *buffer = NULL;
    return SBUFFER_SUCCESS;
}


int sbuffer_remove(sbuffer_t *buffer, sensor_data_t *data) {
    pthread_mutex_lock(&buffer->mutex); // lock
    while (buffer->head == NULL) {
        if (buffer->is_closed) { // 如果缓冲区已关闭
            pthread_mutex_unlock(&buffer->mutex);
            return SBUFFER_NO_DATA;
        }
        pthread_cond_wait(&buffer->cond, &buffer->mutex); // 等待新数据
    }

    sbuffer_node_t *dummy = buffer->head;
    *data = dummy->data;

    if (buffer->head == buffer->tail) { // buffer has only one node
        buffer->head = buffer->tail = NULL;
    } else { // buffer has many nodes empty
        buffer->head = buffer->head->next;
    }
    free(dummy);
    pthread_mutex_unlock(&buffer->mutex); // unlock

    return SBUFFER_SUCCESS;
}


int sbuffer_insert(sbuffer_t *buffer, sensor_data_t *data) {
    sbuffer_node_t *dummy = malloc(sizeof(sbuffer_node_t));
    if (dummy == NULL) return SBUFFER_FAILURE;
    dummy->data = *data;
    dummy->next = NULL;

    pthread_mutex_lock(&buffer->mutex); // lock

    if (buffer->tail == NULL) { // empty buffer
        buffer->head = buffer->tail = dummy;
    } else { // buffer not empty
        buffer->tail->next = dummy;
        buffer->tail = dummy;
    }

    pthread_cond_signal(&buffer->cond); // condition:signal
    pthread_mutex_unlock(&buffer->mutex); // unlock

    return SBUFFER_SUCCESS;
}

int sbuffer_close(sbuffer_t *buffer) {
    pthread_mutex_lock(&buffer->mutex);//lock
    buffer->is_closed = 1;
    pthread_cond_broadcast(&buffer->cond); // broadcast all threads
    pthread_mutex_unlock(&buffer->mutex);//unlock
    return SBUFFER_SUCCESS;
}
