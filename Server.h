#ifndef ZADANIE2_SERVER_H
#define ZADANIE2_SERVER_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <iostream>
#include <cstdint>
#include <iomanip>
#include "boost/program_options.hpp"
#include "boost/filesystem.hpp"
#include <boost/foreach.hpp>

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


namespace po = boost::program_options;
namespace fs = boost::filesystem;

namespace server {
    const int DEFAULT_SPACE  = 52428800;
    const uint64_t DEFAULT_TIMEOUT = 5;
    const bool debug_ON = true;
    const uint32_t MAX_DATA = 100000;
    const uint32_t MAX_SEND = 50000;
}

struct __attribute__((__packed__)) SIMPL_CMD {
    char cmd[10];
    uint64_t cmd_seq;
    char data[server::MAX_DATA + 8];
};

struct __attribute__((__packed__)) CMPLX_CMD {
    char cmd[10];
    uint64_t cmd_seq;
    uint64_t param;
    char data[server::MAX_DATA];
};

using SIMPL_CMD = struct SIMPL_CMD;
using CMPLX_CMD = struct CMPLX_CMD;

// Class for handling of folder and files
class FileManager {
private:
    // Can be negative, that means that files in folder have larger size than available free space
    // Files can only be removed in that situation, once positive it can never go negative !!!
    uint64_t freeSpace;
    bool isNegative;
    fs::path p;
    fs::path folderPath;

public:
    std::map<std::string, fs::path> availableFiles;

    FileManager(std::string &SHRD_FLDR);

    int removeFile(std::string &fileName);

    bool fileExists(std::string &fileName);

    uint64_t getFreeSpace();
};

// Give two strings and checks first 10 bytes of both
// True when C-strings are equal
bool customStrCheck(const char *tab, const char *str);

// Handle discovery request
void respondDiscover(int socket, uint64_t specialSeq, uint64_t freeSpace, struct sockaddr_in &client_address);

// Handle search request
void respondSearch(int socket, FileManager &fm, std::string &data, uint64_t specialSeq, struct sockaddr_in &client_address);

// Handle remove request
void respondRemove(FileManager &fm, std::string &data, struct sockaddr_in &client_address);

// Handle fetch request
void respondFetch(FileManager &fm, std::string &data, uint64_t specialSeq, struct sockaddr_in &client_address);

// Parses options for the program 1 when successful -1 otherwise
int parseOptions(int argc, char *argv[]);

// REads input from simple cmd returns number representing the command
int readCMD(uint64_t &specialSeq, std::string &data, struct sockaddr_in &client_address,
                   int &sock);

// Initialize main UDP socket
void initializeMainUDPSocket(int &sock);

// Initialize TCP socket
void TCPSocketServer(int &sock);

/* Wypisuje informację o błędnym zakończeniu funkcji systemowej
i kończy działanie programu. */
void syserr(const char *fmt, ...);

/* Wypisuje informację o błędzie i kończy działanie programu. */
void fatal(const char *fmt, ...);

#endif //ZADANIE2_SERVER_H
