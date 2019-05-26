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
#include <cwctype>
#include <functional>
#include <regex>
#include <unordered_set>
#include <thread>

namespace po = boost::program_options;
namespace fs = boost::filesystem;


namespace client {
    const uint64_t DEFAULT_TIMEOUT = 5;
    const int TTL_VALUE = 4;
    const uint32_t MAX_DATA = 100000;
    const bool debug_ON = true;
    const int ERROR = -1;
    const int EXIT = 0;
    const int DISCOVER = 1;
    const int SEARCH = 2;
    const int DELETE = 3;
    const int FETCH = 4;
}

struct __attribute__((__packed__)) SIMPL_CMD_ {
    char cmd[10];
    uint64_t cmd_seq;
    char data[client::MAX_DATA + 8];
};

struct __attribute__((__packed__)) CMPLX_CMD_ {
    char cmd[10];
    uint64_t cmd_seq;
    uint64_t param;
    char data[client::MAX_DATA];
};

using SIMPL_CMD = struct SIMPL_CMD_;
using CMPLX_CMD = struct CMPLX_CMD_;

class uint64_tGen {
    std::uniform_int_distribution<uint64_t> dis;
    std::random_device rd;
    std::mt19937 gen;

public:
    uint64_tGen();

    uint64_t genNum();
};

// Tests if line starts with a command, sets rest of the line when appropriate flag is set
bool testLine(std::string &line, std::string &&command, std::string &rest, int flag);

// Give two strings and checks first 10 bytes of both
// True when C-strings are equal
bool customStrCheck(const char *tab, const char *str);

// Prints skipping for client
void printSkipping(uint16_t badPort, std::string badAddress, int messageType);

// Checks what client types and returns code representing typed command
int parseLine(std::string &line, std::string &rest);

// 1 if options were set correctly, -1 otherwise
int parseOptions(int argc, char *argv[]);

// Initializes main UDP socket
void initializeUDPBroadcastSocketClient(int &sock);

// Sets timeout with given seconds and micro seconds
void setTimeout(int sock, time_t sec, suseconds_t micro);

// Function for discover
void do_discover(int &sock, uint64_tGen &gen);

// Function for search
void do_search(int &sock, uint64_tGen &gen, std::string &rest);

// Function for removal
void do_remove(int &sock, uint64_tGen &gen, std::string &rest);

// Function for fetch
void do_fetch(uint64_tGen &gen, std::string &rest);

// Create additional UDP socket
void normalUDPSocketClient(int &sock);

// Function for initial handling of folder
void check_folder(fs::path &p);

/* Wypisuje informację o błędnym zakończeniu funkcji systemowej
i kończy działanie programu. */
void syserr(const char *fmt, ...);

/* Wypisuje informację o błędzie i kończy działanie programu. */
void fatal(const char *fmt, ...);

#endif //ZADANIE2_CLIENT_H
