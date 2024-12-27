#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include "lib/tcpsock.h"
#include "sbuffer.h"
#include "config.h"

// 供日志进程使用的函数原型（由你在别的 .c 文件里实现）
extern int write_to_log_process(char *msg);

// 全局变量（从 config.h 中得到 extern 声明也可）
// ---------------------------------------
pthread_mutex_t mutex_monitor;    // 用于保护以下两变量
int active_connections = 0;       // 当前还活跃的连接数
int connections_closed = 0;       // 已经断开的连接数

extern int MAX_CONN;              // 最大允许客户端数
extern int PORT;                  // 监听的端口
extern sbuffer_t *buffer;         // 共享缓冲区，用于存放传感器数据

// 发送结束信号时，需要用到 sensor_data_t
// 这里可以是全局的也行，或在函数中临时定义都行
static sensor_data_t end_signal_data;

// 监控超时的结构体
typedef struct conn {
    tcpsock_t   *client_socket;
    bool         receive_msg;
    bool         timeout;
    sensor_id_t  sensorID;
} conn_t;

// ============= 工具函数：发送结束信号到 sbuffer =============
static void send_end_of_stream() {
    // 这里发送一个特殊的 id=0, value=0, ts=0 作为结束标记
    printf("Sending end-of-stream signal\n");  // 可选的调试信息
    end_signal_data.id    = 0;
    end_signal_data.value = 0;
    end_signal_data.ts    = 0;
    sbuffer_insert(buffer, &end_signal_data);
}

// ============= 超时监控线程 =============
static void *monitor_thread(void *arg) {
    conn_t *conn = (conn_t *)arg;
    // 等待 TIMEOUT 秒，如果还没收到任何数据，认为超时
    sleep(TIMEOUT);
    if (!conn->receive_msg) {
        pthread_mutex_lock(&mutex_monitor);
        tcp_close(&(conn->client_socket));
        conn->timeout = true;
        char msg[100];
        snprintf(msg, sizeof(msg),
                 "Sensor node %hu connection closed due to TIMEOUT",
                 conn->sensorID);
        write_to_log_process(msg);
        pthread_mutex_unlock(&mutex_monitor);
    }
    return NULL;
}

// ============= 处理每个客户端的线程函数 =============
static void *listen_to_sensor_node(void *arg) {
    tcpsock_t    *client_socket = (tcpsock_t *)arg;
    pthread_t     monitor_tid;
    conn_t        conn;               // 用于传给 monitor_thread
    sensor_data_t data;
    int           bytes, status;

    conn.client_socket = client_socket;
    conn.receive_msg   = false;
    conn.timeout       = false;
    conn.sensorID      = 0;

    // 创建超时监控线程
    pthread_create(&monitor_tid, NULL, monitor_thread, &conn);

    // 不断接收该客户端发送的数据包
    while (true) {
        bytes  = sizeof(data.id);
        status = tcp_receive(client_socket, &data.id, &bytes);
        if (status != TCP_NO_ERROR || bytes == 0) break;

        bytes  = sizeof(data.value);
        status = tcp_receive(client_socket, &data.value, &bytes);
        if (status != TCP_NO_ERROR || bytes == 0) break;

        bytes  = sizeof(data.ts);
        status = tcp_receive(client_socket, &data.ts, &bytes);
        if (status != TCP_NO_ERROR || bytes == 0) break;

        // 一旦接收到数据，则代表该连接活跃
        pthread_mutex_lock(&mutex_monitor);
        conn.receive_msg = true;
        pthread_mutex_unlock(&mutex_monitor);

        // 如果这是第一次收到数据，获取 sensorID 并打印日志
        if (conn.sensorID == 0) {
            conn.sensorID = data.id;
            char msg[100];
            snprintf(msg, sizeof(msg),
                     "Sensor node %hu has opened a new connection.",
                     conn.sensorID);
            write_to_log_process(msg);
        }

        // 把数据存到共享缓冲区
        sbuffer_insert(buffer, &data);
    }

    // 退出循环说明客户端断开或出错，终止监控线程
    pthread_cancel(monitor_tid);
    pthread_join(monitor_tid, NULL);

    // 如果是超时关闭，这里不再重复写日志
    if (!conn.timeout) {
        // 正常断开
        char msg[100];
        snprintf(msg, sizeof(msg),
                 "Sensor node %hu has closed the connection.", conn.sensorID);
        write_to_log_process(msg);

        tcp_close(&client_socket);
    }

    // 更新全局计数
    pthread_mutex_lock(&mutex_monitor);
    active_connections--;
    // 已经有一个客户端断开了
    connections_closed++;
    pthread_mutex_unlock(&mutex_monitor);

    return NULL;
}

// ============= 主函数：接受客户端连接的循环 =============
void connmgr_listen() {
    int number_of_client = 0;
    tcpsock_t *server_socket = NULL, *client_socket = NULL;
    pthread_t *threads = NULL;

    printf("Server is starting.\n");
    write_to_log_process("Server started.");

    if (tcp_passive_open(&server_socket, PORT) != TCP_NO_ERROR) {
        write_to_log_process("Error: Unable to open server socket on port.");
        pthread_exit(NULL);
    }

    threads = malloc(sizeof(pthread_t) * MAX_CONN);
    if (!threads) {
        write_to_log_process("Failed to allocate memory for threads array.");
        tcp_close(&server_socket);
        pthread_exit(NULL);
    }

    while (number_of_client < MAX_CONN) {
        if (tcp_wait_for_connection(server_socket, &client_socket) != TCP_NO_ERROR) {
            write_to_log_process("Error: Failed to accept connection.");
            continue;
        }

        if (pthread_create(&threads[number_of_client], NULL, listen_to_sensor_node, (void *)client_socket) != 0) {
            write_to_log_process("Failed to create thread for new client.");
            tcp_close(&client_socket);
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

    if (server_socket) {
        tcp_close(&server_socket);
    }

    printf("Server is shutting down.\n");
    write_to_log_process("Server shut down.");

    free(threads);
    pthread_exit(NULL);
}
