//
// Created by marg_zhao on 15/11/23.
//
#include "sensor_db.h"
#include "stdio.h"
#include <inttypes.h>
#include <unistd.h>


char msg[256];

FILE * open_db(char * filename, bool append){
    FILE * file;
    if (append) {
        file = fopen(filename, "a");
    } else {
        file = fopen(filename, "w");
    }
    if (file == NULL) {
        perror("Error opening file");
    }
    printf("file open\n");


    write_into_log_pipe("Data.csv file is created.");

    return file;
}

int insert_sensor(FILE * f, sensor_id_t id, sensor_value_t value, sensor_ts_t ts){
    if(f == NULL){
        write_into_log_pipe("Invalid file to write.");
        perror("Error writing to file");
        return -1;
    }

    fprintf(f, "%" PRIu16 ", %.6f, %" PRId64,"\n", id, value, (int64_t)ts);
    sprintf(msg,"Data insertion from sensor %d succeeded.",id);
    write_into_log_pipe(msg);
    // fprintf(fp_text, "%" PRIu16 " %" PRIu16 "\n", room_id[i], sensor_id[i]);
    return 0;
}
int close_db(FILE * f){
    if(f == NULL){
        write_into_log_pipe("Invalid file to close.");
        return -1;
    }

    fclose(f);
    write_into_log_pipe("Data.csv file closed.");
    printf("file closed\n");
    printf("end log process");
    return 1;
}