#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "sbuffer.h"
#include "connmgr.h"
#include "datamgr.h"
#include "sensor_db.h"
#include "wait.h"

#define SIZE 50
#define READ_END 0
#define WRITE_END 1

typedef struct File File;

int MAX_CONN;
int number_of_client;
int PORT;
pid_t pid;
sbuffer_t *bufferStoragemgr;
int fd[2];
pthread_mutex_t mutex_logger;
pthread_mutex_t mutex_reader;
FILE *fp;
int sequence =0;

// Function declarations

int create_log_process();
void initial_Buffer(char *argv[]);
int write_to_log_process(char *msg);
int end_log_process();

int main(int argc,char *argv[]) {
    if(argc < 3) {
        printf("Please provide the right arguments: First the port number, then the max number of clients");
        return -1;
    }

    if(pipe(fd) == -1){
        printf("Pipe creating failed\n");
        return -1;
    }

    // fork logger process
    pid = fork();
    if(pid < 0){
        // fork failed
        perror("Fork creating failed");
        exit(EXIT_FAILURE);
    }
    else if(pid == 0){
        //Create log
        create_log_process();
    }
    else{
        close(fd[READ_END]);
        //Initial buffer
        initial_Buffer(argv);

        //Create three threads
        pthread_t pthread_connmgr;
        pthread_t pthread_datamgr;
        pthread_t pthread_storagemgr;
        pthread_create(&pthread_connmgr, NULL, create_connmgr, NULL);
        pthread_create(&pthread_datamgr, NULL, create_datamgr, NULL);
        pthread_create(&pthread_storagemgr, NULL, create_sensormgr, NULL);

        //Join thread
        pthread_join(pthread_connmgr, NULL);
        pthread_join(pthread_datamgr, NULL);
        pthread_join(pthread_storagemgr, NULL);

        //Free thread
        sbuffer_free(&bufferStoragemgr);
        pthread_mutex_destroy(&mutex_reader);
        pthread_mutex_destroy(&mutex_logger);
        //end
        end_log_process();
    }
}

void initial_Buffer(char *argv[]){
    PORT = atoi(argv[1]);
    MAX_CONN = atoi(argv[2]);
    number_of_client = 0;
    sbuffer_init(&bufferStoragemgr);
    pthread_mutex_init(&mutex_logger, NULL);
    pthread_mutex_init(&mutex_reader, NULL);
}

int create_log_process(){
    fp = fopen("gateway.log","w");
    char msg[SIZE];

    sequence = 0;
    uint16_t len;
    while(1){
        read(fd[READ_END], &len, sizeof(len));
        char *buffer = malloc(len + 1);
        read(fd[READ_END], buffer, len);
        buffer[len] = '\0';
        if(strncmp(buffer, "break\0", len+1) == 0){
            break;
        }
        char write_msg[120];
        char formatted_time[60];
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
