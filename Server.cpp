#include "Server.h"

// Variables
static uint16_t CMD_PORT;
static uint64_t MAX_SPACE;
static std::string MCAST_ADDR;
static int TIMEOUT;
static std::string SHRD_FLDR;

static struct sockaddr_in group_address;


int main(int argc, char *argv[]) {
    int socket;
    uint64_t specialSeq;
    struct sockaddr_in client_address;
    SIMPL_CMD message;

    if(parseOptions(argc, argv) != 1) {
        std::cerr << "Failed parsing options\n";
        exit(1);
    }

    initializeMainUDPSocket(socket);

    // Initial file management
    FileManager fm = FileManager(SHRD_FLDR);

    // String received from udp
    std::string data;

    while (true) {
        for_loop:
        int cmd = readCMD(specialSeq, data, client_address, socket);

        switch (cmd) {
            case -2: {
                // TIMEOUT
                break;
            }
            case -1: {
                std::cout << "Non critical error occured\n";
                break;
            }
            case 0: {
                std::cout << "Nothing matched send message" << std::endl;
                break;
            }
            case 1: {
                respondDiscover(socket, specialSeq, fm.getFreeSpace(), client_address);

                break;
            }
            case 2: {
                respondSearch(socket, fm, data, specialSeq, client_address);
                break;
            }
            case 3: {
                respondRemove(fm, data, client_address);
                break;
            }
            case 4: {

                respondFetch(fm, data, specialSeq, client_address);
                break;
            }
            default: {
                //std::cout << "XDDDDD" << std::endl;
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

    size_t responseLength = MCAST_ADDR.size() + 26;
    auto sendLen = (socklen_t) sizeof(client_address);
    if(sendto(socket, &response, responseLength, 0, (struct sockaddr *)&client_address, sendLen) != responseLength)
        std::cerr << "Bad write for discover\n";
}

void respondSearch(int socket, FileManager &fm, std::string &data, uint64_t specialSeq, struct sockaddr_in &client_address) {
    SIMPL_CMD response;
    uint64_t leftInBuffer = server::MAX_SEND;
    uint64_t position = 0;
    bool someFile = false;

    memset(&response, 0, sizeof(response));
    auto sendLen = (socklen_t) sizeof(client_address);
    for(auto &it : fm.availableFiles) {
        std::string str = it.first;

        if(!data.empty() && str.find(data) == std::string::npos)
            continue;

        if(leftInBuffer < str.length()) {
            strcpy((response.data + position), str.c_str());
            position += str.length() + 19;
            strcpy(response.cmd, "MY_LIST\0\0\0");
            response.cmd_seq = specialSeq;

            if(sendto(socket, &response, position, 0, (struct sockaddr *)&client_address, sendLen) != position)
                std::cerr << "Bad write for search\n";
            if(server::debug_ON)
                std::cout << "[MY_LIST] To: " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << " length: " << position << std::endl;

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

    if(someFile) {
        response.data[position - 1] = '\0';
        strcpy(response.cmd, "MY_LIST\0\0\0");
        response.cmd_seq = specialSeq;

        position += 18;
        if(sendto(socket, &response, position, 0, (struct sockaddr *)&client_address, sendLen) != position)
            std::cerr << "Bad write for search\n";

        if(server::debug_ON)
            std::cout << "[MY_LIST] To: " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << " length: " << position << std::endl;
    }
}

// Handle remove request
void respondRemove(FileManager &fm, std::string &data, struct sockaddr_in &client_address) {
    if(server::debug_ON)
        std::cout << "[DEL] From: " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << " file|" << data << "|" << std::endl;


    int x = fm.removeFile(data);

    if(x == -1) {
        if(server::debug_ON)
            std::cerr << "Undefined behaviour removing a file" << std::endl;
    } else if(x == 0) {
        if(server::debug_ON)
            std::cerr << "File was not in the set of available files" << std::endl;
    } else {
        if(server::debug_ON)
            std::cout << "Removal was successful" << std::endl;
    }
}

// Handle fetch request
void respondFetch(FileManager &fm, std::string &data, uint64_t specialSeq, struct sockaddr_in &client_address) {

    if(!fm.fileExists(data)) {
        std::cerr << "[PCKG ERROR] Skipping invalid package from " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << ". Fetched file did not exist" << std::endl;
        return;
    }

    std::cout << "FILE " << data << "EXISTS" << std::endl;

    // Teraz trzeba wysłać info z powrotem otworzyć połączenie
    // Czekamy TIMEOUT i kończymy

}

// Return:
// -2 -> TIMEOUT
// -1 -> ERROR OCCURED
// 0 -> No match
// 1 -> DISCOVER
// 2 -> LIST
// 3 -> DEL
// 4 -> FETCH
int readCMD(uint64_t &specialSeq, std::string &data, struct sockaddr_in &client_address,
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
        std::cerr << "error receiving data" << std::endl;
        return -1;
    }

    messageSIMPLE = *((SIMPL_CMD *) &messageCMPLX);
    specialSeq = messageCMPLX.cmd_seq;
    if (length == 18 && customStrCheck("HELLO\0\0\0\0\0", messageCMPLX.cmd)) {
        if(server::debug_ON)
            std::cout << "[HELLO] From: " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << std::endl;
        return 1;
    } else if(length >= 18 && customStrCheck("LIST\0\0\0\0\0\0", messageCMPLX.cmd)) {
        data = std::string(messageSIMPLE.data);
        if(server::debug_ON) {
            std::cout << "[LIST] From: " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << std::endl;
            std::cout << "Substring to look for: |" << data << "|length: " << data.length() << std::endl;
        }
        return 2;
    } else if(length > 18 && customStrCheck("DEL\0\0\0\0\0\0\0", messageCMPLX.cmd)) {
        data = std::string(messageSIMPLE.data);
        if(server::debug_ON) {
            std::cout << "[DEL] From: " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << std::endl;
            std::cout << "File to delete: |" << data << "|length: " << data.length() << std::endl;
        }
        return 3;
    } else if(length > 18 && customStrCheck("GET\0\0\0\0\0\0\0", messageCMPLX.cmd)) {
        data = std::string(messageSIMPLE.data);
                if(server::debug_ON) {
            std::cout << "[GET] From: " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << std::endl;
            std::cout << "File to fetch: |" << data << "|length: " << data.length() << std::endl;
        }
        return 4;
    } else {
        std::cerr << "[PCKG ERROR] Skipping invalid package from " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << ". Incorrect command or package length" << std::endl;
        return 0;
    }

//    REMEMBER TO MEMSET IT AS WELL !!!
//    messageSIMPLE = *((SIMPL_CMD *) &messageCMPLX);
//    std::cout << "length " << length << " message.cmd " << messageSIMPLE.cmd << " cmd_seq: " << be64toh(messageSIMPLE.cmd_seq) << std::endl;
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

FileManager::FileManager(std::string &SHRD_FLDR) : freeSpace(MAX_SPACE), isNegative(false) {
    p = fs::path(SHRD_FLDR);
    folderPath = fs::path(SHRD_FLDR);

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
    std::cout << "MAXSPACE: " << MAX_SPACE  << " Freespace: " << freeSpace <<  " isNegative: " << isNegative << std::endl;
}

uint64_t FileManager::getFreeSpace() {
    if(isNegative)
        return 0;
    else
        return freeSpace;
}

// Returns -1 when there was error removing a file or given file was in set but didn't exists in the folder (UNDEFINED)
// Returns 0 when there was no file to remove in availableFiles (IGNORE)
// Returns 1 otherwise and removes the file and filename from set (OK)
int FileManager::removeFile(std::string &fileName) {
    if(availableFiles.find(fileName) == availableFiles.end())
        return 0;

    fs::path filePath = availableFiles.find(fileName)->second;
    availableFiles.erase(fileName);

    if(!fs::exists(filePath) || !fs::is_regular(filePath)){
        printf("asd\n");
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

    if(fs::remove(filePath))
        return 1;
    else
        return -1;
}

bool FileManager::fileExists(std::string &fileName) {
    fs::directory_iterator end_itr;

    for (fs::directory_iterator itr(fileName); itr != end_itr; ++itr) {
        if (is_regular_file(itr->path()) && itr->path().filename().compare(fileName)) {
            return true;
        }
    }
    return false;
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
        if (server::debug_ON)
            std::cout << "MCAST_ADDR was set to "
                      << vm["-g"].as<std::string>() << ".\n";
        MCAST_ADDR = vm["-g"].as<std::string>();
    } else {
        std::cout << "MCAST_ADDR was not set.\n";
        succesfull = -1;
    }

    if (vm.count("-p")) {
        if (server::debug_ON)
            std::cout << "CMD_PORT was set to "
                      << vm["-p"].as<uint16_t>() << ".\n";
        CMD_PORT = vm["-p"].as<uint16_t>();
    } else {
        std::cout << "CMD_PORT was not set.\n";
        succesfull = -1;
    }

    if (vm.count("-b")) {
        if (server::debug_ON)
            std::cout << "MAX_SPACE was set to "
                      << vm["-b"].as<uint64_t>() << ".\n";
        MAX_SPACE = vm["-b"].as<uint64_t>();
    } else {
        std::cout << "MAX_SPACE was not set.\n";
        succesfull = -1;
    }

    if (vm.count("-f")) {
        if (server::debug_ON)
            std::cout << "SHRD_FLDR was set to "
                      << vm["-f"].as<std::string>() << ".\n";
        SHRD_FLDR = vm["-f"].as<std::string>();
    } else {
        std::cout << "SHRD_FLDR was not set.\n";
        succesfull = -1;
    }

    if (vm.count("-t")) {
        if (server::debug_ON)
            std::cout << "TIMEOUT was set to "
                      << vm["-t"].as<int>() << ".\n";
        TIMEOUT = vm["-t"].as<int>();
        if(TIMEOUT < 0 || TIMEOUT > 300){
            succesfull = -1;

            std::cerr << "Bad timeout value" << std::endl;
        }
    } else {
        std::cout << "TIMEOUT was not set.\n";
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

void fatal(const char *fmt, ...) {
    va_list fmt_args;

    fprintf(stderr, "ERROR: ");
    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);
    fprintf(stderr, "\n");
    exit(1);
}
