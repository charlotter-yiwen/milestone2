#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#include "dplist.h"




/*
 * The real definition of struct list / struct node
 */



dplist_t *dpl_create(// callback functions
        void *(*element_copy)(void *src_element),
        void (*element_free)(void **element),
        int (*element_compare)(void *x, void *y)
) {
    dplist_t *list;
    list = (dplist_t*)malloc(sizeof(struct dplist));
    list->head = NULL;
    list->element_copy = element_copy;
    list->element_free = element_free;
    list->element_compare = element_compare;
    return list;
}


void dpl_free(dplist_t **list, bool free_element) {

    //TODO: add your code here
    if(*list==NULL)
        return;
    while((*list)->head!=NULL){
        dpl_remove_at_index(*list,0,free_element);
    }
    free(*list);
    *list = NULL;
    //Do extensive testing with valgrind.

}

dplist_t *dpl_insert_at_index(dplist_t *list, void *element, int index, bool insert_copy) {

    //TODO: add your code here
    dplist_node_t *ref_at_index, *list_node; // ref_at_index is the latter neighbor of inserted node
    if (list == NULL) return NULL;

    list_node = (dplist_node_t*)malloc(sizeof(dplist_node_t)); // allocate mem for new element
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
        list->head = list_node;  // when list is empty, the first inserted element is the head
        // pointer drawing breakpoint
    } else if (index <= 0) { // covers case 2: inserted element becomes head
        list_node->prev = NULL;
        list_node->next = list->head;
        list->head->prev = list_node;
        list->head = list_node;
        // pointer drawing breakpoint
    } else {
        ref_at_index = dpl_get_reference_at_index(list, index);
        assert(ref_at_index != NULL);
        // pointer drawing breakpoint, if false, stop
        if (index < dpl_size(list)) { // covers case 4
            list_node->prev = ref_at_index->prev;
            list_node->next = ref_at_index;
            ref_at_index->prev->next = list_node; //former node's next node is inserted node
            ref_at_index->prev = list_node;
            // pointer drawing breakpoint
        } else { // covers case 3
            assert(ref_at_index->next == NULL); //make sure it is the end
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
    dplist_node_t *ref_at_index; // ref_at_index is the latter neighbor of inserted node
    if (list == NULL) return NULL;
    if (list->head == NULL) return list;

    ref_at_index = dpl_get_reference_at_index(list,index);
    if(ref_at_index->prev == NULL){
        if(ref_at_index->next == NULL){
            list->head = NULL;
            if(free_element){
                list->element_free(&(ref_at_index->element));
            }
            return list;
        }
        ref_at_index->next->prev=NULL; // set new head
        list->head = ref_at_index->next;
    }else if(ref_at_index->next == NULL){
        ref_at_index->prev->next = NULL; // set new end
    }else{
        ref_at_index->prev->next = ref_at_index->next;
        ref_at_index->next->prev = ref_at_index->prev;
    }
    if(free_element){
        list->element_free(&(ref_at_index->element));
    }
    free(ref_at_index);
    return list;
//    if (list == NULL || list->head == NULL ) {
//        return list;
//    }
//    int size = dpl_size(list);
//    dplist_node_t *current = list->head;
//    if (index <= 0) {
//        // Remove the first node
//        list->head = current->next;
//        if (current->next != NULL) {
//            current->next->prev = NULL;
//        }
//    } else {
//        // Remove a middle or last node
//        for (int i = 0; i < index && i<size; i++) {
//            current = current->next;
//        }
//        current->prev->next = current->next;
//        if (current->next != NULL) {
//            current->next->prev = current->prev;
//        }
//    }
//    if (free_element ) {
//        list->element_free(&(current->element));
//    }
//    free(current);
//    return list;

}


int dpl_size(dplist_t *list) {

    //TODO: add your code here
    if(list == NULL)
        return -1;
    if (list->head == NULL)
        return 0;
    dplist_node_t* location = list->head;
    int count = 0;
    while(location != NULL){
        count++;
        location = location->next;
    }
    return count;
}


void *dpl_get_element_at_index(dplist_t *list, int index) {

    //TODO: add your code here
    dplist_node_t* dummy = dpl_get_reference_at_index(list, index);
    void* data = NULL;
    if(dummy != NULL)   //invalid situation
        data = dummy->element;
    return data;

}

/** Returns an index to the first list node in the list containing 'element'.
 * - the first list node has index 0.
 * - Use 'element_compare()' to search 'element' in the list, a match is found when 'element_compare()' returns 0.
 * - If 'element' is not found in the list, -1 is returned.
 * - If 'list' is NULL, NULL is returned.
 * \param list a pointer to the list
 * \param element the element to look for
 * \return the index of the element that matches 'element'
 */
int dpl_get_index_of_element(dplist_t *list, void *element) {

    //TODO: add your code here
    if(list == NULL) return -1;
    dplist_node_t * current = list->head;

    for(int i=0;current != NULL;i++){
        if(list->element_compare(element,current->element)==0){
            return i;
        }
        current=current->next;
    }
    return -1;
}

dplist_node_t *dpl_get_reference_at_index(dplist_t *list, int index) {

    //TODO: add your code here
    //int count = 0 ;
    dplist_node_t *dummy = NULL;


    if(list == NULL) return NULL;  //list is not initialized
    if (list->head== NULL) return NULL;  //list is empty
    else if(index <= 0){
        dummy = list->head;
    }
    else{
        dummy = list->head;
        if (index < dpl_size(list)) {
            for (int i = 0; i < index; i++) {
                dummy = dummy->next;
            }
        }
        else{
            while(dummy->next!=NULL){
                dummy = dummy->next;
            }
        }

    }

    return dummy;
}

/** Returns the element contained in the list node with reference 'reference' in the list.
 * - If the list is empty, NULL is returned.
 * - If 'list' is is NULL, NULL is returned.
 * - If 'reference' is NULL, NULL is returned.
 * - If 'reference' is not an existing reference in the list, NULL is returned.
 * \param list a pointer to the list
 * \param reference a pointer to a certain node in the list
 * \return the element contained in the list node or NULL
 */
void *dpl_get_element_at_reference(dplist_t *list, dplist_node_t *reference) {

    //TODO: add your code here
    if(list==NULL || list->head==NULL || reference==NULL)
        return NULL;
    dplist_node_t *current = list->head;
    while(current!= NULL){
        if(current == reference)
            return current->element;
        current = current->next;
    }
    return NULL;
}

