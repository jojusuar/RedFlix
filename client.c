#include "common.c"

int main(int argc, char *argv[]){
    char *hostname = "localhost";
    char *port = "8080";
    int connfd = open_clientfd(hostname, port);
	if(connfd < 0)
		connection_error(connfd);
    while(1);
    return 0;
}