#include "connmgr.h"
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


extern int MAX_CONN;
extern int PORT;
extern sbuffer_t *buffer;
int extern write_to_log_process(char *msg);

static pthread_mutex_t mutex_monitor = PTHREAD_MUTEX_INITIALIZER;


//static const int MY_TIMEOUT = 5;

typedef struct conn {
    tcpsock_t   *client;
    sensor_id_t  sensorID;
    bool         timeout;
    bool         recv_thread_done;
    bool         monitor_thread_exit;
    time_t       last_active_time;
} conn_t;



static void send_end_of_stream(void) {
    sensor_data_t data;
    data.id    = 0;   // end
    data.value = 0;
    data.ts    = 0;
    data.read  = false;
    sbuffer_insert(buffer, &data);
}


static void *monitor_thread_func(void *ptr) {// monitor time out
    if (!ptr) pthread_exit(NULL);

    conn_t *conn = (conn_t *)ptr;

    while (1) {
        sleep(1);

        pthread_mutex_lock(&mutex_monitor);
        if (conn->monitor_thread_exit) {
            pthread_mutex_unlock(&mutex_monitor);
            break;
        }

        if (conn->timeout) {
            pthread_mutex_unlock(&mutex_monitor);
            break;
        }

        // 检查是否超时
        time_t now = time(NULL);
        if ((int)difftime(now, conn->last_active_time) >= TIMEOUT) {
            conn->timeout = true;
            if (conn->client) {
                tcp_close(&conn->client);
            }

            char msg[100];
            snprintf(msg, sizeof(msg),
                     "TCP connection closed from sensor %hu (TIMEOUT)", conn->sensorID);
            write_to_log_process(msg);
        }

        pthread_mutex_unlock(&mutex_monitor);
    }

    pthread_exit(NULL);
}


void *listen_to_a_sensor_node(void *ptr) {
    if (!ptr) pthread_exit(NULL);

    tcpsock_t *client = (tcpsock_t *)ptr;

    conn_t *conn = malloc(sizeof(conn_t));
    if (!conn) {
        write_to_log_process("malloc failed for conn_t.\n");
        tcp_close(&client);
        pthread_exit(NULL);
    }

    conn->client              = client;
    conn->sensorID            = 0;
    conn->timeout             = false;
    conn->recv_thread_done    = false;
    conn->monitor_thread_exit = false;
    conn->last_active_time    = time(NULL);

    pthread_t monitor_tid;
    if (pthread_create(&monitor_tid, NULL, monitor_thread_func, conn) != 0) {
        write_to_log_process("Failed to create monitor_thread.\n");
        free(conn);
        tcp_close(&client);
        pthread_exit(NULL);
    }

    int bytes, result;
    sensor_data_t data;
    bool first_packet = true;

    while (1) {
        // sensor ID
        bytes = sizeof(data.id);
        result = tcp_receive(client, (void *)&data.id, &bytes);
        if (result != TCP_NO_ERROR || bytes == 0) break;

        // temp
        bytes = sizeof(data.value);
        result = tcp_receive(client, (void *)&data.value, &bytes);
        if (result != TCP_NO_ERROR || bytes == 0) break;

        // timestamp
        bytes = sizeof(data.ts);
        result = tcp_receive(client, (void *)&data.ts, &bytes);
        if (result != TCP_NO_ERROR || bytes == 0) break;

        // last_active_time
        pthread_mutex_lock(&mutex_monitor);
        conn->last_active_time = time(NULL);
        pthread_mutex_unlock(&mutex_monitor);

        // first time insert
        if (first_packet) {
            first_packet = false;
            conn->sensorID = data.id;
            char msg[100];
            snprintf(msg, sizeof(msg),
                     "Sensor node %hu has opened a new connection.", conn->sensorID);
            write_to_log_process(msg);
        }

        // 设置 read=false，插入 sbuffer
        data.read = false;
        sbuffer_insert(buffer, &data);
    }

    if (result == TCP_CONNECTION_CLOSED) {
        char msg[100];
        snprintf(msg, sizeof(msg), "Close connection with id: %hu", data.id);
        write_to_log_process(msg);
    } else {
        char msg[100];
        snprintf(msg, sizeof(msg),
                 "Sensor node %hu closed the connection.", conn->sensorID);
        write_to_log_process(msg);
    }

    pthread_mutex_lock(&mutex_monitor);
    if (!conn->timeout && conn->client) {
        tcp_close(&conn->client);
    }
    // monitor thread exit
    conn->recv_thread_done    = true;
    conn->monitor_thread_exit = true;
    pthread_mutex_unlock(&mutex_monitor);

    pthread_join(monitor_tid, NULL);

    free(conn);
    pthread_exit(NULL);
}

void *create_connmgr(void * ptr) {
    int number_of_client = 0;
    tcpsock_t *server = NULL, *client = NULL;
    pthread_t *threads = NULL;

    printf("Test server is started.\n");
    write_to_log_process("Test server is started.");

    if (tcp_passive_open(&server, PORT) != TCP_NO_ERROR) {
        write_to_log_process("Error: tcp_passive_open failed.");
        pthread_exit(NULL);
    }

    threads = malloc(sizeof(pthread_t) * MAX_CONN);
    if (!threads) {
        write_to_log_process("Failed to allocate memory for threads array.");
        tcp_close(&server);
        pthread_exit(NULL);
    }

    while (number_of_client < MAX_CONN) {
        if (tcp_wait_for_connection(server, &client) != TCP_NO_ERROR) {
            write_to_log_process("tcp_wait_for_connection error.");
            break;
        }

        if (pthread_create(&threads[number_of_client], NULL,
                           listen_to_a_sensor_node, (void *)client) != 0) {
            write_to_log_process("Failed to create client thread.");
            tcp_close(&client);
            break;
        }

        number_of_client++;
        printf("Incoming client connection: %d, MAX_CONN: %d\n", number_of_client, MAX_CONN);
    }

    for (int i = 0; i < number_of_client; i++) {
        pthread_join(threads[i], NULL);
        printf("Client thread %d has finished.\n", i);
    }

    send_end_of_stream();

    if (server) {
        tcp_close(&server);
    }

    printf("Test server is shutting down.\n");
    write_to_log_process("Test server is shutting down.");

    free(threads);
    pthread_exit(NULL);
}
