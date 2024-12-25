
#include "datamgr.h"


typedef struct{
    uint16_t sensor_id;
    uint16_t room_id;
    double temp_avg;
    time_t last_modified;
    double temperatures[RUN_AVG_LENGTH];
    int nrOfTemp;
} my_element_t;


dplist_t *list;

void datamgr_main(FILE *fp_sensor_map, sbuffer_t *buffer, int p[2], pthread_mutex_t buffer_lock, pthread_cond_t condition){
    my_element_t *sensor;
    list = dpl_create(element_copy, element_free, element_compare);
    sensor = malloc(sizeof(my_element_t));
    uint16_t sensor_id, room_id;

    char msg[LOG_MESSAGE_SIZE];

// let's read sensor map file & initiate all sensors
    while(feof(fp_sensor_map) == 0){
        fscanf(fp_sensor_map, "%hd %hd\n", &room_id, &sensor_id);
        sensor->sensor_id = sensor_id;
        sensor->room_id = room_id;
        sensor->temp_avg = 0.0; //set average to 0 for now
        sensor->last_modified = 0;
        sensor->nrOfTemp = 0;
// add sensor to linked list
        dpl_insert_at_index(list,sensor, dpl_size(list),true); 

        if (feof(fp_sensor_map) != 0){
            dpl_insert_at_index(list,sensor, dpl_size(list),true); 
        } 
    }

// let's read data from buffer
    pthread_mutex_lock(&buffer_lock);
    sbuffer_node_t *node = malloc(sizeof(sbuffer_node_t));
    node = buffer->head;

    if (node == NULL) {  // means buffer is empty
        while (node == NULL){
            pthread_cond_wait(&condition, &buffer_lock); // wait for data
            printf("\nwaiting for data\n");
        }
    }
    pthread_cond_signal(&condition);

while(node != NULL){
    sensor_data_t data = node->data;
    sensor_id_t id;
    sensor_value_t temperature;
    sensor_ts_t timestamp;

        if (node->read_by_datamgr==1){
            if (node->read_by_storagemgr==1) sbuffer_remove(buffer, &data, buffer_lock); //remove node if read by all threads
        }
        else{
        id = (&data)->id;
        temperature = (&data)->value;
        timestamp = (&data)->ts;
        node->read_by_storagemgr = 1;
        }

    int i = 0;
    double sum;
    // get sensor with corresponding id from reading from list
    sensor = dpl_get_sensor_with_id(list, id);
        if (sensor == NULL) {
            sprintf(msg, "Received sensor data with invalid sensor node %d.\n", id);
            write_into_log_pipe(msg);
        }
       
    if (sensor != NULL){
    // we don't have 5 measurments yet
        if ((sensor->nrOfTemp) < RUN_AVG_LENGTH){
            sensor->temperatures[(sensor->nrOfTemp)] = temperature;
            (sensor->nrOfTemp) ++;
        }
    // we have more than 5 measurments     
        if ((sensor->nrOfTemp) >= RUN_AVG_LENGTH){
            sensor->temperatures[RUN_AVG_LENGTH-1] = temperature;
            sensor->nrOfTemp ++;
        // re-compute average each time
            sum = 0;
            for (i=0; i<RUN_AVG_LENGTH; i++){
                sum += sensor->temperatures[i];
            }
            double avg = sum/RUN_AVG_LENGTH;
            sensor->temp_avg = avg;
            if (avg < SET_MIN_TEMP){
                sprintf(msg, "Sensor node %d reports it’s too cold (avg temp = %f).\n", id, avg);
                write_into_log_pipe(msg);
            }
            if (avg > SET_MAX_TEMP){
                sprintf(msg, "Sensor node %d reports it’s too hot (avg temp = %f).\n", id, avg);
                write_into_log_pipe(msg);
            }
            sensor->last_modified = timestamp; 
        
            for (i=0; i<RUN_AVG_LENGTH; i++){
                if (i == RUN_AVG_LENGTH-1) break;
                sensor->temperatures[i] = sensor->temperatures[i+1];
            }  
        }
    // we have exactly 5 measurments so we compute the first average
        if ((sensor->nrOfTemp) == RUN_AVG_LENGTH-1){ // compute first average of first 5 measurment readings
            sum = 0;
            for (i=0; i<RUN_AVG_LENGTH; i++){
                sum += sensor->temperatures[i];
            }
            double avg = sum/RUN_AVG_LENGTH;
            sensor->temp_avg = avg;
            if (avg < SET_MIN_TEMP){
                sprintf(msg, "Sensor node %d reports it’s too cold (avg temp = %f).\n", id, avg);
                write_into_log_pipe(msg);
            }
            if (avg > SET_MAX_TEMP){
                sprintf(msg, "Sensor node %d reports it’s too hot (avg temp = %f).\n", id, avg);
                write_into_log_pipe(msg);
            }
            sensor->last_modified = timestamp;
            for (i=0; i<RUN_AVG_LENGTH; i++){
                if (i == RUN_AVG_LENGTH-1) break;
                sensor->temperatures[i] = sensor->temperatures[i+1];
            }  
        }
    }
       node = node->next; 
}
    pthread_mutex_unlock(&buffer_lock);
    print_content(list);  
}

void datamgr_free(){
    dpl_free(&list, true);
}

