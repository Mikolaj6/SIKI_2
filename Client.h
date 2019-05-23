#ifndef ZADANIE2_CLIENT_H
#define ZADANIE2_CLIENT_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <boost/version.hpp>
#include <iostream>
#include <iomanip>
#include "boost/program_options.hpp"
#include "boost/filesystem.hpp"

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/socket.h>
#include <chrono>
#include <ctime>
#include <random>

namespace po = boost::program_options;

namespace client {
    const uint64_t DEFAULT_TIMEOUT = 5;
    const int TTL_VALUE = 4;
    const uint32_t MAX_DATA = 50000;
    const bool debug_ON = true;
    const int ERROR = -1;
    const int EXIT = 0;
    const int DISCOVER = 1;
}

struct __attribute__((__packed__)) SIMPL_CMD_ {
    char cmd[10];
    uint64_t cmd_seq;
    char data[client::MAX_DATA];
};

struct __attribute__((__packed__)) CMPLX_CMD_ {
    char cmd[10];
    uint64_t cmd_seq;
    uint64_t param;
    char data[client::MAX_DATA];
};

using SIMPL_CMD = struct SIMPL_CMD_;
using CMPLX_CMD = struct CMPLX_CMD_;

int parseLine(std::string &line);

int parseOptions(int argc, char *argv[]);

int initializeUDPSocket(int &sock);

// returns true when successful
void setTimeout(int sock, time_t sec, suseconds_t micro);

void do_discover(int &sock);

/* Wypisuje informację o błędnym zakończeniu funkcji systemowej
i kończy działanie programu. */
void syserr(const char *fmt, ...);

/* Wypisuje informację o błędzie i kończy działanie programu. */
void fatal(const char *fmt, ...);

#endif //ZADANIE2_CLIENT_H
