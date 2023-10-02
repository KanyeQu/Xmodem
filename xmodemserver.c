#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include "xmodemserver.h"
#include "crc16.h"

#ifndef PORT
  #define PORT 53861 
#endif

#define MAXBUFFER 1024

struct client *head = NULL;
int clientCount = 0;
// !!!!copied from muffinman.c 
static int listenfd;
char signal;

void bindandlisten()  /* bind and listen, abort on error */
{
    struct sockaddr_in r;

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    int yes = 1;
    if((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }

    memset(&r, '\0', sizeof r);
    r.sin_family = AF_INET;
    r.sin_addr.s_addr = INADDR_ANY;
    r.sin_port = htons(PORT);

    printf("listening on port %d\n", PORT);

    if (bind(listenfd, (struct sockaddr *)&r, sizeof r)) {
        perror("bind");
        exit(1);
    }

    if (listen(listenfd, 5)) {
        perror("listen");
        exit(1);
    }
}
// !!!copied from muffinman
static void addclient(int fd)
{
    struct client *p = malloc(sizeof(struct client));
    if (!p) {
        fprintf(stderr, "out of memory!\n");  /* highly unlikely to happen */
        exit(1);
    }
    struct sockaddr_in r;
    printf("Adding client %s\n", inet_ntoa(r.sin_addr));
    fflush(stdout);
    p->fd = fd;               // socket descriptor for this client
    p->buf[0] = '\0';       // buffer to hold data being read from client
    p->inbuf = 0;            // index into buf
    p->filename[0] = '\0';    // name of the file being transferred
    // p->fp = NULL;             // file pointer for where the file is written to
    p->state = initial;  // current state of data transfer for this client
    // p->blocksize = NULL;        // the size of the current block
    p->current_block = 1;    // the block number of the current block
    p->next = head;
    head = p;
    clientCount++;
}

// !!!copied from muffinman
void newconnection()  /* accept connection, sing to them, get response, update
                       * linked list */
{
    int fd;
    struct sockaddr_in r;
    socklen_t socklen = sizeof r;

    if ((fd = accept(listenfd, (struct sockaddr *)&r, &socklen)) < 0) {
        perror("accept");
    } 
    else {
        printf("connection from %s\n", inet_ntoa(r.sin_addr));
        addclient(fd);
    }
    
}

// !!!!copied from helper.c
FILE *open_file_in_dir(char *filename, char *dirname) {
    char buffer[MAXBUFFER];
    strncpy(buffer, "./", MAXBUFFER);
    strncat(buffer, dirname, MAXBUFFER - strlen(buffer));

    // create the directory dirname. Fail silently if directory exists
    if(mkdir(buffer, 0700) == -1) {
        if(errno != EEXIST) {
            perror("mkdir");
            exit(1);
        }
    }
    strncat(buffer, "/", MAXBUFFER - strlen(buffer));
    strncat(buffer, filename, MAXBUFFER - strlen(buffer));

    return fopen(buffer, "w");
}

void FSM_OP();
void disconnect(struct client *client);
static void removeclient(int fd);  // !!!copied from muffinman

int main(){
    bindandlisten();
    struct client *curr;
    while(1){
        fd_set fds;
        int maxfd = listenfd;
        FD_ZERO(&fds);
        FD_SET(listenfd, &fds);
        curr = head;
        while (curr != NULL){
            FD_SET(curr->fd, &fds);
            if (curr->fd > listenfd)
                maxfd = curr->fd;
            curr = curr->next;
        }
        // !!!copied from muffinman.c
        if (select(maxfd + 1, &fds, NULL, NULL, NULL) < 0) {
            perror("select");
        }
        if (FD_ISSET(listenfd, &fds)){
            newconnection();
        }
        curr = head;
        while (curr != NULL){
            if (FD_ISSET(curr->fd, &fds)){
                FSM_OP(curr);
            }
            curr = curr->next;
        }
        fflush(stdout);
    }




    return 0;
}

void FSM_OP(struct client *client){
    unsigned char free;
    if (client->state == initial){
        // seems good. Guts tells me theres a bug.
        int num;
        if ((num = read(client->fd, &client->buf[client->inbuf], 20)) == 0){
            perror("read from client error");
        }
        client->inbuf += num;
        if (client->inbuf > 21){
            perror("goes here!\n");            
            perror("filename too long. disconnect\n");
            perror("goes here?\n");
            removeclient(client->fd);
        }
        if (strstr(client->buf, "\r\n")){
            for (int i = 0; i < 20; i++){
                if (client->buf[i] == '\r')
                    client->buf[i] = '\0';
            }
            strncpy(client->filename, client->buf, 20);
        
            client->fp = open_file_in_dir(strcat(client->filename, ".junk"), "filestore");
            client->inbuf = 0;
            client->state = pre_block;
            if (write(client->fd, "C", 1) <= 0){
                perror("write C to client failed");
            }
        }
        return;

    }
    if (client->state == pre_block){
        char buf;
        if (read(client->fd, &buf, 1) == 0){
            perror("read head failed");
            exit(1);
        }
        // printf("buf is %c \n", buf);
        if (buf == EOT){
            client->state = finished;
            signal = ACK;
            if (write(client->fd, &signal, 1) == -1){
                perror("write");
            }
        }
        else if(buf == SOH){
            client->blocksize = 128;
            client->state = get_block;
            return;
        }
        else if(buf == STX){
            client->blocksize = 1028;
            client->state = get_block;
            return;
        }
        else{
            perror("shouldn't get here from pre_block");
            exit(1);
        }
    }

    if (client->state == get_block){
        int num;
        if ((num = read(client->fd, &client->buf[client->inbuf], client->blocksize + 5)) == 0){
            perror("read from phase get_block");
            exit(1);
        }
        client->inbuf += num;
        if (client->inbuf > client->blocksize + 4){
            perror("client sent too much for a block. disconnected");
            removeclient(client->fd);
        }
        else if(client->inbuf < client->blocksize + 4){
            // shouldn't do anything. wait for more byte.
            printf("didn't sent a whole block [%d]. wait for more. \n", client->inbuf);
        }
        else{
            printf("whole block done. proceed to next phase\n");
            client->state = check_block;
            client->inbuf = 0;
        }
    }

    if (client->state == check_block){
        printf("here\n");
        int i;
        unsigned char block = client->buf[0];
        unsigned char inverse = client->buf[1];
        printf("the block number and inverse is %u %u \n", block, inverse);
        unsigned char buffer[MAXBUFFER] = {0};
        for (i = 0; i < client->blocksize; i++){
            buffer[i] = client->buf[i + 2];
        }
        unsigned char client_crc1 = client->buf[client->blocksize + 2];
        unsigned char client_crc2 = client->buf[client->blocksize + 3];
        printf("\n");
        printf("crc1 from client: [%x], crc2 from client: [%x]\n", client_crc1, client_crc2);

        unsigned short CRC = crc_message(XMODEM_KEY, buffer, client->blocksize);
        // printf("CRC from server [%x]\n", CRC);
        if (block != 255 - inverse){
            perror("critical! blocknumber do not correspond to inverse. dropping connection");
            removeclient(client->fd);
        }

        if (block == client->current_block - 1){
            signal = ACK;
            if (write(client->fd, &signal, 1) == -1){
                perror("shouldn't happened");
            }
        }
        else if (block != client->current_block){
            perror("critical! blocknumber do not correspond to inverse. dropping connection");
            removeclient(client->fd);

        }
        else if ((free = CRC >> 8) != client_crc1 || (free = CRC) != client_crc2){
            printf("CRC doesn't match crc server:[%x], crc client:[%x %x]\n", CRC, client_crc1, client_crc2);
            printf("\n");
            signal = NAK;
            if (write(client->fd, &signal, 1) == -1){
                perror("write NAK error");
            }
        }
        else{
            (client->current_block)++;
            if (client->current_block > 255)
                client->current_block = 0;
            signal = ACK;
            if (write(client->fd, &signal, 1) == -1){
                perror("write failed. Unlikely to happen");
            }
            if(fwrite(buffer, sizeof(unsigned char), client->blocksize, client->fp) < client->blocksize) {
                fprintf(stderr, "Error: write failed on init\n");
                // closefs(fp);
                // exit(1);
            }
            printf("write file succeed\n");
            client->state = pre_block;
        }
    }
    if (client->state == finished){
        printf("client [%s] receiving finished. disconnected\n", client->filename);
        fclose(client->fp);
        removeclient(client->fd);
        return;
    }
    return;
}

// copied from muffinman
static void removeclient(int fd)
{
    perror("here");
    struct client **p;
    for (p = &head; *p && (*p)->fd != fd; p = &(*p)->next);
    if (*p) {
        struct client *t = (*p)->next;
        if ((*p)->filename[0] != '\0')
            printf("Removing client %s\n", (*p)->filename);
        else
            printf("Removing client that doesn't have a filename");

        // if ((*p)->fp != NULL)
        //     fclose((*p)->fp);
        // perror("after fclose fp");
        close((*p)->fd);
        perror("after fclose fd");
        fflush(stdout);
        free(*p);
        *p = t;
        clientCount--;
    } 
    else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n", fd);
        fflush(stderr);
    }
}