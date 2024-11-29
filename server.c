#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include "common.h"

#define READ_SIZE 4096 //reads this amount of bytes at a time from the txt file
#define STREAMER_BUFFER_SIZE 32 // max number of values that can be parsed before the streamer starts sending blocks
#define OUTPUT_BLOCK_SIZE 16 // number of values in each block streamed to the client

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
    int fd;
} StreamerBuffer;

typedef struct {
    StreamerBuffer *streamer_buffer;
    char *bitrate;
    sem_t bitrate_mutex;
} EncoderData;

void *workerThread( void * );
void *encoder( void * );
void *streamer( void * );

int video_fd;

int main(int argc, char* argv[]){
    video_fd = open("video.txt", O_RDONLY);

    int listenfd;
    unsigned int clientlen;
    struct sockaddr_in clientaddr;
    char *port = "8080";
    listenfd = open_listenfd(port);

    if (listenfd < 0){
		connection_error(listenfd);
    }
    pthread_t tid;
    while(true){
        clientlen = sizeof(clientaddr);
        int *connfd = (int *)malloc(sizeof(int));
		*connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);
        pthread_create(&tid, NULL, workerThread, (void *)connfd);
    }
}

void *workerThread(void *arg){
    int connfd = *((int *)arg);
	free(arg);
    pthread_detach(pthread_self());
    
    StreamerBuffer *streamer_buffer = (StreamerBuffer *)malloc(sizeof(StreamerBuffer));
    streamer_buffer->buffer = (int *)malloc(STREAMER_BUFFER_SIZE*sizeof(int));
    streamer_buffer->fd = connfd;
    sem_init(&(streamer_buffer->mutex), 0, 1);
    sem_init(&(streamer_buffer->empty), 0, STREAMER_BUFFER_SIZE);
    sem_init(&(streamer_buffer->full), 0, 0);

    pthread_t streamer_tid;
    pthread_create(&streamer_tid, NULL, streamer, (void *)streamer_buffer);

    char *bitrate = (char *)malloc(3*sizeof(char));
    strcpy(bitrate, "HD");
    EncoderData *encoder_data = (EncoderData *)malloc(sizeof(EncoderData));
    encoder_data->streamer_buffer = streamer_buffer;
    encoder_data->bitrate = bitrate;
    sem_init(&(encoder_data->bitrate_mutex), 0, 1);

    pthread_t encoder_tid;
    pthread_create(&encoder_tid, NULL, encoder, (void *)encoder_data);
    while(true){
        printf("Attempting to read options\n");
        char received[3];
        read(connfd, received, 2);
        received[3] = '\0';
        sem_wait(&(encoder_data->bitrate_mutex));
        strcpy(bitrate, received);
        bitrate[3] = '\0';
        printf("bitrate set to %s\n", bitrate);
        sem_post(&(encoder_data->bitrate_mutex));
    }
}

void *encoder(void *arg){
    EncoderData *encoder_data = (EncoderData *)arg;
    pthread_detach(pthread_self());

    StreamerBuffer *streamer_buffer = encoder_data->streamer_buffer;
    char *bitrate = encoder_data->bitrate;

    ThreadData thread_data;
    thread_data.fd = video_fd;
    thread_data.offset = 0;
    thread_data.size = READ_SIZE;
    thread_data.thread_id = pthread_self();

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

        char *selected;
        sem_wait(&(encoder_data->bitrate_mutex));
        selected = bitrate;
        printf("bitrate is supposedly %s\n", selected);
        sem_post(&(encoder_data->bitrate_mutex));
       
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

                if(strcmp(selected, "MD") == 0){  // MD encoder
                    if(counter == 0) add_current_number = true;
                    else add_current_number = false;
                    counter = (counter + 1)%10;
                }

                else if(strcmp(selected, "LD") == 0){ // LD encoder
                    if(counter == 0) add_current_number = true;
                    else add_current_number = false;
                    counter = (counter + 1)%100;
                }

                /*Since the encoders are mutually exclusive, HD will be the default 
                if none of above are activated*/

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
            write(streamer_buffer->fd, to_send, OUTPUT_BLOCK_SIZE*sizeof(int));
            usleep(1000*10*16);
            if(eof){
                printf("Finished!\n");
                break;
            }
        }
    } while (true);
    return NULL;
}