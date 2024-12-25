#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>

#include "connmgr.h"
#include "sbuffer.h"
#include "sensor_db.h"
#include "datamgr.h"


// Global variables
pthread_mutex_t buffer_lock;  // Mutex for synchronizing access to buffer
pthread_cond_t empty_buffer; // Buffer empty_buffer
sbuffer_t *buffer; // Shared buffer
char pipe_buffer[500000000]; // Shared pipe between child and parent
int p[2]; // Pipe
int seqNum = 1; // Sequence number in log file
int port; // Port number parsed from command line arguments given to connection manager

// Function prototypes
void *connection_manager_func(void *arg);
void *data_manager_func(void *arg);
void *storage_manager_func(void *arg);

int main(int argc, char *argv[]) {

    // Parse port number from command line arguments
    if (argc != 2) {
        fprintf(stderr, "Usage: %s port\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    port = atoi(argv[1]);

  // Initialize buffer & mutex
  int a = sbuffer_init(&buffer, buffer_lock);
    if (a != SBUFFER_SUCCESS){printf("buffer init failed\n"); return 1;}

  pthread_t connection_manager;
  pthread_t data_manager;
  pthread_t storage_manager;

  
  // Check pipe for child & parent process
  if (pipe(p)<0){
        exit(1);
        printf("failed to create pipe");}

  // Create child process
  pid_t pid = fork();

  if (pid < 0){
        exit(2);
        printf("failed to fork");}


// WE ARE IN CHILD PROCESS
  if (pid == 0) {
    FILE *gateway_log = fopen("gateway_log", "w");  // Open log file for writing
    if (gateway_log == NULL) {
      perror("Error opening log file");
      exit(1);
    }

    // Read from pipe
    
    while((read(p[0], pipe_buffer, sizeof(pipe_buffer))) > 0){
      close(p[1]); // Close writing end of pipe
      //get only one log event at a time (dilemeter is new line)
      char *event = strtok(pipe_buffer, "\n");

      while (event != NULL){
        //get current time as string
        time_t mytime = time(NULL);
        char * time_str = ctime(&mytime);
        time_str[strlen(time_str)-1] = '\0';
    // Write into log file
        fprintf(gateway_log,"<%d> <%s> <%s>\n",seqNum, time_str, event);
        seqNum++;
        event = strtok(NULL, "\n"); // event = next string
        }
      close(p[0]); //close reading pipe when done
    }
    
    fclose(gateway_log); // close log file when done
    exit(3);

  }
// WE ARE IN PARENT PROCESS
  else {
    // Create threads
    pthread_create(&connection_manager, NULL, connection_manager_func, NULL);
    pthread_create(&data_manager, NULL, data_manager_func, NULL);
    pthread_create(&storage_manager, NULL, storage_manager_func, NULL);

    // Wait for threads to finish
    pthread_join(connection_manager, NULL);
    pthread_join(data_manager, NULL);
    pthread_join(storage_manager, NULL);

    pthread_mutex_destroy(&buffer_lock);
    pthread_cond_destroy(&empty_buffer);
    sbuffer_free(&buffer);
    pthread_exit(NULL);
  }

  return 0;
}

void *connection_manager_func(void *arg) {
  // Start server & write to buffer
  connectionmgr_main(port, buffer, buffer_lock, empty_buffer);
  pthread_exit(NULL);
  return NULL;
}


void *data_manager_func(void *arg) {
  FILE *sensor_map = fopen("room_sensor.map", "r");
  datamgr_main(sensor_map, buffer, p, buffer_lock, empty_buffer);
  pthread_exit(NULL);
  return NULL;
}



void *storage_manager_func(void *arg) {
  pthread_mutex_lock(&buffer_lock);
  // Make and open csv file for write
  FILE *csv = open_db("csv", false);
  sbuffer_node_t *node;
  sensor_data_t data;

  while(1){
    if (buffer == NULL){
        pthread_mutex_unlock(&buffer_lock);
        return NULL;}
    node = buffer->head;
    if (node == NULL) {  // means buffer is empty
        while (node == NULL){
            pthread_cond_wait(&empty_buffer, &buffer_lock); // wait for data
            printf("\nwaiting for data\n");
        }
    }
    pthread_cond_signal(&empty_buffer);
    while (node != NULL){
      data = node->data;
        if (node->read_by_storagemgr==1){
            if (node->read_by_datamgr==1) sbuffer_remove(buffer, &data, buffer_lock); //remove node if read by all threads
        }
        else{
            insert_sensor(csv, data.id, data.value, data.ts);
            node->read_by_storagemgr = 1;
        }
      node = node->next;
    }
  }
  pthread_mutex_unlock(&buffer_lock);
  close_db(csv); // Close csv file
  pthread_exit(NULL);
  return NULL;
}

void write_into_log_pipe(char *msg){
    write(p[1], msg, strlen(msg));

}