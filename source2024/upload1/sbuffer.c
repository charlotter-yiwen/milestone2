#include <stdlib.h>
#include <pthread.h>
#include "sbuffer.h"

struct sbuffer_node {
    sensor_data_t data;             // Store sensor data
    struct sbuffer_node *next;      // Pointer to the next node
};

struct sbuffer {
    struct sbuffer_node *head;      // Pointer to the head node
    struct sbuffer_node *tail;      // Pointer to the tail node
    pthread_mutex_t mutex;          // Mutex for thread safety
    pthread_cond_t condvar;         // Condition variable for synchronization
    int end_of_stream;              // Flag to indicate the end of data stream
};

int sbuffer_init(sbuffer_t **buffer) {
    if (buffer == NULL) return SBUFFER_FAILURE;

    *buffer = malloc(sizeof(sbuffer_t));
    if (*buffer == NULL) return SBUFFER_FAILURE;

    (*buffer)->head = NULL;
    (*buffer)->tail = NULL;
    (*buffer)->end_of_stream = 0;

    pthread_mutex_init(&(*buffer)->mutex, NULL);
    pthread_cond_init(&(*buffer)->condvar, NULL);

    return SBUFFER_SUCCESS;
}

int sbuffer_free(sbuffer_t **buffer) {
    if (buffer == NULL || *buffer == NULL) return SBUFFER_FAILURE;

    struct sbuffer_node *current = (*buffer)->head;
    while (current != NULL) {
        struct sbuffer_node *next = current->next;
        free(current);
        current = next;
    }

    pthread_mutex_destroy(&(*buffer)->mutex);
    pthread_cond_destroy(&(*buffer)->condvar);

    free(*buffer);
    *buffer = NULL;

    return SBUFFER_SUCCESS;
}

int sbuffer_insert(sbuffer_t *buffer, sensor_data_t *data) {
    if (buffer == NULL || data == NULL) return SBUFFER_FAILURE;

    struct sbuffer_node *new_node = malloc(sizeof(struct sbuffer_node));
    if (new_node == NULL) return SBUFFER_FAILURE;

    // 动态分配内存并拷贝数据
    new_node->data.id = data->id;
    new_node->data.value = data->value;
    new_node->data.ts = data->ts;

    new_node->next = NULL;

    pthread_mutex_lock(&buffer->mutex);

    if (buffer->tail == NULL) { // 如果缓冲区为空
        buffer->head = buffer->tail = new_node;
    } else { // 添加到尾部
        buffer->tail->next = new_node;
        buffer->tail = new_node;
    }

    pthread_cond_signal(&buffer->condvar); // 通知等待线程
    pthread_mutex_unlock(&buffer->mutex);

    return SBUFFER_SUCCESS;
}

int sbuffer_remove(sbuffer_t *buffer, sensor_data_t *data) {
    if (buffer == NULL || data == NULL) return SBUFFER_FAILURE;

    pthread_mutex_lock(&buffer->mutex);

    while (buffer->head == NULL && !buffer->end_of_stream) {
        pthread_cond_wait(&buffer->condvar, &buffer->mutex); // 等待数据
    }

    if (buffer->head == NULL) { // 缓冲区为空且数据流结束
        pthread_mutex_unlock(&buffer->mutex);
        return SBUFFER_NO_DATA;
    }

    struct sbuffer_node *old_head = buffer->head;
    // 复制节点数据
    data->id = old_head->data.id;
    data->value = old_head->data.value;
    data->ts = old_head->data.ts;

    // 移动头指针
    buffer->head = old_head->next;
    if (buffer->head == NULL) { // 如果缓冲区变空
        buffer->tail = NULL;
    }

    free(old_head); // 释放旧节点
    pthread_mutex_unlock(&buffer->mutex);

    return SBUFFER_SUCCESS;
}



void sbuffer_end_stream(sbuffer_t *buffer) {
    pthread_mutex_lock(&buffer->mutex);
    buffer->end_of_stream = 1;
    pthread_cond_broadcast(&buffer->condvar); // Notify all waiting threads
    pthread_mutex_unlock(&buffer->mutex);


}

//when test,new add
int sbuffer_is_stream_ended(sbuffer_t *buffer) {
    int is_ended;

    pthread_mutex_lock(&buffer->mutex); // 确保线程安全
    is_ended = buffer->end_of_stream;
    pthread_mutex_unlock(&buffer->mutex);

    return is_ended;
}
