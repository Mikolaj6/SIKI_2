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
#include <shared_mutex>
#include <mutex>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <sstream>

namespace po = boost::program_options;
namespace fs = boost::filesystem;

namespace logging = boost::log;

void init_logging()
{
    logging::core::get()->set_filter
            (
                    logging::trivial::severity > logging::trivial::fatal
            );
}

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
    const int UPLOAD = 5;
    const uint32_t bufferSize = 10000;
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

// Checks what client types and returns code representing typed command
int parseLine(std::string &line, std::string &rest);

// 1 if options were set correctly, -1 otherwise
int parseOptions(int argc, char *argv[]);

// Initializes main UDP socket
void initializeUDPBroadcastSocketClient(int &sock);

// Sets timeout with given seconds and micro seconds
void setTimeout(int sock, time_t sec, suseconds_t micro);

// Function for discover
void do_discover(int &sock, bool modeNormal, std::set<std::pair<std::string, uint64_t>, bool (*)(const std::pair<std::string, uint64_t>&, const std::pair<std::string, uint64_t>&)> &setServerSpace);

// Function for search
void do_search(int &sock, std::string &rest);

// Function for removal
void do_remove(int &sock, std::string &rest);

// Function for fetch
void do_fetch(std::string rest);

// Function for upload
void do_upload(std::string rest);

// Send File through TCP
// True -> Sending was successful
// False -> there was an error
bool sendFileClient(int socketTCP, std::string data);

// Save file read through TCP socket in OUT_FOLDER
bool saveFile(int TcpSocket, std::string &filePath);

// Create additional UDP socket
void normalUDPSocketClient(int &sock);

// Function for initial handling of folder
void check_folder();

// True -> Recv was correct, False -> there was a timeout
int clientValidateRecv(ssize_t length, std::string &&where, bool inLoop);

// Creates TCP socket for client and connects it to server
void createTCPClientSocket(int &socket, uint16_t port, struct sockaddr_in serverAddr);

/* Wypisuje informację o błędnym zakończeniu funkcji systemowej
i kończy działanie programu. */
void syserr(const char *fmt, ...);

// Sends simple message to given coordinates, returns code sequence
uint64_t sendSimple(int UDPsocket, uint16_t port, std::string servAddr, const char *messageType, std::string dataFieled, bool isClient);

// Sends simple message to given coordinates, returns code sequence
uint64_t sendComplex(int UDPsocket, uint16_t port, std::string servAddr, const char *messageType, uint64_t param, std::string dataFieled, bool isClient);

// For receiving simple or complex message, returns received size
ssize_t receiveSth(int UDPsocket, void *ptr, struct sockaddr_in &receivingFrom, bool isComplex);

// Prints skipping for client
bool printSkipping(struct sockaddr_in &receivingAddr, int messageType);

// Werify and print skiping (false -> skipped, true otherwise
// When Bool is True print skipping for given type
bool verifyReceived(struct sockaddr_in &receiving_address, bool verify0, bool verify1, bool verify2, bool verify3);

// Sends data of length through socket
bool sendSomething(void *to_send, int socket, uint32_t length);

// Reads data of length from socket
int readSomething(void *to_read, int socket, uint32_t length, uint32_t &final);

#endif //ZADANIE2_CLIENT_H
