#include "Server.h"

// Variables
static uint16_t CMD_PORT;
static uint64_t MAX_SPACE;
static std::string MCAST_ADDR;
static int TIMEOUT;
static std::string SHRD_FLDR;

static struct sockaddr_in group_address; // moze byc gdzie indziej

// Protecting files with mutex
FileManager fm;
std::shared_mutex fileManagerMutex;

// Preventing output from mixing
std::mutex outputMutex;

int main(int argc, char *argv[]) {
    init_logging();

    int socket;
    uint64_t specialSeq;
    struct sockaddr_in client_address;

    if(parseOptions(argc, argv) != 1) {
        std::cerr << "Failed parsing options\n";
        exit(1);
    }


    initializeMainUDPSocket(socket);

    // Initial file management
    fm.initalizeAll(SHRD_FLDR);

    // Received from udp
    std::string data;
    uint64_t param;

    while (true) {
        int cmd = readCMD(specialSeq, param, data, client_address, socket);

        switch (cmd) {
            case -2: {
                // TIMEOUT
                break;
            }
            case -1: {
                break;
            }
            case 0: {
                BOOST_LOG_TRIVIAL(debug) << "{MATCH DATA} Nothing matched send message";
                break;
            }
            case 1: {
                respondDiscover(socket, specialSeq, fm.getFreeSpace(true), client_address);
                break;
            }
            case 2: {
                respondSearch(socket, data, specialSeq, client_address);
                break;
            }
            case 3: {
                respondRemove(data, client_address);
                break;
            }
            case 4: {
                std::thread(respondFetch, socket, data, specialSeq, client_address).detach();
                break;
            }
            case 5: {
                std::thread(respondUpload, socket, data, specialSeq, param, client_address).detach();
                break;
            }
            default: {
                // Never here
                break;
            }
        }
    }
}

// Handle discovery request
void respondDiscover(int socket, uint64_t specialSeq, uint64_t freeSpace, struct sockaddr_in &client_address) {
    CMPLX_CMD response;
    memset(&response, 0, sizeof(response));
    strcpy(response.cmd, "GOOD_DAY\0\0");
    response.cmd_seq = specialSeq;
    response.param = htobe64(freeSpace);
    strcpy(response.data, MCAST_ADDR.c_str());

    ssize_t responseLength = MCAST_ADDR.size() + 26;
    auto sendLen = (socklen_t) sizeof(client_address);
    if(sendto(socket, &response, responseLength, 0, (struct sockaddr *)&client_address, sendLen) != responseLength)
        BOOST_LOG_TRIVIAL(debug) << "Bad write for discover";
}

void respondSearch(int socket, std::string &data, uint64_t specialSeq, struct sockaddr_in &client_address) {
    SIMPL_CMD response;
    uint64_t leftInBuffer = server::MAX_SEND;
    ssize_t position = 0;
    bool someFile = false;

    memset(&response, 0, sizeof(response));
    auto sendLen = (socklen_t) sizeof(client_address);

    fileManagerMutex.lock_shared();
    for (auto &it : fm.availableFiles) {
        std::string str = it.first;

        if(!data.empty() && str.find(data) == std::string::npos)
            continue;

        if(leftInBuffer < str.length()) {
            strcpy((response.data + position), str.c_str());
            position += str.length() + 18;
            strcpy(response.cmd, "MY_LIST\0\0\0");
            response.cmd_seq = specialSeq;

            if(sendto(socket, &response, position, 0, (struct sockaddr *)&client_address, sendLen) != position)
                BOOST_LOG_TRIVIAL(debug) << "Bad write for search";
            BOOST_LOG_TRIVIAL(debug) << "[MY_LIST] To: " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << " length: " << position ;

            position = 0;
            leftInBuffer = server::MAX_SEND;
            memset(&response, 0, sizeof(response));
            someFile = false;
        } else {
            someFile = true;
            leftInBuffer -= str.length() + 1;
            strcpy((response.data + position), str.c_str());
            response.data[position + str.length()] = '\n';
            position += str.length() + 1;
        }
    }

    fileManagerMutex.unlock_shared();

    if(someFile) {
        response.data[position - 1] = '\0';
        strcpy(response.cmd, "MY_LIST\0\0\0");
        response.cmd_seq = specialSeq;

        position += 17;
        if(sendto(socket, &response, position, 0, (struct sockaddr *)&client_address, sendLen) != position)
            BOOST_LOG_TRIVIAL(debug) << "Bad write for search";

        BOOST_LOG_TRIVIAL(debug) << "[MY_LIST] To: " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << " length: " << position;
    }
}

