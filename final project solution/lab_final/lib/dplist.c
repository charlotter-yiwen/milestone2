
#include "dplist.h"

/*
 * definition of error codes
 * */
#define DPLIST_NO_ERROR 0
#define DPLIST_MEMORY_ERROR 1 // error due to mem alloc failure
#define DPLIST_INVALID_ERROR 2 //error due to a list operation applied on a NULL list 

#ifdef DEBUG
#define DEBUG_PRINTF(...) 									                                        \
        do {											                                            \
            fprintf(stderr,"\nIn %s - function %s at line %d: ", __FILE__, __func__, __LINE__);	    \
            fprintf(stderr,__VA_ARGS__);								                            \
            fflush(stderr);                                                                         \
                } while(0)
#else
#define DEBUG_PRINTF(...) (void)0
#endif


#define DPLIST_ERR_HANDLER(condition, err_code)                         \
    do {                                                                \
            if ((condition)) DEBUG_PRINTF(#condition " failed\n");      \
            assert(!(condition));                                       \
        } while(0)


/*
 * The real definition of struct list / struct node
 */
struct dplist {
    dplist_node_t *head;
    void *(*element_copy)(void *src_element);
    void (*element_free)(void **element);
    int (*element_compare)(void *x, void *y);
};

typedef struct{
    uint16_t sensor_id;
    uint16_t room_id;
    time_t last_modified;
    double temperatures[RUN_AVG_LENGTH];
    double temp_avg;

} my_element_t;


struct dplist_node {
    dplist_node_t *prev, *next;
    my_element_t *element;
};

void * element_copy(void * element) {
    my_element_t* copy = malloc(sizeof (my_element_t));
    //char* new_name;
    //asprintf(&new_name,"%s",((my_element_t*)element)->name);
    assert(copy != NULL);
    copy->sensor_id = ((my_element_t*)element)->sensor_id;
    copy->room_id = ((my_element_t*)element)->room_id;
    copy->temp_avg = ((my_element_t*)element)->temp_avg;
    copy->last_modified = ((my_element_t*)element)->last_modified;
    return (void *) copy;
}
void element_free(void ** element) {
    //free((((my_element_t*)*element))->sensor_id);
    //free((((my_element_t*)*element))->room_id);
    //free((((my_element_t*)*element))->temp_avg);
    //free((((my_element_t*)*element))->last_modified);
    free(*element);
    *element = NULL;
}
int element_compare(void * x, void * y) {
    return ((((my_element_t*)x)->sensor_id < ((my_element_t*)y)->sensor_id) ? -1 : (((my_element_t*)x)->sensor_id == ((my_element_t*)y)->sensor_id) ? 0 : 1);
}


dplist_t *dpl_create(// callback functions
        void *(*element_copy)(void *src_element),
        void (*element_free)(void **element),
        int (*element_compare)(void *x, void *y)
) {
    dplist_t *list;
    list = malloc(sizeof(struct dplist));
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_MEMORY_ERROR);
    list->head = NULL;
    list->element_copy = element_copy;
    list->element_free = element_free;
    list->element_compare = element_compare;
    return list;
}

void dpl_free(dplist_t **list, bool free_element) {

    if (*list != NULL){
		dplist_node_t *node = (*list)->head;
		if (node == NULL){
			free(*list);	
			*list = NULL; 
		}
		else {
            while (node->next != NULL)
            {
                node = node->next;
                dpl_remove_at_index(*list, 0, free_element);	  
            }
			free(*list);	
			*list = NULL;
		}
	}
}

