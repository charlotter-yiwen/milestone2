#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/wait.h>
#include "string.h"
#include "pthread.h"
#include "sbuffer.h"
#include "connmgr.h"
#include "datamgr.h"
#include "sensor_db.h"
#include "lib/tcpsock.h"

#define SIZE 25
#define READ_END 0
#define WRITE_END 1

int MAX_CONN;
int PORT;
pid_t pid;
int number_of_client;
// todo there should be only one buffer in the end
sbuffer_t *buffer;
int fd[2];
pthread_mutex_t mutex_logger;
pthread_mutex_t mutex_reader;


void initialize(char *argv[]);

//log process
int create_log_process(){
    FILE *fp = fopen("gateway.log","w");
    char msg[SIZE];

    int sequence = 0;
    uint16_t len;
    while(1){
        read(fd[READ_END], &len, sizeof(len));
        char *buffer = malloc(len + 1);
        read(fd[READ_END], buffer, len);
        buffer[len] = '\0';
        if(strncmp(buffer, "break\0", len+1) == 0){
            break;
        }
        char write_msg[100];
        char formatted_time[50];
        time_t current = time(NULL);
        struct tm *time = localtime(&current);
        strftime(formatted_time, sizeof(formatted_time), "%a %b %d %H:%M:%S %Y", time);
        snprintf(write_msg,sizeof(write_msg),"%d - %s - %s\n",sequence,formatted_time,buffer);

        fprintf(fp,"%s",write_msg);
        fflush(fp);

        sequence++;
        memset(msg, 0, sizeof(msg));
    }

    close(fd[READ_END]);
    fclose(fp);
    exit(0);
}

int write_to_log_process(char *msg){
    pthread_mutex_lock(&mutex_logger);
    uint16_t len = strlen(msg);
    write(fd[WRITE_END], &len, sizeof(len));
    write(fd[WRITE_END], msg, len);
    pthread_mutex_unlock(&mutex_logger);
    return 0;
}

int end_log_process(){
    close(fd[WRITE_END]);
    waitpid(pid,NULL,0);
    return 0;
}

//main process
int main(int argc, char *argv[]){
    if(argc < 3) {
        printf("Please provide the right arguments: first the port, then the max nb of clients");
        return -1;
    }

    if(pipe(fd) == -1){
        printf("Pipe failed\n");
        return -1;
    }

    // fork logger process
    pid = fork();
    if(pid < 0){
        // fork failed
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
    else if(pid == 0){
        // create logger process
        create_log_process();
    }
    else{
        close(fd[READ_END]);
        // initialize
        initialize(argv);

        // create connection manager thread, data manager thread, storage manager thread
        pthread_t pthread_connmgr;
        pthread_t pthread_datamgr;
        pthread_t pthread_storagemgr;
        pthread_create(&pthread_connmgr, NULL, create_connmgr, NULL);
        pthread_create(&pthread_datamgr, NULL, create_datamgr, NULL);
        pthread_create(&pthread_storagemgr, NULL, create_storagemgr, NULL);

        // wait for join
        pthread_join(pthread_connmgr, NULL);
        pthread_join(pthread_datamgr, NULL);
        pthread_join(pthread_storagemgr, NULL);

        // free and close
        sbuffer_free(&buffer);
        pthread_mutex_destroy(&mutex_logger);
        pthread_mutex_destroy(&mutex_reader);
        end_log_process();
    }
}

void initialize(char *argv[]){
    PORT = atoi(argv[1]);
    MAX_CONN = atoi(argv[2]);
    number_of_client = 0;
    sbuffer_init(&buffer);
    pthread_mutex_init(&mutex_logger, NULL);
    pthread_mutex_init(&mutex_reader, NULL);
}