// Handle remove request
void respondRemove(std::string &data, struct sockaddr_in &client_address) {
    BOOST_LOG_TRIVIAL(debug) << "[DEL] From: " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << " file|" << data << "|" ;

    int x = fm.removeFile(data);

    if(x == -1) {
        BOOST_LOG_TRIVIAL(debug) << "Undefined behaviour removing a file";
    } else if(x == 0) {
        BOOST_LOG_TRIVIAL(debug) << "File was not in the set of available files";
    } else {
        BOOST_LOG_TRIVIAL(debug) << "Removal was successful";
    }
}

//BOOST_LOG_TRIVIAL(debug) << "{MATCH DATA} Nothing matched send message";

// Handle fetch request
void respondFetch(int socket, std::string data, uint64_t specialSeq, struct sockaddr_in client_address) {

    if(!fm.fileExists(data, true)) {
        outputMutex.lock();
        std::cerr << "[PCKG ERROR] Skipping invalid package from " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << ". Fetched file did not exist" << std::endl;
        outputMutex.unlock();
        return;
    }

    BOOST_LOG_TRIVIAL(debug) << "{FETCH} FILE|" << data << "|EXISTS";

    // Sending CMPLX_CMD in response
    CMPLX_CMD response;
    memset(&response, 0, sizeof(response));
    strcpy(response.cmd, "CONNECT_ME\0");
    response.cmd_seq = specialSeq;
    strcpy(response.data, data.c_str());

    ssize_t responseLength = data.size() + 26;
    auto sendLen = (socklen_t) sizeof(client_address);

    BOOST_LOG_TRIVIAL(debug) << "{FETCH} [CONNECT ME] successful: " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port);

    // ifstream ofstream
    // Setting up socket for TCP connection
    int tcpSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (tcpSocket < 0){
        BOOST_LOG_TRIVIAL(debug) << "{FETCH} Error creating TCP socket";
        return;
    }

    struct sockaddr_in newTCP;
    memset(&newTCP, 0 , sizeof(newTCP));
    // assign IP, PORT
    newTCP.sin_family = AF_INET;
    newTCP.sin_addr.s_addr = htonl(INADDR_ANY);;
    newTCP.sin_port = 0;
    if ((bind(tcpSocket, (struct sockaddr *)&newTCP, sizeof(newTCP))) != 0) {
        BOOST_LOG_TRIVIAL(debug) << "{FETCH} Error binding TCP socket for fetch";
        if (close(tcpSocket) < 0)
            BOOST_LOG_TRIVIAL(debug) << "{FETCH} Error closing socket";
        return;
    }
    if ((listen(tcpSocket, 5)) != 0) {
        BOOST_LOG_TRIVIAL(debug) << "{FETCH} Listen failed...";
        if (close(tcpSocket) < 0)
            BOOST_LOG_TRIVIAL(debug) << "{FETCH} Error closing socket..";
        return;
    }


    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getsockname(tcpSocket, (struct sockaddr *)&sin, &len) == -1) {
        BOOST_LOG_TRIVIAL(debug) << "{FETCH} Error getting socket name..";
        if (close(tcpSocket) < 0)
            BOOST_LOG_TRIVIAL(debug) << "{FETCH} Error closing socket...";
        return;
    }
    else {
        response.param = htobe64(ntohs(sin.sin_port));
        if(sendto(socket, &response, responseLength, 0, (struct sockaddr *)&client_address, sendLen) != responseLength) {
            BOOST_LOG_TRIVIAL(debug) << "{FETCH} Error sending CONNECT_ME...";
            if (close(tcpSocket) < 0)
                BOOST_LOG_TRIVIAL(debug) << "{FETCH} Error closing socket...";
            return;
        }
    }

    fd_set set;
    FD_ZERO(&set); /* clear the set */
    FD_SET(tcpSocket, &set); /* add our file descriptor to the set */

    struct timeval timeout;
    timeout.tv_sec = TIMEOUT;
    timeout.tv_usec = 0;

    int rv;         // Select value
    int msg_sock;   // Socket for sending TCP packages
    rv = select(tcpSocket + 1, &set, NULL, NULL, &timeout);
    if(rv == -1) {
        BOOST_LOG_TRIVIAL(debug) << "{FETCH} Error in select...";
        if (close(tcpSocket) < 0)
            BOOST_LOG_TRIVIAL(debug) << "{FETCH} Error closing socket...";
        return;
    }
    else if(rv == 0) {
        BOOST_LOG_TRIVIAL(debug) << "{FETCH} Listen timeout";
        if (close(tcpSocket) < 0)
            BOOST_LOG_TRIVIAL(debug) << "{FETCH} Error closing socket...";
        return;
    }

    socklen_t client_address_len = sizeof(client_address);
    msg_sock = accept(tcpSocket, (struct sockaddr *) &client_address, &client_address_len);
    if(msg_sock < 0) {
        BOOST_LOG_TRIVIAL(debug) << "{FETCH} Error accepting socket";
        if (close(tcpSocket) < 0)
            BOOST_LOG_TRIVIAL(debug) << "{FETCH} Error closing socket...";
        return;
    }
    BOOST_LOG_TRIVIAL(debug) << "{FETCH} Connection established ready to send data";

    // Teraz trzeba wysłać info z powrotem otworzyć połączenie
    // Czekamy TIMEOUT i kończymy
    if(sendFile(msg_sock, data)) {
        BOOST_LOG_TRIVIAL(debug) << "{FETCH} File succesfully send";
    }

    if (close(msg_sock) < 0)
        BOOST_LOG_TRIVIAL(debug) << "{FETCH} Error closing socket...";
}

