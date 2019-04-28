/*
** Title: image_tagger.c
** Author: Hamish Gunasekara
** Adapted from http-server.c
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

// definitions
#define MAXKEYWORDS 20
#define MAXKEYLENGTH 20
#define INTRO "1_intro.html"
#define START "2_start.html"
#define TURN "3_first_turn.html"
#define ACCEPTED "4_accepted.html"
#define DISCARDED "5_discarded.html"
#define END "6_endgame.html"
#define GAMEOVER "7_gameover.html"

// represents the types of method.
typedef enum
{
    GET,
    POST,
    UNKNOWN
} METHOD;

// struct of all the important information for eahc player.
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

// Starting image.
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

          // in the case where a player has pressed start
        } else if (strstr(buff, "start=Start") != NULL)
        {
          for (int i = 0; i < 2; ++i){
            if (players[i].sockfd == sockfd){
              // if they are playing again set nextgame to 1
              if (players[i].finished == 1) {
                players[i].nextgame += 1;
              }

              // set player to playing and not finished
              players[i].playing = 1;
              players[i].finished = 0;
            }
          }
          // send player to first turn
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
          // If there are no players then set player 1 stats
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
          // If there is already one player then set player 2 stats
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
        // In the case of a quit, stop player playing and log off
        else if (strstr(buff, "quit=Quit") != NULL)
        {
          for (int i = 0; i < 2; ++i)
          {
            if (players[i].sockfd == sockfd)
            {
              players[i].playing = 0;
              printf("%s logged out on %d\n", players[i].name, sockfd);
            }
          }
          return send_page(sockfd, n, buff, GAMEOVER);
        }

        // if a keyword has been submitted
        else if (strstr(buff, "keyword=") != NULL) {

          // goes through each player
          for (int self = 0; self < 2; ++self){
            if (players[self].sockfd == sockfd){
            int other;
            other = 1 - self;
            // if both players are playing and not finished
            if(players[self].playing == 1 && players[other].playing == 1 && (players[self].nextgame == players[other].nextgame)) {

              char * keyword = strstr(buff, "keyword=") + 8;
              int keyword_length = strlen(keyword) - 12;

              players[self].guesses[players[self].num_guesses] = strndup(keyword, keyword_length);
              players[self].num_guesses++;

              for (int guess = 0; guess < players[other].num_guesses; ++guess)
              {
                // if guessed correctly

                printf("other player word: %s     your keyword: %s      similarity: %d\n", players[other].guesses[guess], players[self].guesses[players[self].num_guesses - 1], strcmp(players[other].guesses[guess], players[self].guesses[players[self].num_guesses - 1]));


                if (strcmp(players[other].guesses[guess], players[self].guesses[players[self].num_guesses - 1]) == 0)
                {
                  // reset all stats
                  players[self].finished = 1;
                  players[self].playing = 0;

                  // move to new image
                  if (img < 4)
                  {
                    img++;
                  } else if (img == 4){
                    img = 1;
                  }
                  // remove the players current guess list
                  for (int remove = 0; remove <= players[self].num_guesses; ++remove)
                  {
                    free(players[self].guesses[remove]);
                  }
                  // remove the other players guess list
                  for (int remove = 0; remove <= players[other].num_guesses; ++remove)
                  {
                    free(players[other].guesses[remove]);
                  }
                  //reset all stats here
                  return send_page(sockfd, n, buff, END);
                }
              }

              return send_page(sockfd, n, buff, ACCEPTED);
            }
            // if the other player should be going to end game
            else if (players[self].playing == 1 && ((players[self].nextgame < players[other].nextgame) || players[other].playing == 0))   {
                players[self].finished = 1;
                players[self].playing = 0;
                return send_page(sockfd, n, buff, END);
            }
          }
          }
          return send_page(sockfd, n, buff, DISCARDED);

        }

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
  long size = st.st_size;
  if (strcmp(page, TURN) == 0 || strcmp(page, ACCEPTED) == 0 || strcmp(page, DISCARDED) == 0)
  {
    size = st.st_size + sizeof(img);
  }

  n = sprintf(buff, HTTP_200_FORMAT, size);
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

  // Change Image
  if (strcmp(page, TURN) == 0 || strcmp(page, ACCEPTED) == 0 || strcmp(page, DISCARDED) == 0)
  {
    sprintf(buff, buff, img);
  }

  // Show Username
  if (strcmp(page, START) == 0) {
    for (int i = 0; i < 2; ++i)
    {
      if (players[i].sockfd == sockfd)
      {
        sprintf(buff, buff, players[i].username);
      }
  }

  // Show Keywords
  if (strcmp(page, ACCEPTED) == 0) {
    for (int i = 0; i < 2; ++i)
    {
      if (players[i].sockfd == sockfd)
      {
        for (int guess = 0; guess < players[i].num_guesses; ++guess){
          if (guess != 0){
            sprintf(buff, buff, players[i].guesses[guess]);
          } else{
            sprintf(buff, buff, ", %s");
            sprintf(buff, buff, players[i].guesses[guess]);
          }

        }

      }
  }




  if (write(sockfd, buff, size) < 0)
  {
      perror("write");
      return false;
  }

  return true;

}
