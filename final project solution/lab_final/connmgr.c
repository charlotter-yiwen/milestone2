#include "connmgr.h"

/**
 * Implements a sequential test server (only one connection at the same time)
 */
int bytes, result;
int conn_counter = 0;
sensor_data_t data;


pthread_mutex_t my_buffer_lock;  // Mutex for synchronizing access to buffer
pthread_cond_t my_condition; // Buffer condition
sbuffer_t *my_buffer; // Shared buffer

void * connection_thread(void *client){
    char msg[LOG_MESSAGE_SIZE];
    int first_measurment = 1;

        do {
            // read sensor ID
            bytes = sizeof(data.id);
            result = tcp_receive(client, (void *) &data.id, &bytes);
            // read temperature
            bytes = sizeof(data.value);
            result = tcp_receive(client, (void *) &data.value, &bytes);
            // read timestamp
            bytes = sizeof(data.ts);
            result = tcp_receive(client, (void *) &data.ts, &bytes);
            if (first_measurment==1){
                sprintf(msg, "Sensor node %d has opened a new connection.\n", data.id);
                write_into_log_pipe(msg);
                first_measurment = 0;
            }
            if ((result == TCP_NO_ERROR) && bytes) {
                printf("sensor id = %" PRIu16 " - temperature = %g - timestamp = %ld\n", data.id, data.value, (long int) data.ts);
                if (sbuffer_insert(my_buffer, &data, my_buffer_lock, my_condition)== SBUFFER_SUCCESS) printf("measurment added to buffer\n");
            }
        } 
        while (result == TCP_NO_ERROR);
        if (result == TCP_CONNECTION_CLOSED)
            printf("Peer has closed connection\n");
        else
            printf("Error occured on connection to peer\n");
        tcp_close(client);

        pthread_exit(NULL);
        return 0;
}

int connectionmgr_main(int port, sbuffer_t *buffer, pthread_mutex_t buffer_lock, pthread_cond_t condition) {

    tcpsock_t *server, *client;
    pthread_t connection[MAX_CONN];
    int i = 0;

    my_buffer = buffer;
    my_buffer_lock = buffer_lock;
    my_condition = condition;

    printf("Test server is started\n");
    if (tcp_passive_open(&server, port) != TCP_NO_ERROR) exit(EXIT_FAILURE);

do {

    if (tcp_wait_for_connection(server, &client) != TCP_NO_ERROR) exit(EXIT_FAILURE);
        printf("Incoming client connection\n");
        conn_counter++; 

    if (pthread_create(&connection[conn_counter], NULL, connection_thread, client) != 0){
        printf("failed to create thread\n"); }
}
    while (conn_counter < MAX_CONN);

if (conn_counter > MAX_CONN){
    if (tcp_close(&server) != TCP_NO_ERROR){ exit(EXIT_FAILURE); 
    printf("Test server is shutting down\n");
    }
    write_into_log_pipe("Test server reached maximum connections and is shutting down\n");
    
}    
    while(i<MAX_CONN){
        pthread_join(connection[i], NULL);
        i++;
    }

    return 0;
}

