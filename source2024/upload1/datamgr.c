#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>
#include "datamgr.h"
#include "config.h"
#include "lib/dplist.h"
#include "sbuffer.h"
#include <unistd.h>
#include <time.h>
// 如果系统没有声明 nanosleep，手动声明
extern int nanosleep(const struct timespec *req, struct timespec *rem);
extern int write_to_log_process(char *msg);


// 定义元素结构
typedef struct {
    sensor_id_t sensorID;
    uint16_t roomID;
    double running_avg;
    time_t last_modified;
    double sublist[RUN_AVG_LENGTH];
    int next;
} element_t;

#define SET_MIN_TEMP 10
#define SET_MAX_TEMP 20

// 全局变量
extern sbuffer_t *buffer;
extern pthread_mutex_t mutex_reader;
dplist_t *list_map = NULL;

// 函数声明
void *element_copy(void *element);
void element_free(void **element);
int element_compare(void *x, void *y);
int isInvalid(sensor_data_t *data);

// 链表辅助函数
void *element_copy(void *element) {
    element_t *data = malloc(sizeof(element_t));
    *data = *(element_t *)element;
    return data;
}

void element_free(void **element) {
    free(*element);
    *element = NULL;
}

int element_compare(void *x, void *y) {
    return ((element_t *)x)->sensorID - ((element_t *)y)->sensorID;
}

// 检查传感器ID是否无效
int isInvalid(sensor_data_t *data) {
    for (int i = 0; i < dpl_size(list_map); i++) {
        if (data->id == ((element_t *)dpl_get_element_at_index(list_map, i))->sensorID) {
            return 0;
        }
    }
    return 1;
}

// 初始化房间-传感器映射
void init(FILE *fp) {
    datamgr_free();
    list_map = dpl_create(element_copy, element_free, element_compare);

    char line[256]; // 存储每行输入
    while (fgets(line, sizeof(line), fp)) {
        char *endptr;
        uint16_t roomID, sensorID;

        // 解析 roomID
        roomID = (uint16_t)strtoul(line, &endptr, 10);
        if (endptr == line || *endptr != ' ') {
            fprintf(stderr, "Error parsing roomID in line: %s\n", line);
            continue;
        }

        // 跳过空格
        endptr++;

        // 解析 sensorID
        sensorID = (uint16_t)strtoul(endptr, &endptr, 10);
        if (endptr == line || (*endptr != '\0' && *endptr != '\n')) {
            fprintf(stderr, "Error parsing sensorID in line: %s\n", line);
            continue;
        }

        // 构造元素并插入到链表
        element_t data_map = {
            .sensorID = sensorID,
            .roomID = roomID,
            .running_avg = 0,
            .last_modified = 0,
            .next = 0
        };
        memset(data_map.sublist, 0, sizeof(data_map.sublist));
        dpl_insert_at_index(list_map, &data_map, dpl_size(list_map), true);
    }
    fclose(fp);
}

// 从缓冲区处理数据
void datamgr_process_data(sensor_data_t *data) {

    if (isInvalid(data)) {
        char msg[128];
        snprintf(msg, sizeof(msg),
                 "Received sensor data with invalid sensor node ID %d",
                 data->id);
        write_to_log_process(msg);
        return;
    }


    for (int i = 0; i < dpl_size(list_map); i++) {
        element_t *element = dpl_get_element_at_index(list_map, i);
        if (data->id == element->sensorID) {
            element->sublist[element->next % RUN_AVG_LENGTH] = data->value;
            element->last_modified = data->ts;
            element->next++;

            // 打印每条数据
            printf("Room %u (Sensor %u): Temperature %.2f, Timestamp %ld\n",
                   element->roomID, element->sensorID, data->value, data->ts);

            if (element->next >= RUN_AVG_LENGTH) {
                double sum = 0;
                for (int j = 0; j < RUN_AVG_LENGTH; j++) {
                    sum += element->sublist[j];
                }
                element->running_avg = sum / RUN_AVG_LENGTH;

                // 检查是否超过阈值
                if (element->running_avg > SET_MAX_TEMP) {
                    char msg[128];
                    snprintf(msg, sizeof(msg),
                             "Sensor node %u reports it's too hot (avg temp = %.2f)",
                             element->sensorID, element->running_avg);
                    write_to_log_process(msg);  // 改成往日志进程发
                } else if (element->running_avg < SET_MIN_TEMP) {
                    char msg[128];
                    snprintf(msg, sizeof(msg),
                             "Sensor node %u reports it's too cold (avg temp = %.2f)",
                             element->sensorID, element->running_avg);
                    write_to_log_process(msg);
                }

            }
            break;
        }
    }
}

// 释放 datamgr 资源
void datamgr_free() {
    if (list_map) {
        dpl_free(&list_map, true);
        list_map = NULL;
    }
}

// 创建 datamgr 线程
void *create_datamgr(void *ptr) {
    FILE *fp = fopen("room_sensor.map", "r");
    if (!fp) {
        fprintf(stderr, "Error: Could not open room_sensor.map\n");
        pthread_exit(NULL);
    }
    init(fp);

    sensor_data_t data;
    int result;

    while (true) {
        pthread_mutex_lock(&mutex_reader);
        result = sbuffer_remove(buffer, &data);
        pthread_mutex_unlock(&mutex_reader);

        if (result == SBUFFER_NO_DATA) {
            struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000 }; // 100 ms
            nanosleep(&ts, NULL);
            continue;
        } else if (result == SBUFFER_FAILURE || data.id == 0) {
            break;
        }

        datamgr_process_data(&data);
    }

    datamgr_free();
    pthread_exit(NULL);
}

