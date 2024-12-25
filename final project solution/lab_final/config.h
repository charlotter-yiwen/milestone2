/**
 * \author Salma Loukili
 */

#ifndef _CONFIG_H_
#define _CONFIG_H_


#include <stdint.h>
#include <time.h>
#include <stdbool.h>

#define LOG_MESSAGE_SIZE 5000

typedef uint16_t sensor_id_t;
typedef double sensor_value_t;
typedef time_t sensor_ts_t;         // UTC timestamp as returned by time() - notice that the size of time_t is different on 32/64 bit machine

typedef struct {
    sensor_id_t id;
    sensor_value_t value;
    sensor_ts_t ts;
} sensor_data_t;

/**
 * basic node for the buffer, these nodes are linked together to create the buffer
 */
typedef struct sbuffer_node {
    struct sbuffer_node *next;  /**< a pointer to the next node*/
    sensor_data_t data;         /**< a structure containing the data */
    int read_by_datamgr;
    int read_by_storagemgr;
    int readers[2];
} sbuffer_node_t;


/**
 * a structure to keep track of the buffer
 */
struct sbuffer {
    sbuffer_node_t *head;       /**< a pointer to the first node in the buffer */
    sbuffer_node_t *tail;       /**< a pointer to the last node in the buffer */

};

void write_into_log_pipe(char *msg);


#endif /* _CONFIG_H_ */
