

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>
#include "dplist.h"




/*
 * The real definition of struct list / struct node
 */

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


dplist_t *dpl_create(// callback functions
        void *(*element_copy)(void *src_element),
        void (*element_free)(void **element),
        int (*element_compare)(void *x, void *y)
) {
    dplist_t *list;
    list = malloc(sizeof(struct dplist));
    list->head = NULL;
    list->element_copy = element_copy;
    list->element_free = element_free;
    list->element_compare = element_compare;
    return list;
}

void dpl_free(dplist_t **list, bool free_element) {

    //TODO: add your code here
    int size = dpl_size(*list);
    if(size>=0){
        //get the last node
        dplist_node_t *node_pointer = dpl_get_reference_at_index(*list, size);
        for(int i=0;i<size;i++){
            if(node_pointer->prev!=NULL){ //if this is not the first node
                node_pointer = node_pointer->prev;
                if(free_element){
                    (*list)->element_free(&(node_pointer->next->element));
                }
                free(node_pointer->next);
            }
            else{   //if this is the first node
                if(free_element){
                    (*list)->element_free(&((*list)->head->element));
                }
                free((*list)->head);
            }
        }

        free(*list);
    }

    *list = NULL;
}

dplist_t *dpl_insert_at_index(dplist_t *list, void *element, int index, bool insert_copy) {

    //TODO: add your code here
    dplist_node_t *ref_at_index, *list_node;
    if (list == NULL) return NULL;

    list_node = malloc(sizeof(dplist_node_t));

    if(insert_copy){
        list_node->element = list->element_copy(element);
    }
    else{
        list_node->element = element;
    }
    // pointer drawing breakpoint
    if (list->head == NULL) { // covers case 1
        list_node->prev = NULL;
        list_node->next = NULL;
        list->head = list_node;
        // pointer drawing breakpoint
    } else if (index <= 0) { // covers case 2
        list_node->prev = NULL;
        list_node->next = list->head;
        list->head->prev = list_node;
        list->head = list_node;
        // pointer drawing breakpoint
    } else {
        ref_at_index = dpl_get_reference_at_index(list, index);
        assert(ref_at_index != NULL);
        // pointer drawing breakpoint
        if (index < dpl_size(list)) { // covers case 4
            list_node->prev = ref_at_index->prev;
            list_node->next = ref_at_index;
            ref_at_index->prev->next = list_node;
            ref_at_index->prev = list_node;
            // pointer drawing breakpoint
        } else { // covers case 3
            assert(ref_at_index->next == NULL);
            list_node->next = NULL;
            list_node->prev = ref_at_index;
            ref_at_index->next = list_node;
            // pointer drawing breakpoint
        }
    }

    return list;
}

dplist_t *dpl_remove_at_index(dplist_t *list, int index, bool free_element) {

    //TODO: add your code here
    dplist_node_t *node_pointer = dpl_get_reference_at_index(list, index);
    if(node_pointer == NULL){

    }
    else {
        if (index <= 0) {  //if this is the first node
            list->head = node_pointer->next;
            if (node_pointer->next != NULL) {
                node_pointer->next->prev = NULL;
            }
            else{
                list->head = NULL;
            }
        } else if (index >= dpl_size(list)) {
            if(node_pointer->prev!=NULL){
                node_pointer->prev->next = NULL;
            }
            else{
                list->head = NULL;
            }
        } else {
            if (node_pointer->next != NULL) {
                node_pointer->next->prev = node_pointer->prev;
            }
            else{
                list->head = NULL;
            }
            if(node_pointer->prev!=NULL){
                node_pointer->prev->next = node_pointer->next;
            }
            else{
                list->head = NULL;
            }
        }

        if(free_element){
            list->element_free(&(node_pointer->element));
        }
        free(node_pointer);
    }

    return list;
}

int dpl_size(dplist_t *list) {

    //TODO: add your code here
    if(list == NULL){
        return -1;
    }
    else{
        dplist_node_t * node_pointer = list->head;
        if(node_pointer == NULL){
            return 0;
        }
        else{
            int size = 0;
            while(node_pointer != NULL){
                size++;
                node_pointer = node_pointer->next;
            }
            printf("\n");
            return size;
        }
    }
}

void *dpl_get_element_at_index(dplist_t *list, int index) {

    //TODO: add your code here
    void* element = NULL;
    dplist_node_t *node_pointer = dpl_get_reference_at_index(list, index);
    if(node_pointer==NULL){
        element = NULL;
    }
    else{
        element = node_pointer->element;
    }

    return element;
}

int dpl_get_index_of_element(dplist_t *list, void *element) {

    //TODO: add your code here
    int index = -1;
    dplist_node_t *node_pointer = list->head;
    if(list == NULL){

    }
    else if(list->head == NULL){

    }
    else{
        for(int i=0;i<dpl_size(list);i++){
            if(list->element_compare(node_pointer->element, element)){
                index = i;
            }
            node_pointer = node_pointer->next;
        }
    }

    return index;
}

dplist_node_t *dpl_get_reference_at_index(dplist_t *list, int index) {

    //TODO: add your code here
    dplist_node_t *dummy = NULL;

    int size = dpl_size(list);
    if(list == NULL){

    }
    else if(list->head == NULL){

    }
    else if(index <= 0){
        dummy = list->head;
    }
    else if(index >= size){
        dummy = list->head;
        for(int i=1;i<size;i++){
            dummy = dummy->next;
        }
    }
    else{
        dummy = list->head;
        for(int i=0;i<index;i++){
            dummy = dummy->next;
        }
    }

    return dummy;
}

void *dpl_get_element_at_reference(dplist_t *list, dplist_node_t *reference) {

    //TODO: add your code here
    void* element = NULL;
    if(list == NULL){

    }
    else if(reference == NULL){

    }
    else if(list->head == NULL){

    }
    else{
        element = reference->element;
    }
    return element;
}



