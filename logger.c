#include "logger.h"
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

#define LOG_FILE "gateway.log"

static int pipe_fd[2];
static pid_t logger_pid = 0;
static int sequence_number = 0; // 序列号

void format_timestamp(char *buffer, size_t buffer_size) {
    time_t now = time(NULL);
    struct tm *local_time = localtime(&now);
    strftime(buffer, buffer_size, "%a %b %d %H:%M:%S %Y", local_time);
}

int create_log_process() {
    if (logger_pid != 0) {
        return 0; // 子进程已存在
    }

    if (pipe(pipe_fd) == -1) {
        perror("Pipe creation failed");
        return -1;
    }

    logger_pid = fork();
    if (logger_pid == -1) {
        perror("Fork failed");
        return -1;
    }

    if (logger_pid == 0) {
        // 子进程代码
        close(pipe_fd[1]); // 关闭写端
        FILE *log_file = fopen(LOG_FILE, "a");
        if (log_file == NULL) {
            perror("Failed to open log file");
            exit(EXIT_FAILURE);
        }

        char buffer[256];
        while (1) {
            ssize_t bytes_read = read(pipe_fd[0], buffer, sizeof(buffer) - 1);
            if (bytes_read > 0) {
                buffer[bytes_read] = '\0';
                fprintf(log_file, "%s\n", buffer);
                fflush(log_file);
            } else if (bytes_read == 0) {
                break; // 管道关闭
            } else {
                perror("Failed to read from pipe");
                break;
            }
        }

        fclose(log_file);
        close(pipe_fd[0]);
        exit(0);
    }

    close(pipe_fd[0]); // 父进程关闭读端
    return 0;
}

int write_to_log_process(char *msg) {
    if (logger_pid <= 0) {
        if (create_log_process() == -1) {
            fprintf(stderr, "Failed to start logger process\n");
            return -1;
        }
    }

    if (pipe_fd[1] <= 0) {
        fprintf(stderr, "Pipe write end is not valid\n");
        return -1;
    }

    char log_msg[256];
    char timestamp[64];
    format_timestamp(timestamp, sizeof(timestamp));
    snprintf(log_msg, sizeof(log_msg), "%d - %s - %s", sequence_number, timestamp, msg);

    if (write(pipe_fd[1], log_msg, strlen(log_msg) + 1) == -1) {
        perror("Failed to write to pipe");
        return -1;
    }

    fsync(pipe_fd[1]); // 确保消息被立即写入管道
    usleep(1000);      // 添加 1 毫秒延迟

    sequence_number++; // 确保序列号递增
    return 0;
}

void reset_sequence_number() {
    sequence_number = 0;
}

int end_log_process() {
    if (logger_pid > 0) {
        close(pipe_fd[1]); // 关闭写端
        waitpid(logger_pid, NULL, 0); // 等待子进程退出
        logger_pid = 0; // 清理子进程 ID
    }
    return 0;
}
