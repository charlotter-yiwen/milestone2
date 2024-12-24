#include <stdio.h>
#include <string.h>
#include "sensor_db.h"
#include "pthread.h"
#include "config.h"

// Global variable declarations
FILE *Fp;
extern sbuffer_t *bufferStoragemgr;
extern pthread_mutex_t mutex_reader;
extern int write_to_log_process(char *msg);
extern int create_log_process();
extern int end_log_process();

extern pid_t pid;

FILE * open_db(char * filename, bool append){
        Fp = NULL;
        if(append == 1){
            Fp = fopen(filename,"a");
        }
        else{
            Fp = fopen(filename,"w");
        }

    write_to_log_process("A new data.csv file has been created.");
        return Fp;
}

int insert_sensor(sensor_id_t id, sensor_value_t value_t, sensor_ts_t ts_t){

    fflush(Fp);
    char msg[120];
    snprintf(msg,120,"%s%hu%s","Data insertion from sensor ", id," succeeded.");
    write_to_log_process(msg);
    return 0;
}

int close_db(){

        fclose(Fp);
        write_to_log_process("The data.csv file has been closed.\n");
        return 0;
}

// Storage manager thread function
void *create_sensormgr() {
    open_db("data.csv",false);
    sensor_data_t *data= malloc(sizeof(sensor_data_t));
    data->id = 1;
    int result = SBUFFER_SUCCESS;
    while(result == SBUFFER_SUCCESS){
        pthread_mutex_lock(&mutex_reader);
        result = sbuffer_remove(bufferStoragemgr,data);
        if(!data->read) sbuffer_insert(bufferStoragemgr,data);
        pthread_mutex_unlock(&mutex_reader);
        if(data->id != 0 && data->read){
            insert_sensor(data->id,data->value,data->ts);
        }
    }
    close_db();
    free(data);
    pthread_exit(NULL);
}
