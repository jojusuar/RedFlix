#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main() {
    int read_fd = open("/tmp/myfifo", O_RDONLY);
    int received[16];
    ssize_t bytes_read;
    while((bytes_read = read(read_fd, received, 16*sizeof(int))) > 0){
        for(int i = 0; i < 16; i++){
            if(received[i] == -1){
                printf("Video finished!\n");
                break;
            }
            printf("%d\n", received[i]);
            fflush(stdout);
            usleep(1000*10); /*simulates frame pacing*/
        }
    }
    close(read_fd);
    return 0;
}