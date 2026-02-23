#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUF_SIZE 500

int main(int argc, char *argv[]) {
  int sfd, sfd2, s, s2;
  char buf[BUF_SIZE], buf2[BUF_SIZE];
  ssize_t nread, nread2;
  socklen_t peer_addrlen, peer_addrlen2;
  struct addrinfo hints;
  struct addrinfo *result, *result2, *rp;
  struct sockaddr_storage peer_addr, peer_addr2;

  if (argc != 3) {
    fprintf(stderr, "Usage: %s port\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
  hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
  hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
  hints.ai_protocol = 0;          /* Any protocol */
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;

  s = getaddrinfo(NULL, argv[1], &hints, &result);
  s2 = getaddrinfo(NULL, argv[2], &hints, &result2);
  if (s != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
    exit(EXIT_FAILURE);
  }

  /* getaddrinfo() returns a list of address structures.
     Try each address until we successfully bind(2).
     If socket(2) (or bind(2)) fails, we (close the socket
     and) try the next address. */

  for (rp = result; rp != NULL; rp = rp->ai_next) {
    sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sfd == -1)
      continue;

    if (bind(sfd, rp->ai_addr, rp->ai_addrlen) == 0)
      break; /* Success */

    close(sfd);
  }
  for (rp = result2; rp != NULL; rp = rp->ai_next) {
    sfd2 = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sfd2 == -1)
      continue;

    if (bind(sfd2, rp->ai_addr, rp->ai_addrlen) == 0)
      break; /* Success */

    close(sfd2);
  }

  freeaddrinfo(result); /* No longer needed */

  if (rp == NULL) { /* No address succeeded */
    fprintf(stderr, "Could not bind\n");
    exit(EXIT_FAILURE);
  }

  /* Read datagrams and echo them back to sender. */

  for (;;) {
    char host[NI_MAXHOST], service[NI_MAXSERV];

    peer_addrlen = sizeof(peer_addr);
    peer_addrlen2 = sizeof(peer_addr2);
    nread = recvfrom(sfd, buf, BUF_SIZE, 0, (struct sockaddr *)&peer_addr,
                     &peer_addrlen);
    printf("there\n");
    nread2 = recvfrom(sfd2, buf2, BUF_SIZE, 0, (struct sockaddr *)&peer_addr2,
                     &peer_addrlen2);
    printf("there\n");
    if (nread == -1 || nread2 == -1)
      continue; /* Ignore failed request */

    s = getnameinfo((struct sockaddr *)&peer_addr, peer_addrlen, host,
                    NI_MAXHOST, service, NI_MAXSERV, NI_NUMERICSERV);
    s2 = getnameinfo((struct sockaddr *)&peer_addr2, peer_addrlen2, host,
                    NI_MAXHOST, service, NI_MAXSERV, NI_NUMERICSERV);
    if (s == 0)
      printf("Received depuis le premier port %zd bytes from %s:%s\n", nread, host, service);
    else if (s2 == 0)
      printf("Received depuis le second port %zd bytes from %s:%s\n", nread2, host, service);
    else
      fprintf(stderr, "getnameinfo: %s\n", gai_strerror(s));

    if (sendto(sfd, buf, nread, 0, (struct sockaddr *)&peer_addr,
               peer_addrlen) != nread) {
      fprintf(stderr, "Error sending response\n");
    }
  }
}
