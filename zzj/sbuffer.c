/**
 * \author {AUTHOR}
 */

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include "sbuffer.h"

/**
 * basic node for the buffer, these nodes are linked together to create the buffer
 */


int sbuffer_init(sbuffer_t **buffer) {
    *buffer = malloc(sizeof(sbuffer_t));
    if (*buffer == NULL) return SBUFFER_FAILURE;
    (*buffer)->head = NULL;
    (*buffer)->tail = NULL;
    pthread_mutex_init(&(*buffer)->mutex, NULL);
    sem_init(&(*buffer)->available, 0, 0);
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
    // destroy sync tools
    pthread_mutex_destroy(&(*buffer)->mutex);
    sem_destroy(&(*buffer)->available);

    free(*buffer);
    *buffer = NULL;
    return SBUFFER_SUCCESS;
}

int sbuffer_remove(sbuffer_t *buffer, sensor_data_t *data, int SorD) {
    sbuffer_node_t *dummy;

    if (buffer == NULL) return SBUFFER_FAILURE;
    // Wait for data to be available
    sem_wait(&buffer->available);

    // Lock the buffer for critical section
    pthread_mutex_lock(&buffer->mutex);
    //critical section
    if (buffer->head == NULL) {
        //when buffer is empty
        pthread_mutex_unlock(&buffer->mutex);
        return SBUFFER_NO_DATA;
    }
    pthread_t tid = pthread_self();
    printf("%ld Read by %ld\n", buffer->head, tid);
    *data = buffer->head->data;
    dummy = buffer->head;
    if(SorD==0){
        buffer->head->strg_read = 1;
        //printf("%ld set strg \n",  tid);
    }
    if(SorD==1){
        buffer->head->data_read = 1;
    }
    if(buffer->head->strg_read==1 && buffer->head->data_read==1) {
        //check if both data and strg read the node
        if (buffer->head == buffer->tail) // buffer has only one node
        {
            buffer->head = buffer->tail = NULL;
        } else  // buffer has many nodes empty
        {
            buffer->head = buffer->head->next;
        }
        free(dummy);
    }else{
        sem_post(&buffer->available); //not remove go back
    }
    pthread_mutex_unlock(&buffer->mutex);
    return SBUFFER_SUCCESS;
}

int sbuffer_insert(sbuffer_t *buffer, sensor_data_t *data) {
    sbuffer_node_t *dummy;
    if (buffer == NULL) return SBUFFER_FAILURE;
    dummy = malloc(sizeof(sbuffer_node_t));
    if (dummy == NULL) return SBUFFER_FAILURE;
    dummy->data = *data;
    dummy->next = NULL;
    dummy->data_read = 0;
    dummy->strg_read = 0;
    pthread_mutex_lock(&buffer->mutex);
    //critical section
    pthread_t tid = pthread_self();
    printf("write by %ld\n" ,tid);
    if (buffer->tail == NULL) // buffer empty (buffer->head should also be NULL
    {
        buffer->head = buffer->tail = dummy;
    } else // buffer not empty
    {
        buffer->tail->next = dummy;
        buffer->tail = buffer->tail->next;
    }
    pthread_mutex_unlock(&buffer->mutex);
    sem_post(&buffer->available);//signal reader
    return SBUFFER_SUCCESS;
}