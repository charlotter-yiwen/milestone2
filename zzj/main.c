//
// Created by marg_zhao on 10/12/23.
//
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include "connmgr.h"
#include "datamgr.h"
#include "sensor_db.h"

//globals
int pro_pipe[2];
sbuffer_t* buf; //buffer is here
int MAX_CONN;
int PORT;
static FILE *log_file = NULL;

//pthread_mutex_t bufalock;
//runners
void* conn_runner(void *arg);
void* data_runner(void *arg);
void* strg_runner(void *arg);

int write_into_log_pipe(char *msg){
    if (write(pro_pipe[1], msg, strlen(msg)+1) == -1) {
        perror("Error writing to log process");
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[]){
    sbuffer_init(&buf);
    if (argc < 3) {
        printf("Please provide the right arguments: first the port, then the max nb of clients");
        return -1;
    }

    MAX_CONN = atoi(argv[2]);
    PORT = atoi(argv[1]);
    if (pipe(pro_pipe) == -1) {
        perror("Error creating log pipe");
        return -1;
    }
    pid_t pid;
    pid = fork();
    if(pid<0){
        perror("Error fork");
        return -1;
    }
    if(pid==0){
        // child: log process
        //TODO: overwrite problem
        log_file = fopen("data.csv", "r");
        if(log_file==NULL)
            log_file = fopen("gateway.log", "a");
        else
            log_file = fopen("gateway.log","w");
        if (log_file == NULL) {
            perror("Error opening log file");
            exit(EXIT_FAILURE);
        }
        close(pro_pipe[1]);
        char log_message[512] ;
        char msg[256];
        int sequence_number = 0;
        while ((read(pro_pipe[0], msg, sizeof(msg))) > 0){
            printf("from parent : %s \n", msg);
            time_t current = time(NULL);
            struct tm *local = localtime( &current);
            char timestamp[30];
            strftime(  timestamp,sizeof(timestamp), "%a %b %d %H :%M:%S %Y", local);
            // Format log message
            snprintf(log_message, sizeof(log_message), "%d - %s - %s", sequence_number++, timestamp, msg);
            // Write log_message to the log file
            fprintf(log_file, "%s\n", log_message);
            //fflush(log_file);  // Flush to ensure immediate writing
            //sleep(1);
            // For now, let's just print the log messages
            //printf("%s", log_message);
        }
        close(pro_pipe[0]);
        fclose(log_file);

        exit(EXIT_SUCCESS);
        //@TODO: end log, write, main process operation
    }
    else{
        // main process
        close(pro_pipe[0]);//for  what
        pthread_t connmgr;
        pthread_t datamgr;
        pthread_t strgmgr;
        pthread_create(&connmgr,NULL,conn_runner,NULL);
        pthread_create(&datamgr,NULL,data_runner,NULL);
        pthread_create(&strgmgr,NULL,strg_runner,NULL);

        pthread_join(connmgr, NULL);
        pthread_join(datamgr, NULL);
        pthread_join(strgmgr, NULL);
        close(pro_pipe[1]);// end log process
        printf("end log process\n");
        wait(NULL);// wait till child end
        //TODO:destroy and free
        sbuffer_free(&buf);
        printf("The end\n");
    }

    return 0;
}

void* conn_runner(void *arg){
    connmgr_run(PORT,MAX_CONN,buf);
    pthread_exit(NULL);
    return NULL;
}

void* data_runner(void *arg){
    //TODO:
    FILE * map = fopen("room_sensor.map", "r");
    datamgr_parse_sensor_files(map,buf);
    datamgr_free();
    fclose(map);
    pthread_exit(NULL);
    return NULL;
}

void* strg_runner(void *arg){
    //TODO:
    FILE *strg = fopen("data.csv", "r");
    if(strg==NULL)
        strg = open_db("data.csv",true);
    else
        strg = open_db("data.csv",false);
    sbuffer_node_t *node;
    sensor_data_t data;

    while(buf->head==NULL){
        //wait for the first data
    }
    pthread_mutex_lock(&buf->mutex);
    while(buf->head!=NULL){

        node = buf->head;
        pthread_mutex_unlock(&buf->mutex);
        if(node->strg_read==0){
            data = node->data;
            insert_sensor(strg,data.id,data.value,data.ts);
        }
        sbuffer_remove(buf,&data,0);//to remove the head if both thread read
        //node always pointed to head bc both reader read and remove head
        sleep(1);
        pthread_mutex_lock(&buf->mutex);
    }
    pthread_mutex_unlock(&buf->mutex);

    close_db(strg);
    pthread_exit(NULL);
    return NULL;
}