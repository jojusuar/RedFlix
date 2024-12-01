#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ncurses.h>
#include <pthread.h>
#include <fcntl.h>

#define FIFO_PATH "/tmp/myfifo"

int fifo_fd;

int main(int argc, char *argv[]){
    initscr();
    noecho();
    cbreak();
    refresh();
    printw("*********VIDEO CONTROLS***********\n");
    fifo_fd = open(argv[1], O_WRONLY);
    int msg_size = 2;
    int ch;
    printw("First press Shift+S to start the video!\n\n");
    printw("Afterwards, you can change bitrate with:\n");
    printw("Shift+L for Low Definition\n");
    printw("Shift+M for Medium Definition\n");
    printw("Shift+H for High Definition\n\n");
    printw("You can end the streaming at any time by pressing Shift+Q\n");
    while ((ch = getch()) != 'Q') {
        if (ch == 'L') { 
            printw("Changing to Low Definition...\n");
            write(fifo_fd, "LD", msg_size);
            refresh();
        }
        else if (ch == 'M') {
            printw("Changing to Medium Definition...\n");
            write(fifo_fd, "MD", msg_size);
            refresh();
        }
        else if (ch == 'H') {
            printw("Changing to High Definition...\n");
            write(fifo_fd, "HD", msg_size);
            refresh();
        }
        else if(ch == 'S') {
            write(fifo_fd, "ST", msg_size);
            printw("Starting video stream...\n");
            refresh();
        }
    }
    write(fifo_fd, "QT", msg_size);
    close(fifo_fd);
    endwin();
}