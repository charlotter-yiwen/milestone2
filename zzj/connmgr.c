//
// Created by marg_zhao on 8/12/23.
//
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/time.h>
#include "connmgr.h"
int conn_counter;

typedef struct {
    tcpsock_t *client;
    sbuffer_t *buf;
} thread_data_t;

//client runner
void *handle_client(void *arg) {
    thread_data_t* thread_data = (thread_data_t *)arg;
    tcpsock_t *client = thread_data->client;
    sbuffer_t *buf = thread_data->buf;
    char msg[256];
    sensor_data_t data;
    int conn_id = conn_counter;
    int first= 1;

    int bytes, result;
    fd_set readfds;
    struct timeval timeout;

    do {
        FD_ZERO(&readfds);
        FD_SET(client->sd, &readfds);

        timeout.tv_sec = TIMEOUT;
        timeout.tv_usec = 0;

        result = select(client->sd + 1, &readfds, NULL, NULL, &timeout);

        if (result == 0) {
            // Timeout occurred, handle it accordingly (e.g., close the connection)
            printf("Connection #%d - Timeout occurred, closing the connection\n", conn_id);
            break;
        } else if (result < 0) {
            // Error in select, handle it accordingly
            perror("select");
            break;
        }

        // read sensor ID
        bytes = sizeof(data.id);
        result = tcp_receive(client, (void *)&data.id, &bytes);

        // read temperature
        bytes = sizeof(data.value);
        result = tcp_receive(client, (void *)&data.value, &bytes);

        // read timestamp
        bytes = sizeof(data.ts);
        result = tcp_receive(client, (void *)&data.ts, &bytes);

        if(first == 1){
            sprintf(msg,"Sensor node %d has opened a new connection", data.id);
            write_into_log_pipe(msg);
            first = 0;
        }

        if ((result == TCP_NO_ERROR) && bytes) {
            printf("Connection #%d - sensor id = %" PRIu16 " - temperature = %g - timestamp = %ld\n", conn_id,
                   data.id, data.value, (long int)data.ts);
            sbuffer_insert(buf,&data);
        }
        //TODO:time out
    } while (result == TCP_NO_ERROR);

    if (result == TCP_CONNECTION_CLOSED)
        printf("Connection #%d - Peer has closed connection\n", conn_counter);
    else
        printf("Connection #%d - Error occurred on connection to peer\n", conn_counter);

    if(tcp_close(&client)==TCP_NO_ERROR){
        printf("close success\n");
        sprintf(msg,"Sensor node %d has closed the connection", data.id);
        write_into_log_pipe(msg);
    }
    else{
        printf("close failure\n");
    }
    printf("client close\n");
    pthread_exit(NULL);
}

int connmgr_run(int port,int max, sbuffer_t *buf){
    tcpsock_t *server;
    tcpsock_t **clients;
    //sensor_data_t data;
    conn_counter = 0;

    clients = (tcpsock_t **) malloc(max*sizeof(tcpsock_t*));
    pthread_t* thread;
    thread = (pthread_t*) malloc(max*sizeof(pthread_t));
    printf("Test server is started\n");
    if (tcp_passive_open(&server, port) != TCP_NO_ERROR) exit(EXIT_FAILURE);

    while (conn_counter < max) {
        // wait for new conn request
        if (tcp_wait_for_connection(server, &clients[conn_counter]) != TCP_NO_ERROR) exit(EXIT_FAILURE);
        printf("Incoming client connection\n");
        // new thread for new conn
        thread_data_t thread_data = {clients[conn_counter], buf};
        if (pthread_create(&thread[conn_counter], NULL, handle_client, (void *)&thread_data) != 0) {
            fprintf(stderr, "Error creating thread\n");
            exit(EXIT_FAILURE);
        }
        conn_counter++;
    }
    for(int i = 0; i<max; i++){
        pthread_join(thread[i],NULL);
    }

    if (tcp_close(&server) != TCP_NO_ERROR) exit(EXIT_FAILURE);

    free(clients);
    free(thread);

    printf("Test server is shutting down\n");
    return 0;
}
