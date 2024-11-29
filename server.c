#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include "common.h"

#define READ_SIZE 4096 //reads this amount of bytes at a time from the txt file
#define STREAMER_BUFFER_SIZE 256 // max number of values that can be parsed before the streamer starts sending blocks
#define OUTPUT_BLOCK_SIZE 32 // number of values in each block streamed to the client

typedef struct {
    int fd;
    off_t offset;
    size_t size;
    int thread_id;
} ThreadData;

typedef struct {
    int *buffer;
    sem_t mutex;
    sem_t full;
    sem_t empty;
} StreamerBuffer;

void *workerThread( void * );
void *streamer( void * );

int video_fd;

int main(int argc, char* argv[]){
    video_fd = open("video.txt", O_RDONLY);

    // int listenfd;
    // unsigned int clientlen;
    // struct sockaddr_in clientaddr;
    // char *port = "8080";
    // listenfd = open_listenfd(port);

    // if (listenfd < 0){
	// 	connection_error(listenfd);
    // }
    // pthread_t *tid = NULL;
    // while(true){
    //     clientlen = sizeof(clientaddr);
    //     int *connfd = (int *)malloc(sizeof(int));
	// 	*connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
    //     pthread_create(tid, NULL, workerThread, (void *)connfd);
    // }

    int *connfd = (int *)malloc(sizeof(int));
    pthread_t tid;
    pthread_create(&tid, NULL, workerThread, (void *)connfd);
    while(true);
}

void *workerThread(void *arg){
    int connfd = *((int *)arg);
	free(arg);
    pthread_detach(pthread_self());
    ThreadData thread_data;
    thread_data.fd = video_fd;
    thread_data.offset = 0;
    thread_data.size = READ_SIZE;
    thread_data.thread_id = pthread_self();
    
    StreamerBuffer *streamer_buffer = (StreamerBuffer *)malloc(sizeof(StreamerBuffer));
    streamer_buffer->buffer = (int *)malloc(STREAMER_BUFFER_SIZE*sizeof(int));
    sem_init(&(streamer_buffer->mutex), 0, 1);
    sem_init(&(streamer_buffer->empty), 0, STREAMER_BUFFER_SIZE);
    sem_init(&(streamer_buffer->full), 0, 0);

    pthread_t streamer_tid;
    pthread_create(&streamer_tid, NULL, streamer, (void *)streamer_buffer);

    char *bitrate = "LD";

    char *read_buffer = (char *)malloc(thread_data.size + 1);

    int i = 0;
    int counter = 0;
    bool add_current_number = true;
    int previous = -1;
    int piece = -1;
    ssize_t bytes_read;

    while ((bytes_read = pread(thread_data.fd, read_buffer, thread_data.size, thread_data.offset)) > 0) {
        thread_data.offset += bytes_read;
        read_buffer[bytes_read] = '\0';
        if (bytes_read < 0) {
            perror("pread");
            free(read_buffer);
            pthread_exit(NULL);
        }
       
        char *number_string = strtok(read_buffer, ",");
        while(number_string){
            int number = atoi(number_string);
            if(number < previous && piece == -1){  /* Es muy probable que el número de bytes leídos
            no encaje con los separadores entre los números, por lo que cada lectura,
            un número podría ser truncado. La única manera de que un número
            sea menor al anterior es que haya sido truncado, asi que lo almacenamos 
            para juntarlo con el siguiente*/ 
                piece = number;
            }
            else{
                if(piece != -1){
                    char *whole = (char *)malloc(6*sizeof(char)); //un numero hasta 99'999 tiene hasta 5 cifras, mas 1 de null terminator
                    sprintf(whole, "%d%d", piece, number);
                    number = atoi(whole);
                    free(whole);
                    piece = -1;
                }

                if(strcmp(bitrate, "MD") == 0){
                    if(counter == 0) add_current_number = true;
                    else add_current_number = false;
                    counter = (counter + 1)%10;
                }

                else if(strcmp(bitrate, "LD") == 0){
                    if(counter == 0) add_current_number = true;
                    else add_current_number = false;
                    counter = (counter + 1)%100;
                }

                if(add_current_number){
                    sem_wait(&(streamer_buffer->empty));
                    sem_wait(&(streamer_buffer->mutex));
                    
                    streamer_buffer->buffer[i] = number;

                    sem_post(&(streamer_buffer->mutex));
                    sem_post(&(streamer_buffer->full));
                    i = (i + 1)%STREAMER_BUFFER_SIZE;
                }
            }
            number_string = strtok(NULL, ",");
            previous = number;
        }
    }
    sem_wait(&(streamer_buffer->empty));
    sem_wait(&(streamer_buffer->mutex));
    
    streamer_buffer->buffer[i] = -1; //indica el fin del video

    sem_post(&(streamer_buffer->mutex));
    sem_post(&(streamer_buffer->full));
}

void *streamer(void *arg){
    StreamerBuffer *streamer_buffer = (StreamerBuffer *)arg;
    pthread_detach(pthread_self());
    int i = 0;
    int j = 0;
    int to_send[OUTPUT_BLOCK_SIZE];
    bool eof = false;
    do {
        sem_wait(&(streamer_buffer->full));
        sem_wait(&(streamer_buffer->mutex));
        
        to_send[i] = streamer_buffer->buffer[j];
        if(to_send[i] == -1){
            eof = true; 
        }

        sem_post(&(streamer_buffer->mutex));
        sem_post(&(streamer_buffer->empty));

        i = (i + 1)%OUTPUT_BLOCK_SIZE;
        j = (j + 1)%STREAMER_BUFFER_SIZE;
        
        if(i == 0 || eof){
            printf("NUEVO BLOQUE: ");
            for(int index = 0; index < OUTPUT_BLOCK_SIZE; index++){
                printf("%d\n", to_send[index]);
                usleep(1000);
                if(to_send[index] == -1) {
                    printf("fin del video!\n");
                    break;
                }
            }
        }
    } while (true);
    return NULL;
}