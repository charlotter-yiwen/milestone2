#include "sbuffer.h"

#ifndef STUDENTSOURCE2023_CONNMGR_H
#define STUDENTSOURCE2023_CONNMGR_H

#ifndef TIMEOUT
#define TIMEOUT 5
#endif

void *listen_to_a_sensor_node(void *);
void *create_connmgr(void *);

#endif //STUDENTSOURCE2023_CONNMGR_H
