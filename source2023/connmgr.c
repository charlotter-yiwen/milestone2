#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <pthread.h>
#include <sys/socket.h>
#include "config.h"
#include "lib/tcpsock.h"
#include "sbuffer.h"

pthread_mutex_t mutex_monitor;
extern int MAX_CONN;
extern int PORT;
extern sbuffer_t *buffer;
int extern write_to_log_process(char *msg);

typedef struct conn{
    tcpsock_t *client;
    bool receive_msg;
    bool timeout;
    sensor_id_t sensorID;
}conn_t;

void send_end_of_stream() {
    printf("send end of dtream\n");
    sensor_data_t *data = malloc(sizeof(sensor_data_t));
    data->id=0;
    sbuffer_insert(buffer,data);
    free(data);
}

void *monitor_thread(void *ptr){
    if(ptr == NULL) return NULL;
    sleep(TIMEOUT);
    conn_t *conn = (conn_t *) ptr;
    conn->receive_msg = false;
    if(!conn->receive_msg){
        pthread_mutex_lock(&mutex_monitor);
        tcp_close(&(conn->client));
        conn->timeout = true;
        char msg[100];
        snprintf(msg,100,"%s%hu%s","TCP connection closed from sensor ", conn->sensorID,"(TIMEOUT)");
        printf("%s",msg);
        fflush(stdout);
        write_to_log_process(msg);
        pthread_mutex_unlock(&mutex_monitor);
    }
    return NULL;
}

void *listen_to_a_sensor_node(void *ptr) {
    if (ptr == NULL) return NULL;

    tcpsock_t *client = (tcpsock_t *) ptr;
    pthread_t thread;
    conn_t *conn = malloc(sizeof(conn_t));
    conn->client = client;
    conn->receive_msg = false;
    conn->timeout = false;
    conn->sensorID = 0;
    int first_time = 0;
    int bytes, result;
    sensor_data_t data;
    do {
        pthread_create(&thread, NULL, monitor_thread, (void *)conn);
        // read sensor ID
        bytes = sizeof(data.id);
        result = tcp_receive(client, (void *) &data.id, &bytes);
        // read temperature
        bytes = sizeof(data.value);
        result = tcp_receive(client, (void *) &data.value, &bytes);
        // read timestamp
        bytes = sizeof(data.ts);
        result = tcp_receive(client, (void *) &data.ts, &bytes);
        pthread_mutex_lock(&mutex_monitor);
        conn->receive_msg = true;
        pthread_mutex_unlock(&mutex_monitor);
        data.read = false;
        if ((result == TCP_NO_ERROR) && bytes) sbuffer_insert(buffer, &data);
        if(first_time == 0){
            first_time = 1;
            conn->sensorID = data.id;
            char msg[50];
            snprintf(msg,50,"%s%hu%s","Sensor node ", data.id," has opened a new connection.");
            write_to_log_process(msg);
        }
        pthread_cancel(thread);
    } while (result == TCP_NO_ERROR);
    pthread_join(thread,NULL);
    if (result == TCP_CONNECTION_CLOSED) {
        char msg[50];
        snprintf(msg,50,"%s%hu","Close connection with id: ", data.id);
        write_to_log_process(msg);
    }
    else{
        char msg[50];
        snprintf(msg,50,"%s%hu%s","Sensor node ", data.id," closed the connection");
        write_to_log_process(msg);
    }
    if(!conn->timeout){
        tcp_close(&client);
    }
    return NULL;
}

void *create_connmgr(void * ptr){
    pthread_mutex_init(&mutex_monitor, NULL);
    int number_of_client = 0;
    // start a server, and listen to the port
    tcpsock_t *server, *client;
    pthread_t thread_poll[MAX_CONN - 1];

    printf("Test server is started\n");
    write_to_log_process("Test server is started.");
    if (tcp_passive_open(&server, PORT) != TCP_NO_ERROR) exit(EXIT_FAILURE);
    do {
        if (tcp_wait_for_connection(server, &client) != TCP_NO_ERROR) exit(EXIT_FAILURE);
        if(number_of_client < MAX_CONN){
            pthread_t thread;
            if(pthread_create(&thread, NULL, listen_to_a_sensor_node, (void *)client) != 0) exit(EXIT_FAILURE);
            thread_poll[number_of_client - 1] = thread;
            printf("Incoming client connection\n");
            printf("number of client: %d\n", number_of_client);
            printf("MAX_CONN: %d\n",MAX_CONN);
        }
        number_of_client++;
    } while (number_of_client < MAX_CONN);
    //wait all the client to join
    for (int i = 0; i < MAX_CONN ; i++) {
        pthread_join(thread_poll[i], NULL);
        printf("thread%d has finished.\n",i);
    }
    send_end_of_stream();
    if (tcp_close(&server) != TCP_NO_ERROR) exit(EXIT_FAILURE);
    printf("Test server is shutting down\n");
    write_to_log_process("Test server is shutting down.");
    pthread_mutex_destroy(&mutex_monitor);
    return NULL;
}