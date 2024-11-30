#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <signal.h>
#include "common.h"

#define READ_SIZE 4096 //reads this amount of bytes at a time from the txt file
#define STREAMER_BUFFER_SIZE 32 // max number of values that can be parsed before the streamer starts sending blocks
#define OUTPUT_BLOCK_SIZE 16 // number of values in each block streamed to the client

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
    signal(SIGPIPE, SIG_IGN);
    video_fd = open("video.txt", O_RDONLY);
    int listenfd;
    unsigned int clientlen;
    struct sockaddr_in clientaddr;
    char *port = "8080";
    listenfd = open_listenfd(port);

    if (listenfd < 0){
		connection_error(listenfd);
    }
    printf("Server listening on port %s.\n", port);
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
    printf("Connection established with FD: %d\n", connfd);
    
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

    char received[3];
    ssize_t bytes_read = 0;
    while(true){
        bytes_read = read(connfd, received, 2);
        received[3] = '\0';
        if(strcmp(received, "QT") == 0 || bytes_read < 0){
            close(connfd);
            pthread_join(streamer_tid, NULL);
            pthread_cancel(encoder_tid);
            pthread_join(encoder_tid, NULL);
            free(bitrate);
            free(streamer_buffer->buffer);
            free(streamer_buffer);
            free(encoder_data);
            printf("Connection with FD %d successfully terminated.\n",connfd);
            pthread_exit(NULL);
        }
        sem_wait(&(encoder_data->bitrate_mutex));
        strcpy(bitrate, received);
        bitrate[3] = '\0';
        sem_post(&(encoder_data->bitrate_mutex));
    }
}

void *encoder(void *arg){
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    EncoderData *encoder_data = (EncoderData *)arg;

    StreamerBuffer *streamer_buffer = encoder_data->streamer_buffer;
    char *bitrate = encoder_data->bitrate;
   
    char read_buffer[READ_SIZE + 1];
    
    int i = 0;
    int counter = 0;
    bool add_current_number = true;
    volatile int previous = -1;
    volatile int piece = -1;

    off_t offset = 0;
    ssize_t bytes_read = 0;
    
    do {
        bytes_read = pread(video_fd, read_buffer, READ_SIZE, offset);
        offset += bytes_read;
        read_buffer[bytes_read] = '\0';
        if (bytes_read < 0) {
            pthread_exit(NULL);
        }

        char *selected;
        sem_wait(&(encoder_data->bitrate_mutex));
        selected = bitrate;
        sem_post(&(encoder_data->bitrate_mutex));
       
        char *saveptr; // Save state for strtok_r
        char *number_string = strtok_r(read_buffer, ",", &saveptr);
        while(number_string){
            int number = atoi(number_string);
            if(number < previous && piece == -1){  /* It is very likely that the number of bytes read 
            will not match the separators between the numbers, so that each read,
            a number could be truncated. The only way for a number
            to be less than the previous one is if it has been truncated,
             so we store it to join it with the next one */
                piece = number;
            }
            else{
                if(piece != -1){
                    char *whole = (char *)malloc(6*sizeof(char)); // a number up to 99'999 has up to 5 digits, plus 1 for null terminator
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
            number_string = number_string = strtok_r(NULL, ",", &saveptr);
            previous = number;
        }
    } while (bytes_read > 0);
    sem_wait(&(streamer_buffer->empty));
    sem_wait(&(streamer_buffer->mutex));
    
    streamer_buffer->buffer[i] = -1; // end of the video indicator

    sem_post(&(streamer_buffer->mutex));
    sem_post(&(streamer_buffer->full));
    pthread_exit(NULL);
}


void *streamer(void *arg){
    StreamerBuffer *streamer_buffer = (StreamerBuffer *)arg;
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
            ssize_t bytes_written = write(streamer_buffer->fd, to_send, OUTPUT_BLOCK_SIZE*sizeof(int));
            if(eof || bytes_written < 0){
                pthread_exit(NULL);
            }
            usleep(1000*10*16);/* simulates the latency of sending massive amounts of data over the network,
            otherwise the file would be completely sent and buffered by the client in a moment
            without giving them the chance to change bitrate quality.
            */
        }
    } while (true);
}