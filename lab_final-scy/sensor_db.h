/**
 * \author Bert Lagaisse
 */

#ifndef _SENSOR_DB_H_
#define _SENSOR_DB_H_

#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "sbuffer.h"

#include <stdbool.h>

typedef struct File File;

FILE *open_db(char * filename, bool append);
//int insert_sensor(FILE *f,sensor_id_t id, sensor_value_t value, sensor_ts_t ts);
int insert_sensor(sensor_id_t id, sensor_value_t value, sensor_ts_t ts);
int close_db();
void *create_sensormgr();
#endif /* _SENSOR_DB_H_ */