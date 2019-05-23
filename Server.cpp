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

    initializeUDPSocket(socket);
    uint64_t freeSpace = MAX_SPACE;

    while (true) {
        for_loop:
        int cmd = readCMD(specialSeq, client_address, socket);

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
                std::cout << "Nothing matched send message\n";
                break;
            }
            case 1: {
                respondDiscover(socket, specialSeq, freeSpace, client_address);

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
    if(server::debug_ON)
        std::cout << "Received discovery request" << std::endl;

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

// Return:
// -2 -> TIMEOUT
// -1 -> ERROR OCCURED
// 0 -> No match
// 1 -> DISCOVER
int readCMD(uint64_t &specialSeq, struct sockaddr_in &client_address,
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
        std::cerr << "error receiving data";
        return -1;
    }

    specialSeq = messageCMPLX.cmd_seq;
    if (length == 18 && customStrCheck("HELLO\0\0\0\0\0", messageCMPLX.cmd)) {
        return 1;
    } else {
        return 0;
    }

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

// Initialize main UDP socket
void initializeUDPSocket(int &sock) {

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
            (",g", po::value<std::string>(), "MCAST_ADDR")
            (",p", po::value<uint16_t>(), "CMD_PORT")
            (",b", po::value<uint64_t>()->default_value(server::DEFAULT_SPACE),
             "MAX_SPACE")
            (",f", po::value<std::string>(), "SHRD_FLDR")
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
