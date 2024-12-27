#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/wait.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include "sbuffer.h"
#include "connmgr.h"
#include "datamgr.h"
#include "sensor_db.h"
#include "lib/tcpsock.h"
#include <pthread.h>

// 定义全局变量
pthread_mutex_t buffer_mutex;


#define READ_END 0
#define WRITE_END 1

typedef struct {
    int port;
    int max_connections;
    sbuffer_t *buffer;
} connmgr_args_t;

int MAX_CONN;
int PORT;
pid_t pid;
sbuffer_t *buffer;
int fd[2]; // Pipe for logger
pthread_mutex_t mutex_logger;
pthread_mutex_t mutex_reader;

// Logger Process
int create_log_process() {
    FILE *fp = fopen("gateway.log", "w");
    if (!fp) {
        perror("Failed to open gateway.log");
        return -1;
    }

    int sequence = 0;
    uint16_t len;
    while (1) {
        char *buffer = NULL;
        read(fd[READ_END], &len, sizeof(len));
        buffer = malloc(len + 1);
        if (!buffer) {
            perror("Failed to allocate memory");
            break;
        }
        read(fd[READ_END], buffer, len);
        buffer[len] = '\0';

        if (strcmp(buffer, "break") == 0) {
            free(buffer);
            break;
        }

        char write_msg[100];
        char formatted_time[50];
        time_t current = time(NULL);
        struct tm *time = localtime(&current);
        strftime(formatted_time, sizeof(formatted_time), "%a %b %d %H:%M:%S %Y", time);
        snprintf(write_msg, sizeof(write_msg), "%d - %s - %s\n", sequence, formatted_time, buffer);

        fprintf(fp, "%s", write_msg);
        fflush(fp);

        free(buffer);
        sequence++;
    }

    close(fd[READ_END]);
    fclose(fp);
    exit(0);
}

// Write to Logger Process
int write_to_log_process(char *msg) {
    pthread_mutex_lock(&mutex_logger);
    uint16_t len = strlen(msg);
    write(fd[WRITE_END], &len, sizeof(len));
    write(fd[WRITE_END], msg, len);
    pthread_mutex_unlock(&mutex_logger);
    return 0;
}

// End Logger Process
int end_log_process() {
    close(fd[WRITE_END]);
    waitpid(pid, NULL, 0);
    return 0;
}

// Main Initialization
void initialize(char *argv[]) {
    char *endptr;
    PORT = strtol(argv[1], &endptr, 10);
    if (*endptr != '\0' || PORT <= 0 || PORT > 65535) {
        fprintf(stderr, "Error: Invalid port number '%s'\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    MAX_CONN = strtol(argv[2], &endptr, 10);
    if (*endptr != '\0' || MAX_CONN <= 0) {
        fprintf(stderr, "Error: Invalid number of connections '%s'\n", argv[2]);
        exit(EXIT_FAILURE);
    }

    if (sbuffer_init(&buffer) != SBUFFER_SUCCESS) {
        fprintf(stderr, "Error: Failed to initialize buffer.\n");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_init(&mutex_logger, NULL);
    pthread_mutex_init(&mutex_reader, NULL);
}

// Main Function
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <PORT> <MAX_CONN>\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (pipe(fd) == -1) {
        perror("Pipe failed");
        return EXIT_FAILURE;
    }

    pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        create_log_process();
    } else {
        close(fd[READ_END]);

        initialize(argv);

        pthread_t pthread_connmgr, pthread_datamgr, pthread_storagemgr;

        // Prepare arguments for connmgr_listen
        connmgr_args_t args;
        args.port = PORT;
        args.max_connections = MAX_CONN;
        args.buffer = buffer;

        // Start connection manager thread
        if (pthread_create(&pthread_connmgr, NULL, (void *(*)(void *))connmgr_listen, &args) != 0) {
            fprintf(stderr, "Error: Failed to create connmgr thread.\n");
            goto cleanup;
        }

        // Start data manager thread
        if (pthread_create(&pthread_datamgr, NULL, create_datamgr, buffer) != 0) {
            fprintf(stderr, "Error: Failed to create datamgr thread.\n");
            pthread_cancel(pthread_connmgr);
            goto cleanup;
        }

        // Start storage manager thread
        if (pthread_create(&pthread_storagemgr, NULL, create_storagemgr, buffer) != 0) {
            fprintf(stderr, "Error: Failed to create storagemgr thread.\n");
            pthread_cancel(pthread_connmgr);
            pthread_cancel(pthread_datamgr);
            goto cleanup;
        }

        // Wait for threads to complete
        pthread_join(pthread_connmgr, NULL);
        pthread_join(pthread_datamgr, NULL);
        pthread_join(pthread_storagemgr, NULL);

    cleanup:
        sbuffer_free(&buffer);
        pthread_mutex_destroy(&mutex_logger);
        pthread_mutex_destroy(&mutex_reader);
        end_log_process();
    }

    return EXIT_SUCCESS;
}