// Handle upload request
void respondUpload(int socket, std::string data, uint64_t specialSeq, uint64_t param, struct sockaddr_in client_address) {
    fileManagerMutex.lock();
    if(fm.fileExists(data, false) || data.find('/') != std::string::npos || be64toh(param) > fm.getFreeSpace(false)) {
        if(fm.fileExists(data, false))
            BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} FILE|" << data << "| ALREADY EXISTS";
        else if(data.find('/') != std::string::npos)
            BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} FILE|" << data << "| HAS /";
        else
            BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} FILE|" << data << "| NO SPACE, filesize: " << be64toh(param) << " free: " << fm.getFreeSpace(false);

        fileManagerMutex.unlock();
        SIMPL_CMD response;
        memset(&response, 0, sizeof(response));
        strcpy(response.cmd, "NO_WAY\0\0\0\0");
        response.cmd_seq = specialSeq;
        strcpy(response.data, data.c_str());

        ssize_t responseLength = data.size() + 18;
        auto sendLen = (socklen_t) sizeof(client_address);

        if(sendto(socket, &response, responseLength, 0, (struct sockaddr *)&client_address, sendLen) != responseLength) {
            BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} SEND FAILED";
            return;
        }
        return;
    }

    // First we need to create TCP socket then inform client about it

    CMPLX_CMD response;
    memset(&response, 0, sizeof(response));
    strcpy(response.cmd, "CAN_ADD\0\0\0");
    response.cmd_seq = specialSeq;
    ssize_t responseLength = 26;
    auto sendLen = (socklen_t) sizeof(client_address);

    // Setting up socket for TCP connection
    int tcpSocket = ::socket(AF_INET, SOCK_STREAM, 0);
    if (tcpSocket < 0){
        BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} Error creating TCP socket";
        fileManagerMutex.unlock();
        return;
    }

    struct sockaddr_in newTCP;
    memset(&newTCP, 0 , sizeof(newTCP));
    // assign IP, PORT
    newTCP.sin_family = AF_INET;
    newTCP.sin_addr.s_addr = htonl(INADDR_ANY);;
    newTCP.sin_port = 0;
    if ((bind(tcpSocket, (struct sockaddr *)&newTCP, sizeof(newTCP))) != 0) {
        BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} Error binding TCP socket for fetch";
        if (close(tcpSocket) < 0)
            BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} Error closing socket";
        fileManagerMutex.unlock();
        return;
    }
    // Started listing
    if ((listen(tcpSocket, 5)) != 0) {
        BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} Listen failed...";
        if (close(tcpSocket) < 0)
            BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} Error closing socket..";
        fileManagerMutex.unlock();
        return;
    }

    // Getting port to send client
    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getsockname(tcpSocket, (struct sockaddr *)&sin, &len) == -1) {
        BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} Error getting socket name..";
        if (close(tcpSocket) < 0)
            BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} Error closing socket...";
        fileManagerMutex.unlock();
        return;
    }

    response.param = htobe64(ntohs(sin.sin_port));

    // Create an empty file + reserve space has to be under mutex
    fm.createEmptyFileForUpdate(std::string(), be64toh(param));

    fileManagerMutex.unlock();

    if(sendto(socket, &response, responseLength, 0, (struct sockaddr *)&client_address, sendLen) != responseLength) {
        BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} Error sending CONNECT_ME...";
        if (close(tcpSocket) < 0)
            BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} Error closing socket...";
        fm.removeEmptyFileForUpdate(data, be64toh(param));
        return;
    }

    // Adding timeout
    fd_set set;
    FD_ZERO(&set); /* clear the set */
    FD_SET(tcpSocket, &set); /* add our file descriptor to the set */

    struct timeval timeout;
    timeout.tv_sec = TIMEOUT;
    timeout.tv_usec = 0;

    int rv;         // Select value
    int msg_sock;   // Socket for sending TCP packages
    rv = select(tcpSocket + 1, &set, NULL, NULL, &timeout);
    if(rv == -1) {
        BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} Error in select...";
        if (close(tcpSocket) < 0)
            BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} Error closing socket...";
        fm.removeEmptyFileForUpdate(data, be64toh(param));
        return;
    }
    else if(rv == 0) {
        BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} Listen timeout";
        if (close(tcpSocket) < 0)
            BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} Error closing socket...";
        fm.removeEmptyFileForUpdate(data, be64toh(param));
        return;
    }

    socklen_t client_address_len = sizeof(client_address);
    msg_sock = accept(tcpSocket, (struct sockaddr *) &client_address, &client_address_len);
    if(msg_sock < 0) {
        BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} Error accepting socket";
        if (close(tcpSocket) < 0)
            BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} Error closing socket...";
        fm.removeEmptyFileForUpdate(data, be64toh(param));
        return;
    }

    std::string filePath = fm.getFilePath(data);
    BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} Connection established ready to send data";

    if(!receiveFile(msg_sock, filePath, be64toh(param))){
        BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} Failed to download";
        fm.removeEmptyFileForUpdate(data, be64toh(param));
    } else {
        BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} Succesfully downloaded file";
        fileManagerMutex.lock();
        fm.availableFiles.insert(std::make_pair(data, fs::path(fm.getFilePath(data))));
        fileManagerMutex.unlock();
    }
}