dplist_t *dpl_insert_at_index(dplist_t *list, void *element, int index, bool insert_copy) {

    dplist_node_t *ref_at_index, *list_node;
    if (list == NULL) return NULL;

    list_node = malloc(sizeof(dplist_node_t));
    DPLIST_ERR_HANDLER(list_node == NULL, DPLIST_MEMORY_ERROR);

    if (insert_copy==true){
        list_node->element = (list->element_copy)(element);
    } else{
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

    
    if (list ==  NULL || list->head == NULL) {
    	return list; }

//removes first node	
    else if (index <= 0){
    	dplist_node_t *node = list->head;
        /*
        if(free_element){
            (*list).element_free(&(node->element));
            free(node->element);
            node->element = NULL;
        }*/
        list->head = node->next;
        node->next->prev = NULL;
        free(node);
    }
//removes last node
    else if (index >= dpl_size(list)-1){
        dplist_node_t *to_remove_node = dpl_get_reference_at_index(list, dpl_size(list)-1);
        /*
        if(free_element){
            (*list).element_free(&(to_remove_node->element));
            free(to_remove_node->element);
            to_remove_node->element = NULL;
        }*/
        to_remove_node->prev->next = NULL;
        free(to_remove_node);
        to_remove_node = NULL;
    	
    }
	else {
        dplist_node_t *to_remove_node = dpl_get_reference_at_index(list, index);
        assert(to_remove_node != NULL);
        /*
        if(free_element){
            (*list).element_free(&(to_remove_node->element));
            free(to_remove_node->element);
            to_remove_node->element = NULL;
        }*/
        to_remove_node->prev->next = to_remove_node->next;
        to_remove_node->next->prev = to_remove_node->prev;
        free(to_remove_node);
        to_remove_node = NULL;}

	return list;
}

int dpl_size(dplist_t *list) {

    if (list == NULL){
       	return -1;
    }
    else {
	    int count = 1;
	    dplist_node_t *node = list->head;
	    if (node == NULL){ return 0;}
	    else{
		    for (node = list->head; node->next != NULL; node = node->next) {
		    	count ++;
		    	}
		    return count;
		    }
    	}   
    return -1; 
}

void *dpl_get_element_at_index(dplist_t *list, int index) {

    if (list == NULL || list->head == NULL){return NULL;}
    else{
        dplist_node_t *node = dpl_get_reference_at_index(list,index);
        assert(node != NULL);
        return (void*)(node->element);
    }
    return NULL;

}

int dpl_get_index_of_element(dplist_t *list, void *element) {

    if (list == NULL || list->head == NULL){ return -1;}
    int count=0;
    dplist_node_t *node = list->head;
   
    DPLIST_ERR_HANDLER(node==NULL,DPLIST_MEMORY_ERROR);
	while (node->next != NULL && (list->element_compare)(element, node->element) != 0){
        node = node->next;
        count++;
    }
    if ((list->element_compare)(element, node->element) == 0){ 
			return count;
		}
	return -1;
}

dplist_node_t *dpl_get_reference_at_index(dplist_t *list, int index) {

    int count;
    dplist_node_t *dummy = list->head;
    DPLIST_ERR_HANDLER(list == NULL, DPLIST_INVALID_ERROR);

    if (list->head == NULL) return NULL;
    if (index >= dpl_size(list)){index = dpl_size(list)-1;}
    if (index <= 0){index = 0;}

    for (dummy = list->head, count = 0; dummy->next != NULL; dummy = dummy->next, count++) {
        if (count >= index) return dummy;
    }
    return dummy;
}

void *dpl_get_element_at_reference(dplist_t *list, dplist_node_t *reference) {

    if (reference == NULL){ return NULL;}

    if (list == NULL){ return NULL;}
    dplist_node_t *node = list->head;
    if (node == NULL){return NULL;}

    else{
        while (node->next != NULL){
            if (node == reference){
                return (void*)(node->element);
            }
            node = node->next;
        }
        if (node == reference){return (void*)(node->element);}
    }
    return NULL;
    
}


void *dpl_get_sensor_with_id(dplist_t *list, uint16_t id) {
    if (list == NULL || list->head == NULL){return NULL;}
    else{
        dplist_node_t *current_node = list->head;
        
        while(current_node != NULL){
            uint16_t current_id = current_node->element->sensor_id;
            if (current_id == id) return current_node;
            current_node = current_node->next;
        }
    }
    return NULL;
}

void print_content(dplist_t * list){
    if (list == NULL || list->head == NULL){printf("no list or empty list\n");}
    else{
        dplist_node_t *current_node = list->head;
        
        while(current_node->next != NULL){
            uint16_t current_id = current_node->element->sensor_id;
            uint16_t current_room_id = current_node->element->room_id;
            double running_average = current_node->element->temp_avg;
            time_t last_modified = current_node->element->last_modified;
            //int nrOfMeasurments = current_node->element->nrOfTemp;
            printf("%d  %d  %f  %ld\n", current_id, current_room_id, running_average, last_modified);
            current_node = current_node->next;
        }
    }
}


