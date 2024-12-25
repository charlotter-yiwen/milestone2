#include <stdio.h>
#include <string.h>
#include "sensor_db.h"
#include "pthread.h"

extern sbuffer_t *buffer;
extern pthread_mutex_t mutex_reader;
int extern write_to_log_process(char *msg);
FILE *fp;

FILE *open_db(char *filename, bool append){
    if(append){
        fp = fopen(filename, "a");
    }
    else
        fp = fopen(filename, "w");

    if(fp == NULL){
        return NULL;
    }
    write_to_log_process("A new data.csv file has been created.");
    return fp;
}

int insert_sensor(sensor_id_t id, sensor_value_t value, sensor_ts_t ts){
    if(fprintf(fp, "%hu,%f,%ld\n", id, value, ts) < 0){
        return -1;
    }
    char msg[100];
    snprintf(msg,100,"%s%hu%s","Data insertion from sensor ", id," succeeded.");
    write_to_log_process(msg);
    return 0;
}

int close_db(){
    if(fclose(fp) != 0){
        return -1;
    }
    write_to_log_process("The data.csv file has been closed.");
    return 0;
}

void *create_storagemgr(void *ptr){
    //this is a thread to read data from sbuffer and write it in to a csv file
    open_db("data.csv",false);
    sensor_data_t *data= malloc(sizeof(sensor_data_t));
    data->id = 1;
    int result = SBUFFER_SUCCESS;
    while(result == SBUFFER_SUCCESS){
        pthread_mutex_lock(&mutex_reader);
        result = sbuffer_remove(buffer,data);
        if(!data->read) sbuffer_insert(buffer,data);
        pthread_mutex_unlock(&mutex_reader);
        if(data->id != 0 && data->read){
            insert_sensor(data->id,data->value,data->ts);
        }
    }
    close_db();
    free(data);
    pthread_exit(NULL);
}