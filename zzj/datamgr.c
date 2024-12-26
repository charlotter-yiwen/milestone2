//
// Created by marg_zhao on 7/11/23.
//
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>
#include <float.h>
#include <assert.h>
#include <semaphore.h>
#include <unistd.h>
#include <pthread.h>
#include "datamgr.h"


//#define SET_MAX_TEMP 25
//#define SET_MIN_TEMP 10
// Internal data structure to hold sensor nodes
typedef struct {
    sensor_id_t id;
    int idr;/** < sensor id *//** < room id */
    sensor_value_t avg;   /** < sensor avg   */
    sensor_ts_t *ts;         /** < sensor timestamp */
    sensor_value_t last[RUN_AVG_LENGTH];
    int count;
    int first_five; //flag of first five
} node_data_t;

void* element_copy(void * element);
void element_free(void ** element);
int element_compare(void * x, void * y);
//dplist_t *map = NULL;
dplist_t *map;


void * element_copy(void * element) {
    node_data_t* copy = (node_data_t*)malloc(sizeof (node_data_t));
    //deep copy
    //cast to specific type before use
    assert(copy != NULL);
    copy->id = ((node_data_t*)element)->id;
    copy->idr = ((node_data_t*)element)->idr;
    copy->avg = ((node_data_t*)element)->avg;
    copy->ts = ((node_data_t*)element)->ts;
    return (void *) copy;
}

void element_free(void ** element) {
    //free((*((node_data_t **)element))->ts);
    free(*element);
    *element = NULL;
}

int element_compare(void * x, void * y) {
    return ((((node_data_t*)x)->id < ((node_data_t*)y)->id) ? -1 : (((node_data_t*)x)->id == ((node_data_t*)y)->id) ? 0 : 1);
    // cast first if equal return 0, < return -1, > return 1
    // compare timestamp
}


void ck_assert_msg(bool result, char * msg){
    if(!result) printf("%s\n", msg);
}

void datamgr_parse_sensor_files(FILE *fp_sensor_map, sbuffer_t* buf){
    map =  dpl_create(element_copy, element_free, element_compare);
    char msg[256];
    uint16_t room_id[20]={0};
    uint16_t sensor_id[20]={0};// buffer as read
    int i = 0;
    //create dplist node
    while (!feof(fp_sensor_map)) { //if end? not, continue
        fscanf(fp_sensor_map,"%" SCNu16 " %" SCNu16, &room_id[i], &sensor_id[i]); //source stream, format, where to put the value
        i++;
    }
    int total = i-1;
    for(i=0;i<total;i++){
        node_data_t* content = (node_data_t *)malloc(sizeof(node_data_t));
        content->idr = room_id[i];
        content->id  = sensor_id[i];
        content->ts = NULL;
        content->avg = -200;
        content->count = 0;
        content->first_five = 0;
        for(int a = 0;a<RUN_AVG_LENGTH; a++){
            content->last[a] = 0;
        }
        dpl_insert_at_index(map, content,total+1,true);
        free(content);
    }

    //read sbuffer
    while(buf->head==NULL){
        //wait for the first data
    }
    sbuffer_node_t* head;// = (sbuffer_t*) malloc(sizeof(sbuffer_t));
    sensor_data_t data;
    pthread_mutex_lock(&buf->mutex);
    while (buf->head!=NULL) {
        head = buf->head;
        pthread_mutex_unlock(&buf->mutex);
        data = head->data;
        if (head->data_read == 0) {
            double temperature= data.value;
            time_t timestamp=data.ts;// malloc(sizeof (time_t));
            dplist_node_t *current = map->head;
            node_data_t *element = NULL;

            //find the node
            while (current != NULL) {
                element = current->element;
                if (data.id == element->id) {
                    break;
                }
                current = current->next;
            }
            if(current == NULL){
                sprintf(msg, "Receive data with invalid sensor node ID %d  .\n", data.id);
                write_into_log_pipe(msg);
                continue;
            }
            sensor_value_t last5 = 0;
            // int count = element->count;
            element->ts = &timestamp;

            if (element->count < RUN_AVG_LENGTH) {
                element->last[element->count] = temperature;
                element->count += 1;
            } else {
                element->count = 0;
                element->first_five = 1;
                element->last[element->count] = temperature;
                element->count += 1;
            }
            for (int x = 0; x < RUN_AVG_LENGTH; x++) {
                last5 += element->last[x];
            }
            // determine if it is the first five incoming data
            if(element->first_five==1){
                element->avg = last5 / 5;
            }else{
                element->avg = last5 / element->count;
            }
            printf("sensor id: %d room id: %d average temp: %f timestamp: %s", element->id, element->idr, element->avg,
                   ctime(element->ts));
            if (element->avg > SET_MAX_TEMP) {
//                fprintf(stderr, "Log Event: Sensor %d in Room %d has an average temperature outside the range.\n",
//                        element->id, element->idr);
                sprintf(msg, "Sensor node %d reports it’s hot (avg temp = %f).\n", element->id, element->avg);
                write_into_log_pipe(msg);
            }
            if( element->avg < SET_MIN_TEMP){
                sprintf(msg, "Sensor node %d reports it’s too cold (avg temp = %f).\n", element->id, element->avg);
                write_into_log_pipe(msg);
            }
            head->data_read = 1;
        }
        sbuffer_remove(buf,&data,1);
        sleep(1);
        pthread_mutex_lock(&buf->mutex);
    }
    pthread_mutex_unlock(&buf->mutex);
}

void datamgr_free(){
    dpl_free(&map,true);
}

uint16_t datamgr_get_room_id(sensor_id_t sensor_id){
    //ERROR_HANDLER(map==NULL,"list is uninitialized");
    int idr = -1000;
    dplist_node_t *current = map->head;
    while(current!=NULL){
        node_data_t* element =(node_data_t*) current->element;
        if(element->id == sensor_id){
            idr =  element->idr;
            break;
        }
    }
    ERROR_HANDLER(idr==-1000,"invalid sensor_id");
    return idr;
}

sensor_value_t datamgr_get_avg(sensor_id_t sensor_id){
    ERROR_HANDLER(map==NULL,"list is uninitialized");
    sensor_value_t avg = DBL_MAX;
    dplist_node_t *current = map->head;
    while(current!=NULL){
        node_data_t* element =(node_data_t*) current->element;
        if(element->id == sensor_id){
            avg = element->avg;
            break;
        }
    }
    ERROR_HANDLER(avg== DBL_MAX,"invalid sensor_id");
    return avg;
}

time_t datamgr_get_last_modified(sensor_id_t sensor_id){
    ERROR_HANDLER(map==NULL,"list is uninitialized");
    sensor_ts_t* last = NULL;
    dplist_node_t *current = map->head;
    while(current!=NULL){
        node_data_t* element =(node_data_t*) current->element;
        if(element->id == sensor_id){
            last = element->ts;
            break;
        }
    }
    ERROR_HANDLER(last== NULL,"invalid sensor_id");
    return *last;
}

int datamgr_get_total_sensors(){
    ERROR_HANDLER(map==NULL,"list is uninitialized");
    int count = 0;
    if (map==NULL)
        return -1;

    dplist_node_t *current = map->head;
    while(current!=NULL){
        count++;
        current = current->next;
    }
    return count;
}