#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "dplist.h"



struct dplist_node {
    dplist_node_t *prev, *next;
    void *element;
};

struct dplist {
    dplist_node_t *head;
    void *(*element_copy)(void *src_element);
    void (*element_free)(void **element);
    int (*element_compare)(void *x, void *y);
};


dplist_t *dpl_create(
        void *(*element_copy)(void *src_element),
        void (*element_free)(void **element),
        int (*element_compare)(void *x, void *y)
) {
    dplist_t *list = (dplist_t *) malloc(sizeof(dplist_t));
    assert(list != NULL);
    list->head = NULL;
    list->element_copy = element_copy;
    list->element_free = element_free;
    list->element_compare = element_compare;
    return list;
}


void dpl_free(dplist_t **list, bool free_element) {
    assert(list != NULL && *list != NULL);
    dplist_node_t *current = (*list)->head;

    while (current != NULL) {
        dplist_node_t *next = current->next;

        if (free_element && (*list)->element_free != NULL) {
            (*list)->element_free(&(current->element));
        }

        free(current);
        current = next;
    }

    free(*list); // 释放链表结构体
    *list = NULL; // 将指针置为 NULL，防止悬挂指针
}


int dpl_size(dplist_t *list) {
    if (list == NULL || list->head == NULL) {
        return -1; // 如果链表不存在或为空，返回 -1
    }

    int count = 0;
    dplist_node_t *current = list->head;

    while (current != NULL) {
        count++;
        current = current->next;
    }

    return count;
}


dplist_t *dpl_insert_at_index(dplist_t *list, void *element, int index, bool insert_copy) {
    assert(list != NULL); // 确保链表存在

    dplist_node_t *new_node = (dplist_node_t *) malloc(sizeof(dplist_node_t));
    assert(new_node != NULL); // 确保内存分配成功

    if (insert_copy && list->element_copy != NULL) {
        new_node->element = list->element_copy(element);
    } else {
        new_node->element = element;
    }

    if (list->head == NULL) { // 插入到空链表
        new_node->prev = NULL;
        new_node->next = NULL;
        list->head = new_node;
        return list;
    }

    if (index <= 0) { // 插入到链表头
        new_node->prev = NULL;
        new_node->next = list->head;
        list->head->prev = new_node;
        list->head = new_node;
        return list;
    }

    // 找到插入位置的参考节点
    dplist_node_t *ref = list->head;
    int count = 0;
    while (ref->next != NULL && count < index) {
        ref = ref->next;
        count++;
    }

    if (ref->next == NULL && count < index) { // 插入到链表尾
        new_node->prev = ref;
        new_node->next = NULL;
        ref->next = new_node;
    } else { // 插入到中间
        new_node->prev = ref->prev;
        new_node->next = ref;
        if (ref->prev != NULL) {
            ref->prev->next = new_node;
        }
        ref->prev = new_node;
    }

    return list;
}


dplist_t *dpl_remove_at_index(dplist_t *list, int index, bool free_element) {
    assert(list != NULL); // 确保链表存在
    if (list->head == NULL) return list; // 空链表，直接返回

    dplist_node_t *current = list->head;

    if (index <= 0) { // 删除第一个节点
        list->head = current->next;
        if (list->head != NULL) list->head->prev = NULL;
    } else {
        int count = 0;
        while (current->next != NULL && count < index) {
            current = current->next;
            count++;
        }

        if (current->next == NULL && count < index) { // 删除最后一个节点
            current->prev->next = NULL;
        } else { // 删除中间节点
            current->prev->next = current->next;
            if (current->next != NULL) current->next->prev = current->prev;
        }
    }

    if (free_element && list->element_free != NULL) {
        list->element_free(&(current->element));
    }

    free(current);
    return list;
}


void *dpl_get_element_at_index(dplist_t *list, int index) {
    if (list == NULL || list->head == NULL) return NULL; // 检查链表是否为空

    dplist_node_t *current = list->head;
    int count = 0;

    while (current->next != NULL && count < index) {
        current = current->next;
        count++;
    }

    return current->element;
}


int dpl_get_index_of_element(dplist_t *list, void *element) {
    if (list == NULL || list->head == NULL || list->element_compare == NULL) return -1;

    int index = 0;
    dplist_node_t *current = list->head;

    while (current != NULL) {
        if (list->element_compare(current->element, element) == 0) {
            return index;
        }
        current = current->next;
        index++;
    }

    return -1;
}


dplist_node_t *dpl_get_reference_at_index(dplist_t *list, int index) {
    if (list == NULL || list->head == NULL) return NULL;

    dplist_node_t *current = list->head;
    int count = 0;

    while (current->next != NULL && count < index) {
        current = current->next;
        count++;
    }

    return current;
}


void *dpl_get_element_at_reference(dplist_t *list, dplist_node_t *reference) {
    if (list == NULL || list->head == NULL || reference == NULL) return NULL;

    dplist_node_t *current = list->head;
    while (current != NULL) {
        if (current == reference) return current->element;
        current = current->next;
    }

    return NULL;
}