// Send File through TCP
// True -> Sending was successful
// False -> there was an error
bool sendFile(int socketTCP, std::string data) {
    std::ifstream is(fm.getFilePath(data), std::ifstream::binary);

    if (!is) {
        BOOST_LOG_TRIVIAL(debug) << "{FETCH} File empty or sth";
        return false;
    }

    char buffer[server::bufferSize + 1];

    do {

        memset(&buffer, 0, server::bufferSize + 1);
        uint64_t toSendLgh = (uint32_t) is.read(buffer, server::bufferSize).gcount();

        if(!sendSomething((void *)buffer, socketTCP, toSendLgh)) {
            return false;
        }
    } while (is);


    return true;
}

// Send File through TCP
bool receiveFile(int TCPsocket, std::string filePath, uint64_t fileSize) {
    std::ofstream outfile(filePath, std::ofstream::binary);
    uint64_t readSoFar = 0;

    char buff[server::bufferSize + 1];

    uint32_t final;

    int success;
    do {
        memset(&buff, 0 , server::bufferSize + 1);
        success = readSomething(buff, TCPsocket, server::bufferSize, final);

        if (success == 1) {
            outfile.close();
            return false;
        }

        if(success == 0){
            readSoFar += server::bufferSize;
            outfile.write(buff, server::bufferSize);
        }
        else {
            readSoFar += final;
            outfile.write(buff, final);
        }
    } while(success == 0);

    outfile.close();

    if (readSoFar == fileSize) {
        return true;    //Sending was Successful
    } else {
        BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} Received length: " << readSoFar << " Expected: " << fileSize;
        return false;   //Sending was Unsuccessful
    }
}

