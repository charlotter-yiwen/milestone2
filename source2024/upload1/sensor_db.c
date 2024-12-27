#include "sensor_db.h"
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define _POSIX_C_SOURCE 200809L
#include <unistd.h>

// ========== 外部函数：写日志到 Log 进程 ==========
extern int write_to_log_process(char *msg);

// ========== 全局/静态变量 ==========
// 供写 CSV 的文件指针
static FILE *db_file = NULL;

// 互斥锁，确保多线程对 db_file 操作安全
static pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;

// ========== 工具函数：获取当前时间字符串 ==========
char *get_current_time_string() {
    static char time_str[32];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(time_str, sizeof(time_str), "%a %b %d %H:%M:%S %Y", tm_info);
    return time_str;
}

// ========== 打开数据库文件 ==========
// append = false 时表示要新建/覆盖 data.csv
FILE *open_db(char *filename, bool append) {
    pthread_mutex_lock(&db_mutex);

    if (append) {
        db_file = fopen(filename, "a");
    } else {
        db_file = fopen(filename, "w");
        if (db_file) {
            // 通过日志进程记录：新 data.csv 文件已被创建
            write_to_log_process("A new data.csv file has been created.");
        }
    }

    if (!db_file) {
        fprintf(stderr, "Error: Could not open the file %s\n", filename);
        pthread_mutex_unlock(&db_mutex);
        return NULL;
    } else {
        // 可选：也可以记录 "Database file data.csv opened."
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "Database file %s opened.", filename);
        write_to_log_process(msg);
    }

    pthread_mutex_unlock(&db_mutex);
    return db_file;
}

// ========== 插入一条传感器记录到 CSV ==========
// 返回 0 表示成功；-1 表示失败
int insert_sensor(sensor_id_t id, sensor_value_t value, sensor_ts_t ts) {
    pthread_mutex_lock(&db_mutex);

    // 如果想过滤 id=0，可在此处直接返回
    if (id == 0) {
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    if (!db_file) {
        fprintf(stderr, "Database file is not open.\n");
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    // 写入 CSV
    fprintf(db_file, "%u, %.2f, %ld\n", id, value, ts);
    fflush(db_file);  // 确保立刻写入磁盘

    // 通过日志进程记录：Data insertion succeeded
    char log_msg[100];
    snprintf(log_msg, sizeof(log_msg),
             "Data insertion from sensor %u succeeded.", id);
    write_to_log_process(log_msg);

    pthread_mutex_unlock(&db_mutex);
    return 0;
}

// ========== 关闭数据库文件 ==========
// 返回 0 表示成功；-1 表示失败
int close_db() {
    pthread_mutex_lock(&db_mutex);
    if (!db_file) {
        fprintf(stderr, "Database file is not open.\n");
        pthread_mutex_unlock(&db_mutex);
        return -1;
    }

    fclose(db_file);
    db_file = NULL;
    // 记录“data.csv file has been closed.”
    write_to_log_process("The data.csv file has been closed.");

    pthread_mutex_unlock(&db_mutex);
    return 0;
}

// ========== Storage Manager 线程函数 ==========
// 从 sbuffer 中不断读取数据，写入 CSV，直到检测到结束信号

void *create_storagemgr(void *ptr) {
    sbuffer_t *buffer = (sbuffer_t *)ptr;
    if (!buffer) {
        fprintf(stderr, "Error: Shared buffer is NULL.\n");
        pthread_exit(NULL);
    }

    // 打开数据库文件（覆盖模式）
    FILE *fp = open_db("data.csv", false);
    if (!fp) {
        fprintf(stderr, "Error: Failed to open database. Exiting storage manager thread.\n");
        pthread_exit(NULL);
    }

    // 循环从 sbuffer 移除数据
    sensor_data_t data;
    while (true) {
        int result = sbuffer_remove(buffer, &data);

        if (result == SBUFFER_NO_DATA) {
            // 没数据？检查流是否已结束
            if (sbuffer_is_stream_ended(buffer)) {
                printf("[StorageMgr] No more data to consume. Exiting...\n");
                break;
            }
            continue;
        }

        if (result == SBUFFER_SUCCESS) {
            // 如果 id=0 代表结束标志，就 break
            if (data.id == 0) {
                printf("[StorageMgr] Termination signal received. Exiting...\n");
                break;
            }

            // 否则插入到 CSV
            if (insert_sensor(data.id, data.value, data.ts) != 0) {
                fprintf(stderr, "[StorageMgr] Error: Failed to insert sensor data.\n");
            }
        } else {
            fprintf(stderr, "[StorageMgr] Unexpected error in sbuffer_remove.\n");
            break;
        }
    }

    if (close_db(fp) != 0) {
        fprintf(stderr, "[StorageMgr] Error: Failed to close database file.\n");
    } else {
        printf("[StorageMgr] Database file successfully closed.\n");
    }

    pthread_exit(NULL);
}
