

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include "dplist.h"



/*

 * The real definition of struct list / struct node*/


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
    if (*list == NULL) {
        return;
    }

    dplist_node_t *current = (*list)->head;
    while (current != NULL) {
        dplist_node_t *temp = current;
        current = current->next;
        if (free_element) {
            (*list)->element_free(&(temp->element));
        }
        free(temp);
    }

    free(*list);
    *list = NULL;
}

dplist_t *dpl_insert_at_index(dplist_t *list, void *element, int index, bool insert_copy) {

    //TODO: add your code here
    dplist_node_t *ref_at_index, *node;
    if (list == NULL) return NULL;

    node = malloc(sizeof(dplist_node_t));

    if(insert_copy){
        node->element = list->element_copy(element);
    }
    else{
        node->element = element;
    }

    if (list->head == NULL) {
        node->prev = NULL;
        node->next = NULL;
        list->head = node;
    } else if (index <= 0) {
        node->prev = NULL;
        node->next = list->head;
        list->head->prev = node;
        list->head = node;
    } else {
        ref_at_index = dpl_get_reference_at_index(list, index);
        if (index < dpl_size(list)) {
            node->prev = ref_at_index->prev;
            node->next = ref_at_index;
            ref_at_index->prev->next = node;
            ref_at_index->prev = node;
        } else {
            node->next = NULL;
            node->prev = ref_at_index;
            ref_at_index->next = node;
        }
    }

    return list;
}

dplist_t *dpl_remove_at_index(dplist_t *list, int index, bool free_element) {

    //TODO: add your code here
    dplist_node_t *node = dpl_get_reference_at_index(list, index);
    if(node == NULL){

    }
    else {
        if (index <= 0) {
            list->head = node->next;
            if (node->next != NULL) {
                node->next->prev = NULL;
            }
            else{
                list->head = NULL;
            }
        } else if (index >= dpl_size(list)) {
            if(node->prev != NULL){
                node->prev->next = NULL;
            }
            else{
                list->head = NULL;
            }
        } else {
            if (node->next != NULL) {
                node->next->prev = node->prev;
            }
            else{
                list->head = NULL;
            }
            if(node->prev != NULL){
                node->prev->next = node->next;
            }
            else{
                list->head = NULL;
            }
        }

        if(free_element){
            list->element_free(&(node->element));
        }
        free(node);
    }

    return list;
}
int dpl_size(dplist_t *list) {
/** Returns the number of elements in the list.
 * - If 'list' is is NULL, -1 is returned.
 * \param list a pointer to the list
 * \return the size of the list*/

    //TODO: add your code here
    int conclusion;
    conclusion = 0;
    if(list == NULL){
        conclusion = -1;
    }else{
        dplist_node_t *node = list-> head;
        if(node == NULL){
            conclusion = 0;
        }else{
            while(node!=NULL){
                conclusion ++;
                node = node->next;
            }
        }
    }
    return conclusion;

}

void *dpl_get_element_at_index(dplist_t *list, int index) {
   //TODO: add your code here
   int size = dpl_size(list);
   dplist_node_t *node = (dplist_node_t *) (dplist_t *) dpl_get_reference_at_index(list, index);
   void *ele;
   ele = 0;

   if(index <= 0){
       node = list->head;
       ele = node ->element;
   } else if(index >=size){
       node = dpl_get_reference_at_index(list,size);
       ele = node->element;
   } else{
       node = dpl_get_reference_at_index(list,index);
       ele = node->element;
   }
   return ele;
 }

int dpl_get_index_of_element(dplist_t *list, void *element) {

    //TODO: add your code here
    int indeX;
    indeX = -1;
    dplist_node_t *node = list->head;
    int size = dpl_size(list);
    if(list == NULL){}
    else if(list->head == NULL){}
    else{
        for(int i=0;i<size;i++){
            if(list->element_compare(node->element, element)){
                indeX = i;
            }else{
                node = node->next;
            }
        }
    }

    return indeX;

}

dplist_node_t *dpl_get_reference_at_index(dplist_t *list, int index) {

    //TODO: add your code here
    dplist_node_t *NODE = NULL;

    int size = dpl_size(list);
    if(list == NULL){

    }
    else if(list->head == NULL){

    }
    else if(index <= 0){
        NODE = list->head;
    }
    else if(index >= size){
        NODE = list->head;
        for(int i=1;i<size;i++){
            NODE = NODE->next;
        }
    }
    else{
        NODE = list->head;
        for(int i=0;i<index;i++){
            NODE = NODE->next;
        }
    }

    return NODE;
}

void *dpl_get_element_at_reference(dplist_t *list, dplist_node_t *reference) {
    //TODO: add your code here
    void *ele = NULL;

    if(list == NULL){

    } else if(reference == NULL){

    } else if(list->head == NULL){

    } else{
        ele = reference->element;
    }

    return ele;
}
