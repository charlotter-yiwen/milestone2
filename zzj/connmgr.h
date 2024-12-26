//
// Created by marg_zhao on 8/12/23.
//

#ifndef STUDENTSOURCE2023_CONNMGR_H
#define STUDENTSOURCE2023_CONNMGR_H
#include "lib/tcpsock.h"
#include "sbuffer.h"
#include "config.h"
#ifndef TIMEOUT
#error "timeout not defined"
#endif
//typedef struct thread_data thread_data_t;
void *handle_client(void *arg);
int connmgr_run(int port, int max, sbuffer_t *buf);
#endif //STUDENTSOURCE2023_CONNMGR_H
