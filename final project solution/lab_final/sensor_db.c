
#include "sensor_db.h"

char msg[LOG_MESSAGE_SIZE];

FILE * open_db(char * filename, bool append){

    sprintf(msg, "A new csv file is created or an existing file has been opened.\n");
    write_into_log_pipe(msg);

    if (append){
        return fopen(filename, "a"); //checks if file exists, if not, new file created for appending
    }        
    else{
        return fopen(filename, "w"); //if file exists, its contents are destroyed, if not it will be created
    }

}

int insert_sensor(FILE * f, sensor_id_t id, sensor_value_t value, sensor_ts_t ts){
    if (f != NULL){
        fprintf(f, "%d,%f,%ld,\n", id, value, ts);

        sprintf(msg, "Data insertion from sensor %d succeeded.\n", id);
        write_into_log_pipe(msg);
     
        return 0;
    }else{
        perror("failed to open file");
        
        sprintf(msg, "An error occurred when writing to the csv file.\n");
        write_into_log_pipe(msg);

        return 1;
    }
    return 1;
}

int close_db(FILE * f){
    if (f != NULL){
        fclose(f);
        sprintf(msg, "The csv file has been closed.\n");
        write_into_log_pipe(msg);

        return 0;}
    return 1;
}
