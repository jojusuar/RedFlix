#include "common.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ncurses.h>
#include <pthread.h>
#include <fcntl.h>

#define MESSAGE "Low Definition"

void *feed_visor(void *);

int main(int argc, char *argv[]){
    initscr();
    noecho();
    cbreak();
    refresh();
    
    int connfd = -1;
    int msg_size = 2;
    int ch;
    printw("First press Shift+S to start the video!\n\n");
    printw("Afterwards, you can change bitrate with:\n");
    printw("Shift+L for Low Definition\n");
    printw("Shift+M for Medium Definition\n");
    printw("Shift+H for High Definition\n\n");
    printw("You can end the streaming at any time by pressing Shift+Q\n");
    while ((ch = getch()) != 'Q') {
        if(connfd != -1){
            if (ch == 'L') { 
                printw("Changing to Low Definition...\n");
                write(connfd, "LD", msg_size);
                refresh();
            }
            else if (ch == 'M') {
                printw("Changing to Medium Definition...\n");
                write(connfd, "MD", msg_size);
                refresh();
            }
            else if (ch == 'H') {
                printw("Changing to High Definition...\n");
                write(connfd, "HD", msg_size);
                refresh();
            }
        }
        else if(ch == 'S') {
            char *hostname = "localhost";
            char *port = "8080";
            connfd = open_clientfd(hostname, port);
            if(connfd < 0)
                connection_error(connfd);
            pthread_t tid;
            pthread_create(&tid, NULL, feed_visor, (void *)&connfd);
        }
    }
    endwin();
}

void *feed_visor(void *arg){
    int connfd = *(int *)arg;
    pthread_detach(pthread_self());
    const char *fifo_path = "/tmp/myfifo";
    mkfifo(fifo_path, 0666);
    if(fork() == 0){
        char *argv[] = {
            "gnome-terminal", "--", "bash", "-c", "./visor; exec bash", NULL
        };
        execvp(argv[0], argv);
    }
    int fifo_fd = open(fifo_path, O_WRONLY);
    int received[16];
    ssize_t bytes_read;
    while((bytes_read = read(connfd, received, 16*sizeof(int))) > 0){
        write(fifo_fd, received, 16*sizeof(int));
    }
    close(fifo_fd);
}