#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <time.h>    // time()

#include "err.h"

#define BSIZE         1024
#define TIMEBUFFSIZE  256
#define REPEAT_COUNT  3

int main (int argc, char *argv[]) {
  /* argumenty wywołania programu */
  char *multicast_dotted_address;
  in_port_t local_port;

  /* zmienne i struktury opisujące gniazda */
  int sock;
  struct sockaddr_in local_address;
  struct ip_mreq ip_mreq;

  /* Na odpowiedź do klienta */
  struct sockaddr_in client_address;
  socklen_t rcvLen;

  /* zmienne obsługujące komunikację */
  char buffer[BSIZE];
  ssize_t rcv_len;
  int i;
  int length;
  int len;

  char timeBuffer[TIMEBUFFSIZE]; // time stuff
  time_t time_buffer;

  /* parsowanie argumentów programu */
  if (argc != 3)
    fatal("Usage: %s multicast_dotted_address local_port\n", argv[0]);
  multicast_dotted_address = argv[1];
  local_port = (in_port_t)atoi(argv[2]);

  /* otworzenie gniazda */
  sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0)
    syserr("socket");

  /* podpięcie się do grupy rozsyłania (ang. multicast) */
  ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
  if (inet_aton(multicast_dotted_address, &ip_mreq.imr_multiaddr) == 0)
    syserr("inet_aton");
  if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&ip_mreq, sizeof ip_mreq) < 0)
    syserr("setsockopt");

  /* podpięcie się pod lokalny adres i port */
  local_address.sin_family = AF_INET;
  local_address.sin_addr.s_addr = htonl(INADDR_ANY);
  local_address.sin_port = htons(local_port);
  if (bind(sock, (struct sockaddr *)&local_address, sizeof local_address) < 0)
    syserr("bind");

  client_address.sin_family = AF_INET;
  client_address.sin_addr.s_addr = htonl(INADDR_ANY);
  client_address.sin_port = htons(local_port);

  memset(buffer, 0 , sizeof buffer);
  while(1) {
    rcvLen = (socklen_t) sizeof(client_address);

    length = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *) &client_address, &rcvLen);
    if (length < 0)
      syserr("error on datagram from client socket");
    else {
      (void) printf("read from socket: %d bytes: %.*s\n", length, (int) length, buffer);
      //// Got request from client time to reply
      // First save time
      memset(timeBuffer, 0 , TIMEBUFFSIZE);
      time(&time_buffer);
      strncpy(timeBuffer, ctime(&time_buffer), TIMEBUFFSIZE);
      length = strnlen(timeBuffer, TIMEBUFFSIZE);

      // Czy do unicastowania trzeba tworzyć nowy socket???
      len = sendto(sock, timeBuffer, length, 0, (struct sockaddr *) &client_address, rcvLen);
      printf("len: %d length: %d\n", len, length); fflush(stdout);
      if(len != length)
        syserr("error sending time to client");
    }
    memset(buffer, 0 , sizeof buffer);
  }
}
