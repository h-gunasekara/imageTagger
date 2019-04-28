/*
** http-server.c
*/

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// constants
static char const * const HTTP_200_FORMAT = "HTTP/1.1 200 OK\r\n\
Content-Type: text/html\r\n\
Content-Length: %ld\r\n\r\n";
static char const * const HTTP_400 = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
static int const HTTP_400_LENGTH = 47;
static char const * const HTTP_404 = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
static int const HTTP_404_LENGTH = 45;


#define MAXKEYWORDS 20
#define MAXKEYLENGTH 20

// represents the types of method
typedef enum
{
    GET,
    POST,
    UNKNOWN
} METHOD;

typedef struct
{
  int sockfd;
  char* name;
  int name_len;
  char* guesses[MAXKEYLENGTH];
  int num_guesses;
  int playing;
  int finished;
  int nextgame;
} player_t;


#define INTRO "1_intro.html"
#define START "2_start.html"
#define TURN "3_first_turn.html"
#define ACCEPTED "4_accepted.html"
#define DISCARDED "5_discarded.html"
#define END "6_endgame.html"
#define GAMEOVER "7_gameover.html"

int img = 1;

static bool send_page(int sockfd, int n, char* buff, char* page);

static bool handle_http_request(int sockfd, player_t* players)
{
    // try to read the request
    char buff[2049];
    int n = read(sockfd, buff, 2049);
    if (n <= 0)
    {
        if (n < 0)
            perror("read");
        else
            printf("socket %d close the connection\n", sockfd);
        return false;
    }

    // terminate the string
    buff[n] = 0;

    char * curr = buff;

    // parse the method
    METHOD method = UNKNOWN;
    if (strncmp(curr, "GET ", 4) == 0)
    {
        curr += 4;
        method = GET;
    }
    else if (strncmp(curr, "POST ", 5) == 0)
    {
        curr += 5;
        method = POST;
    }
    else if (write(sockfd, HTTP_400, HTTP_400_LENGTH) < 0)
    {
        perror("write");
        return false;
    }

    // sanitise the URI
    while (*curr == '.' || *curr == '/')
        ++curr;
        // assume the only valid request URI is "/" but it can be modified to accept more files



    if (method == GET)
    {
        // get the size of the file
        if (*curr == ' ')
        {
          return send_page(sockfd, n, buff, INTRO);

        } else if (strstr(buff, "start=Start") != NULL)
        {
          for (int i = 0; i < 2; ++i){
            if (players[i].sockfd == sockfd){
              if (players[i].finished == 1){
                  players[i].nextgame = 1;
              }
              players[i].playing = 1;
              players[i].finished = 0;
            }
          }
          return send_page(sockfd, n, buff, TURN);
        }

    }
    else if (method == POST)
    {
        // locate the username, it is safe to do so in this sample code, but usually the result is expected to be
        // copied to another buffer using strcpy or strncpy to ensure that it will not be overwritten.
        if(strstr(buff, "user=") != NULL)
        {
          char * username = strstr(buff, "user=") + 5;
          int username_length = strlen(username);
          // the length needs to include the ", " before the username
          if (players[0].name == NULL)
          {
            players[0].sockfd = sockfd;
            players[0].name = strdup(username);
            players[0].name_len = username_length;
            players[0].playing = 0;
            players[0].num_guesses = 0;
            players[0].finished = 0;
            players[0].nextgame = 0;
          }
          else
          {
            players[1].sockfd = sockfd;
            players[1].name = strdup(username);
            players[1].name_len = username_length;
            players[1].playing = 0;
            players[1].num_guesses = 0;
            players[1].finished = 0;
            players[0].nextgame = 0;
          }
          return send_page(sockfd, n, buff, START);
        }

        else if (strstr(buff, "quit=Quit") != NULL)
        {
          for (int i = 0; i < 2; ++i)
          {
            if (players[i].sockfd == sockfd)
            {
              players[i].playing = 0;
    //          players[i].finished = 1;
              printf("%s logged out on %d\n", players[i].name, sockfd);
            }
          }
          return send_page(sockfd, n, buff, GAMEOVER);
        }

        else if ((strstr(buff, "keyword=") != NULL) && players[0].playing == 1 && players[1].playing == 1 && players[0].finished == 0 && players[1].finished == 0)
        {
        	char * keyword = strstr(buff, "keyword=") + 8;
          int keyword_length = strlen(keyword) - 12;

        	int j = 1;
          int other;

        	for (int self = 0; self < 2; ++self)
        	{
        		if (players[self].sockfd == sockfd)
        		{
              players[self].guesses[players[self].num_guesses] = strndup(keyword, keyword_length);
              players[self].num_guesses++;
              other = j - self;
            }
          }


          for (int self = 0; self < 2; ++self)
        	{
            other = 1 - self;
            if (players[self].sockfd == sockfd){
              for (int guess = 0; guess < players[other].num_guesses; ++guess)
              {
                if (strcmp(players[other].guesses[guess], players[self].guesses[players[self].num_guesses - 1]) == 0)
                {
                  players[self].finished = 1;
                  players[self].nextgame = 0;
                  players[self].playing = 0;
                  players[other].playing = 0;
                  players[other].nextgame = 0;
                  if (img < 4)
            			{
            				img++;
            			} else if (img == 4){
                    img = 1;
                  }
                  for (int remove = 0; remove <= players[self].num_guesses; ++remove)
                  {
                    free(players[self].guesses[remove]);
                  }
                  for (int remove = 0; remove <= players[other].num_guesses; ++remove)
                  {
                    free(players[other].guesses[remove]);
                  }
                  //reset all stats here
                  return send_page(sockfd, n, buff, END);
                }
              }
            }
          }

          return send_page(sockfd, n, buff, ACCEPTED);
        }
        else if (strstr(buff, "keyword=") != NULL) {
          int j;
          int other;
          for (int self = 0; self < 2; ++self){
            other = 1 - self;
            if ((players[self].finished == 0 && players[self].nextgame == 0) && (players[other].finished == 1 || players[other].nextgame == 1)) {
                players[self].finished = 1;
                return send_page(sockfd, n, buff, END);

            }

          }
          return send_page(sockfd, n, buff, DISCARDED);
        }


        // int p1, p2;
        // for (p1 = size - 1, p2 = p1 - added_length; p1 >= size - 25; --p1, --p2)
        //     buff[p1] = buff[p2];
        // ++p2;
        // // put the separator
        // buff[p2++] = ',';
        // buff[p2++] = ' ';
        // // copy the username
        // strncpy(buff + p2, username, username_length);
        // if (write(sockfd, buff, size) < 0)
        // {
        //     perror("write");
        //     return false;
        // }
      }
    // send 404
    else if (write(sockfd, HTTP_404, HTTP_404_LENGTH) < 0)
    {
        perror("write");
        return false;
    }

    return true;
}

