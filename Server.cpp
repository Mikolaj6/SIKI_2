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

    parseOptions(argc, argv);
    initializeUDPSocket(socket);

    uint64_t freeSpace = MAX_SPACE;

    while (true) {
        for_loop:
        int cmd = readCMD(specialSeq, client_address, socket);

        switch (cmd) {
            case 1: {
                if(server::debug_ON) {
                    std::cout << "Received listing request" << std::endl;
                }

                CMPLX_CMD response;
                memset(&response, 0, sizeof(response));
                strcpy(response.cmd, "GOOD DAY\0\0");
                response.cmd_seq = htobe64(specialSeq);
                response.param = htobe64(freeSpace);
                strcpy(response.data, MCAST_ADDR.c_str());

                size_t responseLength = MCAST_ADDR.size() + 26;
                auto sendLen = (socklen_t) sizeof(client_address);
                if(sendto(socket, &response, responseLength, 0, (struct sockaddr *)&client_address, sendLen) != responseLength) {
                    std::cerr << "Bad write for discover\n";
                    goto for_loop;
                }
                break;
            }
            default: {
                std::cout << "XDDDDD" << std::endl;
                break;
            }
        }
    }

    return 0;
}

// Return:
// -2 -> TIMEOUT
// -1 -> ERROR OCCURED
// 1 -> list all
int readCMD(uint64_t &specialSeq, struct sockaddr_in &client_address,
                   int &sock) {
    memset(&client_address, 0, sizeof(client_address));
    SIMPL_CMD messageSIMPLE;
    CMPLX_CMD messageCMPLX;

    auto rcvLen = (socklen_t) sizeof(client_address);

    ssize_t length = recvfrom(sock, &messageCMPLX, sizeof(messageCMPLX), 0,
                               (struct sockaddr *) &client_address, &rcvLen);

    if(length < 0){
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            printf("TIMEOUT\n");
            return -2;
        }
        std::cerr << "error receiving data";
    }

    specialSeq = messageCMPLX.cmd_seq;
    if (strcmp("HELLO", messageCMPLX.cmd) == 0) {
        return 1;
    }

    messageSIMPLE = *((SIMPL_CMD *) &messageCMPLX);
    std::cout << "length " << length << " message.cmd " << messageSIMPLE.cmd << " cmd_seq: " << be64toh(messageSIMPLE.cmd_seq) << std::endl;
    return 2;
}

int initializeUDPSocket(int &sock) {

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
