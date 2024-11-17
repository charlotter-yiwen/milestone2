#include "sensor_db.h"
#include "logger.h"
#include <time.h>
#include <stdio.h>

FILE *open_db(char *filename, bool append) {
    FILE *f = fopen(filename, append ? "a" : "w");
    if (f == NULL) {
        write_to_log_process("Error: Failed to open database file");
        exit(EXIT_FAILURE);
    }

    reset_sequence_number(); // 每次打开新文件时重置序列号
    write_to_log_process("Data file opened."); // 记录日志
    return f;
}

int insert_sensor(FILE *f, sensor_id_t id, sensor_value_t value, sensor_ts_t ts) {
    if (f == NULL) {
        write_to_log_process("Error: Database file is not open");
        return -1;
    }

    ts = time(NULL); // 动态获取时间戳
    if (fprintf(f, "%d, %.6f, %ld\n", id, value, ts) < 0) {
        write_to_log_process("Error occurred when writing to the CSV file.");
        return -1;
    }
    fflush(f);

    write_to_log_process("Data inserted."); // 记录日志
    return 0;
}

int close_db(FILE *f) {
    if (f == NULL) {
        write_to_log_process("Error: Database file is not open");
        return -1;
    }

    fclose(f); // 关闭文件
    write_to_log_process("Data file closed."); // 记录日志
    return 0;
}
