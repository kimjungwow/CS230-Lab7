#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

static const char *user_agent_hdr = "\r\nUser-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

char proxystring[] = "./proxy";
char eighty[] = "80";
char enterport[20];
int ee = 80;
char *first_get, *second_url, *third_http, *parse_host, *parse_path;
void echo(int connfd);
void writeerror(int connfd);
void parse(int connfd);

void *parse_thread(void *vargp);
int main(int argc, char **argv)
{

    Signal(SIGPIPE, SIG_IGN);
    int listenfd;
    // int connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr; // Enough space for any address
    char client_hostname[MAXLINE], client_port[MAXLINE];

    if (argc != 2)
    {
        fprintf(stderr, "usage: ./proxy <port>\n");
        exit(0);
    }
    pthread_t tid;
    listenfd = open_listenfd(argv[1]);
    while (1)
    {
        int *connfdp = malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);

        pthread_create(&tid, NULL, parse_thread, connfdp);
    }
    Close(listenfd);
    exit(0);
}

void *parse_thread(void *vargp)
{
    int connfd = *((int *)vargp);
    pthread_detach(pthread_self());
    free(vargp);

    parse(connfd);
    Close(connfd);
    return NULL;
}

void echo(int connfd)
{
    size_t n;
    char buf[MAXLINE];
    rio_t rio;

    Rio_readinitb(&rio, connfd);
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
    {
        printf("server received %d bytes\n", (int)n);
        Rio_writen(connfd, buf, n);
    }
    return;
}

void writeerror(int connfd)
{
    Rio_writen(connfd, "usage: GET [URL] HTTP/1.1\n\n", 26);
    return;
}

void parse(int connfd)
{
    size_t n, m;
    char buf[MAXLINE], buf_cli[MAXLINE];
    rio_t rio, rio_cli;
    int clientfd, binary = 0;
    Rio_readinitb(&rio, connfd);
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
    {

        printf("%s\n", buf);
        int i, space = 0;
        for (i = 0; i < n; i++)
        {
            if (buf[i] == ' ')
            {
                space++;
            }
        }

        if (space != 2)
        {
            writeerror(connfd);
            continue;
        }
        // printf("server received %d bytes\n", (int)n);
        first_get = strtok(buf, " ");
        if (strcmp(first_get, "GET"))
        {
            Rio_writen(connfd, "We only use GET method\n\n", 24);
            continue;
        }

        second_url = strtok(NULL, " ");
        if (strncmp(second_url, "http://", 7))
        {
            Rio_writen(connfd, "URL should start with http:// \n\n", 32);
            continue;
        }

        third_http = strtok(NULL, " ");

        if ((strncmp(third_http, "HTTP/1.0", 8) != 0) && (strncmp(third_http, "HTTP/1.1", 8) != 0))
        {
            Rio_writen(connfd, "Please use HTTP/1.0 or HTTP/1.1\n\n", 33);
            continue;
        }
        parse_host = strtok(second_url, "/"); // Overwrite

        parse_host = strtok(NULL, "/");

        parse_path = strtok(NULL, "");

        char *givenport = strchr(parse_host, ':');
        if (givenport != NULL)
        {
            parse_host = strtok(parse_host, ":");

            strcpy(enterport, givenport + 1);
        }
        else
        {
            strcpy(enterport, eighty);
        }

        if ((clientfd = open_clientfd(parse_host, enterport)) < 0)
        {
            continue;
        }
        printf("Connected! host: %s port %s\n", parse_host, eighty);
        Rio_readinitb(&rio_cli, clientfd);
        strcat(buf_cli, "GET /");
        if (parse_path != NULL)
        {
            strcat(buf_cli, parse_path);
        }
        strcat(buf_cli, " HTTP/1.0\r\nHost: ");
        strcat(buf_cli, parse_host);
        strcat(buf_cli, user_agent_hdr);
        strcat(buf_cli, "Connection: close\r\nProxy-Connection: close\r\n\r\n");

        Rio_writen(clientfd, buf_cli, strlen(buf_cli));

        char *binarystr = strstr(parse_path, ".jpg");
        if (binarystr != NULL)
        {
            binary = 1;
        }
        binarystr = strchr(parse_path, '.');
        if (binarystr == NULL)
        {
            binary = 1;
        }


        char *binlen;
        if (binary == 0)
        {
            while ((m = Rio_readlineb(&rio_cli, buf_cli, MAXLINE)) > 0)
            {
                Rio_writen(connfd, buf_cli, strlen(buf_cli));
            }
        }
        else
        {
            
            while ((m = Rio_readlineb(&rio_cli, buf_cli, MAXLINE)) > 0)
            {

                if (strncmp(buf_cli, "Content-length:", 15) == 0)
                {
                    char buf_clitemp[20];
                    strcpy(buf_clitemp,buf_cli);
                    binlen = strtok(buf_clitemp, ":");
                    binlen = strtok(NULL, " ");
                }

                
                Rio_writen(connfd, buf_cli, strlen(buf_cli));
                if(strncmp(buf_cli, "Content-type:",13)==0) {break;}
            }
            char *bindata = malloc(atoi(binlen));
            while ((m = Rio_readnb(&rio_cli, bindata, atoi(binlen))) > 0)
            {

                Rio_writen(connfd, bindata, atoi(binlen));
            }
        }

        Close(clientfd);
    }

    return;
}