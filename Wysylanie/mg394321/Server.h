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
#include <thread>
#include <shared_mutex>
#include <mutex>
#include <sstream>

// Test
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>

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

namespace server {
    const int DEFAULT_SPACE  = 52428800;
    const uint64_t DEFAULT_TIMEOUT = 5;
    const bool debug_ON = true;
    const uint32_t MAX_DATA = 100000;
    const uint32_t MAX_SEND = 50000;
    const uint32_t bufferSize = 10000;
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

    void initalizeAll(std::string &SHRD_FLDR);

    void createEmptyFileForUpdate(std::string fileName, uint64_t fileSize); // ---

    void removeEmptyFileForUpdate(std::string fileName, uint64_t fileSize); // ---

    std::string getFilePath(std::string filePath);

    int removeFile(std::string &fileName); // ---

    bool fileExists(std::string &fileName, bool takeMutex); //

    uint64_t getFreeSpace(bool takeMutex); //
};

// Give two strings and checks first 10 bytes of both
// True when C-strings are equal
bool customStrCheck(const char *tab, const char *str);

// Handle discovery request
void respondDiscover(int socket, uint64_t specialSeq, uint64_t freeSpace, struct sockaddr_in &client_address);

// Handle search request
void respondSearch(int socket, std::string &data, uint64_t specialSeq, struct sockaddr_in &client_address);

// Handle remove request
void respondRemove(std::string &data, struct sockaddr_in &client_address);

// Handle fetch request
void respondFetch(int socket, std::string data, uint64_t specialSeq, struct sockaddr_in client_address);

// Handle upload request
void respondUpload(int socket, std::string data, uint64_t specialSeq, uint64_t param, struct sockaddr_in client_address);

// Send File through TCP
bool sendFile(int socket, std::string data);

// Send File through TCP
bool receiveFile(int socket, std::string data, uint64_t fileSize);

// Parses options for the program 1 when successful -1 otherwise
int parseOptions(int argc, char *argv[]);

// REads input from simple cmd returns number representing the command
int readCMD(uint64_t &specialSeq, uint64_t &param, std::string &data, struct sockaddr_in &client_address,
                   int &sock);

// Initialize main UDP socket
void initializeMainUDPSocket(int &sock);

/* Wypisuje informację o błędnym zakończeniu funkcji systemowej
i kończy działanie programu. */
void syserr(const char *fmt, ...);

// Sends data of length through socket
bool sendSomething(void *to_send, int socket, uint32_t length);

// Reads data of length from socket
int readSomething(void *to_read, int socket, uint32_t length, uint32_t &final);

#endif //ZADANIE2_SERVER_H