// Sends data of length through socket
// True -> Sending was successful
// False -> there was an error
bool sendSomething(void *to_send, int socket, uint32_t length) {
    errno = 0;

    if (send(socket, to_send, length, MSG_NOSIGNAL) !=
        length) {
        if (errno == EPIPE) { // Sending on closed connection
            BOOST_LOG_TRIVIAL(debug) << "{FETCH} Connection lost while sending data";
            return false;
        }
        BOOST_LOG_TRIVIAL(debug) << "{FETCH} FailedPartialWrite";
        return false;
    }
    return true;
}

// Reads data of length from socket
// -1 -> Received info about close connection
// 0  -> Successfully read another chunk of data
// 1  -> Error reading occured
int readSomething(void *to_read, int socket, uint32_t length, uint32_t &final) {
    unsigned int prev_len = 0;
    unsigned int remains = 0;

    while (prev_len < length) {
        remains = length - prev_len;
        ssize_t len = read(socket, ((char *) to_read) + prev_len, remains);
        if (len < 0) {
            return 1;
        }

        if (len == 0) {
            final = prev_len;
            return -1;
        }

        prev_len += len;
        if (prev_len == length)
            return 0;
    }

    return 0;
}

// Return:
// -2 -> TIMEOUT
// -1 -> ERROR OCCURED
// 0 -> No match
// 1 -> DISCOVER
// 2 -> LIST
// 3 -> DEL
// 4 -> FETCH
int readCMD(uint64_t &specialSeq, uint64_t &param, std::string &data, struct sockaddr_in &client_address,
                   int &sock) {
    memset(&client_address, 0, sizeof(client_address));
    SIMPL_CMD messageSIMPLE;
    CMPLX_CMD messageCMPLX;

    auto rcvLen = (socklen_t) sizeof(client_address);
    memset(&messageCMPLX, 0, sizeof(messageCMPLX));
    ssize_t length = recvfrom(sock, &messageCMPLX, sizeof(messageCMPLX), 0,
                               (struct sockaddr *) &client_address, &rcvLen);

    if(length < 0){
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return -2;
        }
        BOOST_LOG_TRIVIAL(debug) << "{MATCH DATA} Recv From failed";
        return -1;
    }

    messageSIMPLE = *((SIMPL_CMD *) &messageCMPLX);
    specialSeq = messageCMPLX.cmd_seq;
    if (length == 18 && customStrCheck("HELLO\0\0\0\0\0", messageCMPLX.cmd)) {
        BOOST_LOG_TRIVIAL(debug) << "[HELLO] From: " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port);
        return 1;
    } else if(length >= 18 && customStrCheck("LIST\0\0\0\0\0\0", messageCMPLX.cmd)) {
        data = std::string(messageSIMPLE.data);
        BOOST_LOG_TRIVIAL(debug) << "[LIST] From: " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << ", Substring to look for: |" << data << "|length: " << data.length();
        return 2;
    } else if(length > 18 && customStrCheck("DEL\0\0\0\0\0\0\0", messageCMPLX.cmd)) {
        data = std::string(messageSIMPLE.data);
        BOOST_LOG_TRIVIAL(debug) << "[DEL] From: " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << ", File to delete: |" << data << "|length: " << data.length();
        return 3;
    } else if(length > 18 && customStrCheck("GET\0\0\0\0\0\0\0", messageCMPLX.cmd)) {
        data = std::string(messageSIMPLE.data);
        BOOST_LOG_TRIVIAL(debug) << "[GET] From: " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port);
        return 4;
    } else if(length > 26 && customStrCheck("ADD\0\0\0\0\0\0\0", messageCMPLX.cmd)) {
        data = std::string(messageCMPLX.data);
        param = messageCMPLX.param;
        BOOST_LOG_TRIVIAL(debug) << "[ADD] From: " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << ", File to add: |" << data << "|length: " << data.length();
        return 5;
    } else {
        outputMutex.lock();
        std::cerr << "[PCKG ERROR] Skipping invalid package from " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << ". Incorrect command or package length" << std::endl;
        outputMutex.unlock();
        return 0;
    }
}

