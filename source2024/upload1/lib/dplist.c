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

// 创建一个新的链表
dplist_t *dpl_create(
    void *(*element_copy)(void *src_element),
    void (*element_free)(void **element),
    int (*element_compare)(void *x, void *y)
) {
    dplist_t *list = malloc(sizeof(dplist_t));
    list->head = NULL;
    list->element_copy = element_copy;
    list->element_free = element_free;
    list->element_compare = element_compare;
    return list;
}

// 释放链表中的所有元素
void dpl_free(dplist_t **list, bool free_element) {
    if (list == NULL || *list == NULL) return;

    dplist_node_t *current = (*list)->head;
    while (current != NULL) {
        dplist_node_t *next = current->next;
        if (free_element && (*list)->element_free != NULL) {
            (*list)->element_free(&(current->element));
        }
        free(current);
        current = next;
    }
    free(*list);
    *list = NULL;
}

// 返回链表的大小
int dpl_size(dplist_t *list) {
    if (list == NULL) return -1;

    int count = 0;
    dplist_node_t *current = list->head;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}


/*
 * Inserts a new element at the specified index in the double-linked list.
 * If insert_copy is true, the element_copy callback is used to create a copy of the element.
 */
dplist_t *dpl_insert_at_index(dplist_t *list, void *element, int index, bool insert_copy) {
    if (list == NULL) return NULL;

    dplist_node_t *new_node = malloc(sizeof(dplist_node_t));
    if (new_node == NULL) return NULL;
    new_node->element = insert_copy && list->element_copy ? list->element_copy(element) : element;

    if (list->head == NULL) { // Insert into an empty list
        new_node->prev = NULL;
        new_node->next = NULL;
        list->head = new_node;
    } else if (index <= 0) { // Insert at the beginning
        new_node->prev = NULL;
        new_node->next = list->head;
        list->head->prev = new_node;
        list->head = new_node;
    } else {
        dplist_node_t *ref = dpl_get_reference_at_index(list, index);
        if (ref == NULL || index >= dpl_size(list)) { // Insert at the end
            dplist_node_t *last = list->head;
            while (last->next != NULL) last = last->next;
            new_node->next = NULL;
            new_node->prev = last;
            last->next = new_node;
        } else { // Insert in the middle
            new_node->prev = ref->prev;
            new_node->next = ref;
            if (ref->prev != NULL) ref->prev->next = new_node;
            ref->prev = new_node;
        }
    }
    return list;
}

// 删除链表中指定索引的节点
dplist_t *dpl_remove_at_index(dplist_t *list, int index, bool free_element) {
    if (list == NULL || list->head == NULL) return list;

    dplist_node_t *current = dpl_get_reference_at_index(list, index);
    if (current == NULL) return list;

    if (current->prev != NULL) current->prev->next = current->next;
    if (current->next != NULL) current->next->prev = current->prev;
    if (current == list->head) list->head = current->next;

    if (free_element && list->element_free != NULL) {
        list->element_free(&(current->element));
    }
    free(current);
    return list;
}


/*
*/
dplist_node_t *dpl_get_reference_at_index(dplist_t *list, int index) {
    if (list == NULL || list->head == NULL) return NULL;

    dplist_node_t *current = list->head;
    int count = 0;

    // 处理负索引或索引为 0 的情况，直接返回第一个节点
    if (index <= 0) {
        return current;
    }

    // 遍历链表，找到对应索引的节点
    while (current->next != NULL && count < index) {
        current = current->next;
        count++;
    }

    // 如果索引超出范围，则返回最后一个节点
    return current;
}


/*
 * Returns the element at the specified index in the double-linked list.
 */
void *dpl_get_element_at_index(dplist_t *list, int index) {
    dplist_node_t *ref = dpl_get_reference_at_index(list, index);
    if (ref != NULL) return ref->element;
    return NULL;
}


// 获取链表中与指定元素匹配的第一个节点的索引
int dpl_get_index_of_element(dplist_t *list, void *element) {
    if (list == NULL) return -1;

    int index = 0;
    dplist_node_t *current = list->head;
    while (current != NULL) {
        if (list->element_compare != NULL && list->element_compare(current->element, element) == 0) {
            return index;
        }
        current = current->next;
        index++;
    }
    return -1;
}

void *dpl_get_element_at_reference(dplist_t *list, dplist_node_t *reference) {
    if (list == NULL || list->head == NULL || reference == NULL) {
        return NULL; // 空列表或空引用返回 NULL
    }

    dplist_node_t *current = list->head;
    while (current != NULL) {
        if (current == reference) {
            return current->element; // 找到匹配的引用，返回元素
        }
        current = current->next;
    }

    return NULL; // 没有找到匹配的引用，返回 NULL
}



