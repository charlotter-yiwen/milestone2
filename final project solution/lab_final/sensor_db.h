#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

#include "config.h"


// creates or open existing file, if append is true opens for append if false clears file and overwrites
// gets pipe passsed as parameter to be able to write log message
FILE * open_db(char * filename, bool append);


// inserts data in csv form in file that's given as parameter
// gets pipe passsed as parameter to be able to write log message
int insert_sensor(FILE * f, sensor_id_t id, sensor_value_t value, sensor_ts_t ts);

// closes file and writes log message into pipe
int close_db(FILE * f);