// Give two strings and checks first 10 bytes of both
// True when C-strings are equal
bool customStrCheck(const char *tab, const char *str) {

    for(int i=0; i<10; i++) {
        if(tab[i] != str[i])
            return false;
    }
    return true;
}

uint64_t FileManager::getFreeSpace(bool takeMutex) {
    if(takeMutex)
        fileManagerMutex.lock_shared();
    if(isNegative){
        if(takeMutex)
            fileManagerMutex.unlock_shared();
        return 0;
    } else {
        if(takeMutex)
            fileManagerMutex.unlock_shared();
        return freeSpace;
    }
}

// Returns -1 when there was error removing a file or given file was in set but didn't exists in the folder (UNDEFINED)
// Returns 0 when there was no file to remove in availableFiles (IGNORE)
// Returns 1 otherwise and removes the file and filename from set (OK)
int FileManager::removeFile(std::string &fileName) {
    fileManagerMutex.lock();
    if(availableFiles.find(fileName) == availableFiles.end()){
        fileManagerMutex.unlock();
        return 0;
    }


    fs::path filePath = availableFiles.find(fileName)->second;
    availableFiles.erase(fileName);

    if(!fs::exists(filePath) || !fs::is_regular(filePath)){
        fileManagerMutex.unlock();
        return -1;
    }

    uint64_t fileSize = fs::file_size(filePath);

    if(!isNegative) {
        freeSpace += fileSize;
    } else {
        if(fileSize > freeSpace) {
            isNegative = false;
            freeSpace = fileSize - freeSpace;
        } else {
            freeSpace -= fileSize;
        }
    }

    if(fs::remove(filePath)) {
        fileManagerMutex.unlock();
        return 1;
    } else {
        fileManagerMutex.unlock();
        return -1;
    }
}

// Return true when file exists
bool FileManager::fileExists(std::string &fileName, bool takeMutex) {
    if(takeMutex)
        fileManagerMutex.lock_shared();
    if(availableFiles.find(fileName) != availableFiles.end()) {
        if(takeMutex)
            fileManagerMutex.unlock_shared();
        return true;
    }
    if(takeMutex)
        fileManagerMutex.unlock_shared();
    return false;
}

void FileManager::initalizeAll(std::string &SHRD_FLDR) {
    p = fs::path(SHRD_FLDR);
    folderPath = fs::path(SHRD_FLDR);
    freeSpace = MAX_SPACE;
    isNegative = false;

    if(!fs::is_directory(p))
        syserr("Bad folder path");

    fs::directory_iterator it(p), eod;

    BOOST_FOREACH(fs::path const &p, std::make_pair(it, eod))
                {
                    if(fs::is_regular_file(p))
                    {
                        availableFiles.insert(std::make_pair(p.filename().string(), p));
                        if(!isNegative) {
                            if(fs::file_size(p) <= freeSpace) {
                                freeSpace -= (uint64_t) fs::file_size(p);
                            } else {
                                isNegative = true;
                                freeSpace = (uint64_t) fs::file_size(p) - freeSpace;
                            }
                        } else {
                            freeSpace += fs::file_size(p);
                        }
                    }
                }
    BOOST_LOG_TRIVIAL(debug) << "MAXSPACE: " << MAX_SPACE  << " Freespace: " << freeSpace <<  " isNegative: " << isNegative;
}

std::string FileManager::getFilePath(std::string filePath) {
    std::string fp = folderPath.string();
    fp += "/" + filePath;
    return fp;
}

// Is protected wit mutex (unshared by default)
void FileManager::createEmptyFileForUpdate(std::string fileName, uint64_t fileSize) {
    std::ofstream output(folderPath.string() + "/" + fileName);
    output.close();
    freeSpace -= fileSize;
}