int main(int argc, char * argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "usage: %s ip port\n", argv[0]);
        return 0;
    }

    // create TCP socket which only accept IPv4
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // reuse the socket if possible
    int const reuse = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0)
    {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // create and initialise address we will listen on
    struct sockaddr_in serv_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    // if ip parameter is not specified
    serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
    serv_addr.sin_port = htons(atoi(argv[2]));

    // bind address to socket
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    // listen on the socket
    listen(sockfd, 5);

    // initialise an active file descriptors set
    fd_set masterfds;
    FD_ZERO(&masterfds);
    FD_SET(sockfd, &masterfds);
    // record the maximum socket number
    int maxfd = sockfd;
    player_t* players = malloc(sizeof(player_t) * 2);
    while (1)
    {
        // monitor file descriptors
        fd_set readfds = masterfds;
        if (select(FD_SETSIZE, &readfds, NULL, NULL, NULL) < 0)
        {
            perror("select");
            exit(EXIT_FAILURE);
        }

        // loop all possible descriptor
        for (int i = 0; i <= maxfd; ++i)
            // determine if the current file descriptor is active
            if (FD_ISSET(i, &readfds))
            {
                // create new socket if there is new incoming connection request
                if (i == sockfd)
                {
                    struct sockaddr_in cliaddr;
                    socklen_t clilen = sizeof(cliaddr);
                    int newsockfd = accept(sockfd, (struct sockaddr *)&cliaddr, &clilen);
                    if (newsockfd < 0)
                        perror("accept");
                    else
                    {
                        // add the socket to the set
                        FD_SET(newsockfd, &masterfds);
                        // update the maximum tracker
                        if (newsockfd > maxfd)
                            maxfd = newsockfd;
                        // print out the IP and the socket number
                        char ip[INET_ADDRSTRLEN];
                        printf(
                            "new connection from %s on socket %d\n",
                            // convert to human readable string
                            inet_ntop(cliaddr.sin_family, &cliaddr.sin_addr, ip, INET_ADDRSTRLEN),
                            newsockfd
                        );
                    }
                }
                // a request is sent from the client
                else if (!handle_http_request(i, players))
                {
                    close(i);
                    FD_CLR(i, &masterfds);
                }
            }
    }

    return 0;
}

static bool send_page(int sockfd, int n, char* buff, char* page) {
  struct stat st;
  stat(page, &st);
  // increase file size to accommodate the username
  n = sprintf(buff, HTTP_200_FORMAT, st.st_size);
  // send the header first
  if (write(sockfd, buff, n) < 0)
  {
      perror("write");
      return false;
  }
  // read the content of the HTML file
  int filefd = open(page, O_RDONLY);
  n = read(filefd, buff, 2048);
  if (n < 0)
  {
      perror("read");
      close(filefd);
      return false;
  }
  close(filefd);

  if (strcmp(page, TURN) == 0 || strcmp(page, ACCEPTED) == 0 || strcmp(page, DISCARDED) == 0)
  {
    sprintf(buff, buff, img);
  }

  if (write(sockfd, buff, st.st_size) < 0)
  {
      perror("write");
      return false;
  }

  return true;

}
