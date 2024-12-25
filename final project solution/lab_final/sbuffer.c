#include "sbuffer.h"

//bool done = false; // initially not done with data

int sbuffer_init(sbuffer_t **buffer, pthread_mutex_t buffer_lock) {
    *buffer = malloc(sizeof(sbuffer_t));
    if (*buffer == NULL) return SBUFFER_FAILURE;
    (*buffer)->head = NULL;
    (*buffer)->tail = NULL;

    // mutex init
    if (pthread_mutex_init(&buffer_lock, NULL) != 0){
        printf("\nmutex init failed\n");
    }
    
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

int sbuffer_remove(sbuffer_t *buffer, sensor_data_t *data, pthread_mutex_t buffer_lock) {
    //pthread_mutex_lock(&buffer_lock);
    sbuffer_node_t *dummy;
    if (buffer == NULL){
        return SBUFFER_FAILURE;}

    if (buffer->head == NULL) {
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
    //pthread_mutex_unlock(&buffer_lock);
    return SBUFFER_SUCCESS;
}

int sbuffer_insert(sbuffer_t *buffer, sensor_data_t *data, pthread_mutex_t buffer_lock, pthread_cond_t cond) {
    pthread_mutex_lock(&buffer_lock);
    sbuffer_node_t *dummy;
    if (buffer == NULL) {
        pthread_mutex_unlock(&buffer_lock);
        return SBUFFER_FAILURE;}

    dummy = malloc(sizeof(sbuffer_node_t));

    if (dummy == NULL) {
        pthread_mutex_unlock(&buffer_lock);
        return SBUFFER_FAILURE;}

    dummy->data = *data;
    dummy->next = NULL;
    dummy->read_by_datamgr = 0;
    dummy->read_by_storagemgr = 0;

    if (buffer->tail == NULL) // buffer empty (buffer->head should also be NULL
    {
        buffer->head = buffer->tail = dummy;
    } else // buffer not empty
    {
        buffer->tail->next = dummy;
        buffer->tail = buffer->tail->next;
    }
    pthread_mutex_unlock(&buffer_lock);
    pthread_cond_signal(&cond);
    return SBUFFER_SUCCESS;
}

