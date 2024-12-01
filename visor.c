#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include "common.c"

#define FIFO_PREFIX "/tmp/myfifo"

void *visorThread( void * ); 

int fifo_fd;

int main() {
    printf("*********************************************\n");
    printf("****************VIDEO VISOR******************\n");
    printf("*********************************************\n");
    char ip_address[16];
    printf("Enter server IP address (or localhost): ");
    fgets(ip_address, sizeof(ip_address), stdin);
    ip_address[strcspn(ip_address, "\n")] = 0;
    srand(time(NULL));
    char random_number[17];
    for (int i = 0; i < 16; i++) {
        random_number[i] = '0' + (rand() % 10);
    }
    random_number[16] = '\0';
    char fifo_path[28] = FIFO_PREFIX;
    strcat(fifo_path, random_number);
    mkfifo(fifo_path, 0666);
    if(fork() == 0){
        char to_exec[256];
        snprintf(to_exec, sizeof(to_exec), "./controls %s", fifo_path);
        char *argv[] = {
            "gnome-terminal",
            "--",
            "bash",
            "-c",
            to_exec,
            NULL
        };
        execvp(argv[0], argv);
    }
    fifo_fd = open(fifo_path, O_RDONLY);
    int connfd = -1;
    int command_size = 2;
    char command[command_size + 1];
    while(true){
        read(fifo_fd, command, command_size*sizeof(char));
        command[command_size] = '\0';
        if(connfd > -1){
            if (strcmp(command, "LD") == 0) { 
                write(connfd, "LD", command_size);
            }
            else if (strcmp(command, "MD") == 0) {
                write(connfd, "MD", command_size);
            }
            else if (strcmp(command, "HD") == 0) {
                write(connfd, "HD", command_size);
            }
        }
        else if(strcmp(command, "ST") == 0) {
            char *hostname = ip_address;
            char *port = "8080";
            connfd = open_clientfd(hostname, port);
            if(connfd < 0)
                connection_error(connfd);
            pthread_t tid;
            int *connfd_ptr = (int *)malloc(sizeof(int));
            pthread_create(&tid, NULL, visorThread, (void *)&connfd);
        }
        if(strcmp(command, "QT") == 0 && connfd != -1){
            write(connfd, "QT", command_size);
            close(connfd);
            printf("Video stopped!\n");
            printf("Disconnecting from streamer...\n");
            break;
        }
    }
    close(fifo_fd);
    return 0;
}

void *visorThread(void *arg){
    int connfd = *(int *)arg;
    pthread_detach(pthread_self());
    printf("Video starting...\n");
    int received[16];
    ssize_t bytes_read;
    while((bytes_read = read(connfd, received, 16*sizeof(int))) > 0){
        for(int i = 0; i < 16; i++){
            if(received[i] == -1){
                printf("Video finished!\n");
                write(connfd, "QT", 2);
                break;
            }
            printf("%d\n", received[i]);
            usleep(1000*10); /*simulates frame pacing*/
        }
    }
    printf("Disconnecting from streamer...\n");
    close(fifo_fd);
    exit(0);
}