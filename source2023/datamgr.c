#include <stdio.h>
#include "datamgr.h"
#include "config.h"
#include "lib/dplist.h"
#include "sbuffer.h"
#include "pthread.h"

typedef struct {
    uint16_t sensorID;
    uint16_t roomID;
    double running_avg;
    time_t last_modified;
    double sublist[RUN_AVG_LENGTH];
    int next;
}element_t;

void *element_copy(void *element){
    element_t *data = malloc(sizeof (element_t));

    data->sensorID = ((element_t *)element)->sensorID;
    data->roomID = ((element_t *)element)->roomID;
    data->running_avg = ((element_t *)element)->running_avg;
    data->last_modified = ((element_t *)element)->last_modified;
    for(int i=0;i<RUN_AVG_LENGTH;i++) data->sublist[i] = ((element_t *)element)->sublist[i];
    data->next = ((element_t *)element)->next;

    return (void *)data;
}

void element_free(void **element){
    free(*element);
    *element = NULL;
}

int element_compare(void *x, void *y){
    if(((element_t *)x)->running_avg > ((element_t *)y)->running_avg){
        return 1;
    }
    else if(((element_t *)x)->running_avg < ((element_t *)y)->running_avg){
        return -1;
    }
    else{
        return 0;
    }
}

extern sbuffer_t *buffer;
extern pthread_mutex_t mutex_reader;
dplist_t *list_map = NULL;

int extern write_to_log_process(char *msg);

int isInvalid(sensor_data_t *data){
    int invalid = 1;
    for(int i=0;i<dpl_size(list_map);i++){
        if(data->id == ((element_t *) dpl_get_element_at_index(list_map,i))->sensorID){
            invalid = 0;
            break;
        }
    }
    return invalid;
}

void init(FILE *fp){
    datamgr_free();     //if user call this function multiple times
    list_map = dpl_create(element_copy, element_free, element_compare);

    element_t *data_map = malloc(sizeof (element_t));
    uint16_t sensorID;
    uint16_t roomID;
    int index_map = 0;

    while(fscanf(fp,"%hu %hu", &roomID, &sensorID) == 2){
        data_map->sensorID = sensorID;
        data_map->roomID = roomID;
        data_map->running_avg = 0;
        data_map->last_modified = 0;
        data_map->next = 0;

        dpl_insert_at_index(list_map, data_map, index_map, true);
        index_map++;
    }
}

void datamgr_process_data(sensor_data_t *data){
    // find the data from list_map with same sensor id
    if(isInvalid(data) == 1){
        char msg[100];
        snprintf(msg,100,"%s%d","Received sensor data with invalid sensor node ID ", data->id);
        write_to_log_process(msg);
    }
    else{
        for(int i=0;i<dpl_size(list_map);i++){
            element_t *element = ((element_t *)dpl_get_element_at_index(list_map, i));
            if(data->id == element->sensorID && data->ts >= element->last_modified){
                // add the value into the element and update the timestamp
                element->sublist[element->next%RUN_AVG_LENGTH] = data->value;
                element->last_modified = data->ts;
                element->next++;
                // do the calculation if there are more than RUN_AVG_LENGTH
                if(element->next >= RUN_AVG_LENGTH){
                    double sum = 0;
                    for(int j=0;j<RUN_AVG_LENGTH;j++) sum = sum + element->sublist[j];
                    element->running_avg = sum / RUN_AVG_LENGTH;
                    if(element->running_avg > SET_MAX_TEMP){
                        printf("beyond max temperature\n");
                        char msg[100];
                        snprintf(msg, 100,"%s%d%s%f).","Sensor node ", element->sensorID, " reports it’s too hot (avg temp = ", element->running_avg);
                        write_to_log_process(msg);
                    }
                    else if(element->running_avg < SET_MIN_TEMP){
                        printf("below min temperature\n");
                        char msg[100];
                        snprintf(msg, 100,"%s%d%s%f).","Sensor node ", element->sensorID, " reports it’s too cold (avg temp = ", element->running_avg);
                        write_to_log_process(msg);
                    }
                }
                break;
            }
        }
    }
}

void datamgr_free(){
    if(list_map == NULL){

    }
    else {
        dpl_free(&list_map, true);
        list_map = NULL;
    }
}

void *create_datamgr(void *ptr){
    //this is a thread to keep read data from sbuffer and do the calculation
    FILE *fp = fopen("room_sensor.map","r");
    init(fp);
    sensor_data_t *data= malloc(sizeof(sensor_data_t));
    data->id = 1;
    int result = SBUFFER_SUCCESS;
    int need_process = 0;
    while(result == SBUFFER_SUCCESS){
        pthread_mutex_lock(&mutex_reader);
        result = sbuffer_remove(buffer,data);
        if(!data->read) need_process = 1;
        data->read = true;
        sbuffer_insert(buffer,data);
        pthread_mutex_unlock(&mutex_reader);
        if(data->id != 0 && need_process == 1){
            need_process = 0;
            datamgr_process_data(data);
        }
    }
    free(data);
    pthread_exit(NULL);
}