// Has to protect itself
void FileManager::removeEmptyFileForUpdate(std::string fileName, uint64_t fileSize) {
    fileManagerMutex.lock();
    fs::remove(fs::path(folderPath.string() + "/" + fileName));
    freeSpace += fileSize;
    fileManagerMutex.unlock();
}

// Initialize main UDP socket
void initializeMainUDPSocket(int &sock) {

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        syserr("socket");

    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        syserr("setsockopt(SO_REUSEADDR) failed");

    struct ip_mreq ip_mreq;
    const char *multicast_dotted_address = MCAST_ADDR.c_str();

    ip_mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (inet_aton(multicast_dotted_address, &ip_mreq.imr_multiaddr) == 0)
        syserr("inet_aton");
    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *) &ip_mreq,
                   sizeof ip_mreq) < 0)
        syserr("setsockopt");

    struct timeval tv;
    tv.tv_sec = TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        syserr("Error setting main socket timeout");
    }

    /* podpięcie się pod lokalny adres i port */
    group_address.sin_family = AF_INET;
    group_address.sin_addr.s_addr = htonl(INADDR_ANY);
    group_address.sin_port = htons(CMD_PORT);

    if (bind(sock, (struct sockaddr *) &group_address, sizeof group_address) <
        0)
        syserr("bind");
}

// Parses options for the program 1 when successful -1 otherwise
int parseOptions(int argc, char *argv[]) {

    int succesfull = 1;
    // Declare the supported options.
    po::options_description desc("Allowed options");
    desc.add_options()
            (",g", po::value<std::string>()->required(), "MCAST_ADDR")
            (",p", po::value<uint16_t>()->required(), "CMD_PORT")
            (",b", po::value<uint64_t>()->default_value(server::DEFAULT_SPACE),
             "MAX_SPACE")
            (",f", po::value<std::string>()->required(), "SHRD_FLDR")
            (",t", po::value<int>()->default_value(server::DEFAULT_TIMEOUT),
             "TIMEOUT")
            (",help", "produce help message");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("-help")) {
        std::cout << desc << "\n";
        return 0;
    }

    if (vm.count("-g")) {
        BOOST_LOG_TRIVIAL(debug) << "MCAST_ADDR was set to "
                                 << vm["-g"].as<std::string>();
        MCAST_ADDR = vm["-g"].as<std::string>();
    } else {
        std::cerr << "MCAST_ADDR was not set." << std::endl;
        succesfull = -1;
    }

    if (vm.count("-p")) {
        BOOST_LOG_TRIVIAL(debug) << "CMD_PORT was set to "
                                 << vm["-p"].as<uint16_t>();
        CMD_PORT = vm["-p"].as<uint16_t>();
    } else {
        std::cerr << "CMD_PORT was not set." << std::endl;
        succesfull = -1;
    }

    if (vm.count("-b")) {
        BOOST_LOG_TRIVIAL(debug) << "MAX_SPACE was set to "
                                 << vm["-b"].as<uint64_t>();
        MAX_SPACE = vm["-b"].as<uint64_t>();
    } else {
        std::cerr << "MAX_SPACE was not set." << std::endl;
        succesfull = -1;
    }

    if (vm.count("-f")) {
        BOOST_LOG_TRIVIAL(debug) << "SHRD_FLDR was set to "
                                 << vm["-f"].as<std::string>();
        SHRD_FLDR = vm["-f"].as<std::string>();
    } else {
        std::cerr << "SHRD_FLDR was not set.\n";
        succesfull = -1;
    }

    if (vm.count("-t")) {
        BOOST_LOG_TRIVIAL(debug) << "TIMEOUT was set to "
                                 << vm["-t"].as<int>();
        TIMEOUT = vm["-t"].as<int>();
        if(TIMEOUT < 0 || TIMEOUT > 300){
            succesfull = -1;
            std::cerr << "Bad timeout value" << std::endl;
        }
    } else {
        std::cerr << "TIMEOUT was not set." << std::endl;
        succesfull = -1;
    }

    return succesfull;
}

void syserr(const char *fmt, ...) {
    va_list fmt_args;

    fprintf(stderr, "ERROR: ");
    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);
    fprintf(stderr, " (%d; %s)\n", errno, strerror(errno));
    exit(2);
}