#include <stdio.h>
#include "datamgr.h"
#include "config.h"
#include "lib/dplist.h"
#include "sbuffer.h"
#include "pthread.h"

extern sbuffer_t *buffer;
extern pthread_mutex_t mutex_reader;
int extern write_to_log_process(char *msg);

dplist_t *sensor_list = NULL;

typedef struct {
    uint16_t sensor_id;
    uint16_t room_id;
    double running_avg;
    time_t last_modified;
    double sublist[RUN_AVG_LENGTH];
    int next;
}element_t;

void *element_copy(void *element){
    element_t *data = malloc(sizeof (element_t));

    data->sensor_id = ((element_t *)element)->sensor_id;
    data->room_id = ((element_t *)element)->room_id;
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


void init(FILE *fp){
    datamgr_free();     //if user call this function multiple times
    sensor_list = dpl_create(element_copy, element_free, element_compare);

    element_t *data_map = malloc(sizeof (element_t));
    uint16_t sensor_id;
    uint16_t room_id;
    int index_map = 0;

    while(fscanf(fp,"%hu %hu", &room_id, &sensor_id) == 2){
        data_map->sensor_id = sensor_id;
        data_map->room_id = room_id;
        data_map->running_avg = 0;
        data_map->last_modified = 0;
        data_map->next = 0;

        dpl_insert_at_index(sensor_list, data_map, index_map, true);
        index_map++;
    }
}

void datamgr_process_data(sensor_data_t *data)
{
    bool found = false;
    int count = dpl_size(sensor_list);
    for(int i = 0; i < count; i++){
        element_t *el = (element_t *) dpl_get_element_at_index(sensor_list, i);
        if(data->id == el->sensor_id) {
            found = true;

            if(data->ts >= el->last_modified){
                int pos = el->next % RUN_AVG_LENGTH;
                el->sublist[pos] = data->value;
                el->last_modified = data->ts;
                el->next++;

                if(el->next >= RUN_AVG_LENGTH){
                    double sum = 0;
                    for(int j = 0; j < RUN_AVG_LENGTH; j++){
                        sum += el->sublist[j];
                    }
                    el->running_avg = sum / RUN_AVG_LENGTH;

                    if(el->running_avg > SET_MAX_TEMP){
                        printf("average temperature %f beyond max temperature %d\n",el->running_avg, SET_MAX_TEMP);
                        char msg[100];
                        snprintf(msg, 100, "%s%d%s%f).",
                                 "Sensor node ",
                                 el->sensor_id,
                                 " reports it’s too hot (avg temp = ",
                                 el->running_avg);
                        write_to_log_process(msg);
                    }
                    else if(el->running_avg < SET_MIN_TEMP){
                        printf("average temperature %f below min temperature %d\n",el->running_avg, SET_MIN_TEMP);
                        char msg[100];
                        snprintf(msg, 100, "%s%d%s%f).",
                                 "Sensor node ",
                                 el->sensor_id,
                                 " reports it’s too cold (avg temp = ",
                                 el->running_avg);
                        write_to_log_process(msg);
                    }
                }
            }
            break;
        }
    }

    if(!found){
        char msg[100];
        snprintf(msg, sizeof(msg),
                 "Received sensor data with invalid sensor node ID %u",
                 data->id);
        write_to_log_process(msg);
    }
}


void datamgr_free(){
    if(sensor_list){
        dpl_free(&sensor_list, true);  // 第二个参数 true => 调用 element_free
        sensor_list = NULL;
    }
}

void *create_datamgr(void *ptr){//the data mannager thread read data from sbuffer and do calculation
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