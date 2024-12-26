#include <stdio.h>
#include <string.h>
#include "sensor_db.h"
#include <errno.h>
#include "pthread.h"
#include "sbuffer.h"

extern sbuffer_t *buffer;
extern pthread_mutex_t mutex_reader;
int extern write_to_log_process(char *msg);


FILE *open_db(char *filename, bool append) {
    FILE *file = fopen(filename, append ? "a" : "w");
    if (!file) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Failed to open file %s: %s", filename, strerror(errno));
        write_to_log_process(error_msg);
        return NULL;
    }

    write_to_log_process("A new data.csv file has been created.");
    return file;
}


int insert_sensor(FILE *f, sensor_id_t id, sensor_value_t value, sensor_ts_t ts) {
    if (!f) {
        write_to_log_process("Error: File pointer is NULL when inserting sensor data.");
        return -1;
    }
    if (fprintf(f, "%d, %.6f, %ld\n", id, value, ts) < 0) {
        write_to_log_process("Error: Writing sensor data failed.");
        return -1;
    }
    fflush(f);
    char msg[100];
    snprintf(msg, sizeof(msg), "Data insertion from sensor %d succeeded.", id);
    write_to_log_process(msg);
    return 0;
}

int close_db(FILE *f) {
    if (!f) {
        write_to_log_process("Error: File pointer is NULL when closing the database.");
        return -1;
    }
    fclose(f);
    write_to_log_process("The data.csv file has been closed");

    return 0;
}


void *create_storagemgr(void *ptr){//The storage manager thread reads sensor measurements from the shared data buffer and inserts them into a csvfile
    FILE *fp = open_db("data.csv", false);
    sensor_data_t *data= malloc(sizeof(sensor_data_t));
    if (!data) {
        write_to_log_process("Error: malloc failed in create_storagemgr.");
        close_db(fp);
        pthread_exit(NULL);
    }
    data->id = 1;
    int result = SBUFFER_SUCCESS;
    while(result == SBUFFER_SUCCESS){
        pthread_mutex_lock(&mutex_reader);
        result = sbuffer_remove(buffer,data);
        if(!data->read) sbuffer_insert(buffer,data);
        pthread_mutex_unlock(&mutex_reader);
        if(data->id != 0 && data->read){
            insert_sensor(fp,data->id,data->value,data->ts);
        }else if (data->id == 0) {
            break;
        }
    }
    close_db(fp);
    free(data);
    pthread_exit(NULL);
}