#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <pthread.h>
#include <unistd.h>
#include "config.h"
#include "lib/tcpsock.h"
#include "sbuffer.h"

// Global variable declarations
extern int MAX_CONN;
extern int PORT;
extern sbuffer_t *bufferStoragemgr;
extern int write_to_log_process(char *msg);
pthread_mutex_t mutex_monitor;

typedef struct conn{
    tcpsock_t *client;
    bool receive_msg;
    bool timeout;
    sensor_id_t sensorID;
}conn_t;

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
        if ((result == TCP_NO_ERROR) && bytes) sbuffer_insert(bufferStoragemgr, &data);
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
        snprintf(msg,50,"%s%hu","Closing the connection with sensor: ", data.id);
        write_to_log_process(msg);
    }
    else{
        char msg[50];
        snprintf(msg,50,"%s%hu%s","Sensor ", data.id," has closed the connection");
        write_to_log_process(msg);
    }
    if(!conn->timeout){
        tcp_close(&client);
    }
    return NULL;
}

// Send end of data stream signal
void send_end_of_stream() {
    printf("Sending the end signal of the data stream\n");
    sensor_data_t *data = malloc(sizeof(sensor_data_t));
    data->id = 0;
    sbuffer_insert(bufferStoragemgr, data);
    free(data);
}

// Connection manager thread function
void *create_connmgr(){
    int number_of_client = 1;

    // Start the server, listening on the port
    tcpsock_t *server, *client;
    pthread_t thread_poll[MAX_CONN - 1];

    printf("Test server started\n");
    write_to_log_process("Test server started.\n");

    if (tcp_passive_open(&server, PORT) != TCP_NO_ERROR) {
        exit(EXIT_FAILURE);
    }

    do {
        if (tcp_wait_for_connection(server, &client) != TCP_NO_ERROR) {
            exit(EXIT_FAILURE);
        }

        if (number_of_client < MAX_CONN) {
            pthread_t thread;
            if (pthread_create(&thread, NULL, listen_to_a_sensor_node, (void *)client) != 0) {
                exit(EXIT_FAILURE);
            }
            thread_poll[number_of_client - 1] = thread;

            printf("New client connected\n");
            printf("Number of clients: %d\n", number_of_client);
            printf("Maximum number of clients: %d\n", MAX_CONN);
        }

        number_of_client++;
    } while (number_of_client < MAX_CONN);

    // Wait for all client threads to join
    for (int i = 0; i < MAX_CONN; i++) {
        pthread_join(thread_poll[i], NULL);
        printf("Thread %d completed.\n", i);
    }

    // Send end of data stream signal
    send_end_of_stream();

    if (tcp_close(&server) != TCP_NO_ERROR) {
        exit(EXIT_FAILURE);
    }

    printf("Test server is closing\n");
    write_to_log_process("Test server is closing.\n");

    return NULL;
}
