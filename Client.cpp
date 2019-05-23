#include "Client.h"

// variables
static uint16_t CMD_PORT;
static std::string MCAST_ADDR;
static int TIMEOUT;
static std::string OUT_FLDR;
static struct sockaddr_in group_address;

int main(int argc, char *argv[])
{
    int UDP_sock;

    if(parseOptions(argc, argv) != 1)
        syserr("Some options were not set");

    initializeUDPSocket(UDP_sock);
    std::string line;

    while (std::getline (std::cin, line)) {

        int x = parseLine(line);

        if(x == 1) {
            if(client::debug_ON)
                std::cout << "DISCOVER" << std::endl;
            do_discover(UDP_sock);
            continue;
        }

        if (x == -1) {
            if(client::debug_ON)
                std::cout << "ERROR" << std::endl;
            break;
        }

        if (x == 0) {
            if(client::debug_ON)
                std::cout << "EXIT" << std::endl;
            break;
        }
    }

    if(client::debug_ON)
        std::cout << "PROGRAM IS DONE" << std::endl;

    return 0;
}

// Function for discover
void do_discover(int &sock) {
    auto sendLen = (socklen_t) sizeof(group_address);

    SIMPL_CMD bla;
    strcpy (bla.cmd, "HELLO\0\0\0\0\0");

    std::uniform_int_distribution<uint64_t > dis;
    std::random_device rd;
    std::mt19937 gen(rd());
    uint64_t specialSeq = dis(gen);
    bla.cmd_seq = htobe64((uint64_t) specialSeq);

    if(sendto(sock, &bla, 18, 0, (struct sockaddr *)&group_address, sendLen) != 18)
        syserr("Bad write for discover");

    struct sockaddr_in server_address;
    CMPLX_CMD reply;
    auto rcvLen = (socklen_t) sizeof(server_address);

    setTimeout(sock, 0, 100);
    auto start = std::chrono::system_clock::now();

    while(true) {
        auto curr = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed_seconds = curr-start;

        if(elapsed_seconds.count() > TIMEOUT) {
            setTimeout(sock, TIMEOUT, 0);
            if(client::debug_ON)
                std::cout << "Finished waiting" << std::endl;
            break;
        }

        memset(&server_address, 0, sizeof(server_address));
        ssize_t length = recvfrom(sock, &reply, sizeof(reply), 0,
                                  (struct sockaddr *) &server_address, &rcvLen);
        if(length < 0){
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            syserr("Error reading GOOD DAY from server");
        }

        if(length < 26)
            printSkipping(ntohs(server_address.sin_port), inet_ntoa(server_address.sin_addr), 2);
        else if(!customStrCheck(reply.cmd, "GOOD_DAY\0\0"))
            printSkipping(ntohs(server_address.sin_port), inet_ntoa(server_address.sin_addr), 0);
        else if(be64toh(reply.cmd_seq) != specialSeq)
            printSkipping(ntohs(server_address.sin_port), inet_ntoa(server_address.sin_addr), 1);
        else
            std::cout << "Found " << inet_ntoa(server_address.sin_addr) << " (" << reply.data << ") with free space " << be64toh(reply.param) << std::endl;
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

// Prints skipping for client
void printSkipping(uint16_t badPort, std::string badAddress, int messageType) {
    // Bad cmd field
    if(messageType == 0)
        std::cerr << "[PCKG ERROR] Skipping invalid package " << badAddress << ":" << badPort << ". Bad cmd field" << std::endl;

    // Bad cmd sequence
    if(messageType == 1)
        std::cerr << "[PCKG ERROR] Skipping invalid package " << badAddress << ":" << badPort << ". Bad cmd sequence" << std::endl;

    // Bad package length
    if(messageType == 2)
        std::cerr << "[PCKG ERROR] Skipping invalid package " << badAddress << ":" << badPort << ". Bad package length" << std::endl;
}

// Sets timeout with given seconds and micro seconds
void setTimeout(int sock, time_t sec, suseconds_t micro) {
    struct timeval tv;
    tv.tv_sec = sec;
    tv.tv_usec = micro;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        syserr("Setting timeout failed");
}

// Initializes main UDP socket
void initializeUDPSocket(int &sock) {
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        syserr("socket");

    int optval = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (void*)&optval, sizeof optval) < 0)
        syserr("setsockopt broadcast");

    /* ustawienie TTL dla datagramów rozsyłanych do grupy */
    optval = client::TTL_VALUE;
    if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (void*)&optval, sizeof optval) < 0)
        syserr("setsockopt multicast ttl");

    setTimeout(sock, TIMEOUT, 0);

    group_address.sin_family = AF_INET;
    group_address.sin_port = htons(CMD_PORT);
    if (inet_aton(MCAST_ADDR.c_str(), &group_address.sin_addr) == 0)
        syserr("inet_aton");
}

// Checks what client types and returns code representing typed command
int parseLine(std::string &line) {
    std::string lineLowercase = line;
    std::transform(lineLowercase.begin(), lineLowercase.end(), lineLowercase.begin(), ::tolower);

    if (lineLowercase.find("discover") != std::string::npos) {

        return client::DISCOVER;
    } else if (lineLowercase.find("exit") == std::string::npos) {
        return client::EXIT;
    } else {
        return client::ERROR;
    }
}


// Tests if line starts with a command and returns rest of the line when appropriate flag is set
// -1 - requires something for rest
// 0 - something can be in rest
// 1 - requires nothing for rest
bool testLine(std::string &line, std::string &command, std::string &rest, int flag) {

    rest = "";

    if(line.compare(0, command.length(), command) != 0)
        return false;

    rest = line.substr(command.length());
    std::cout << "|" << rest << "|" << std::endl;


    std::wstring::size_type firstPos = rest.find_first_not_of(' ');
    std::wstring::size_type lastPos = rest.find_last_not_of(' ');

    if()


    command.length();




}

// 1 if options were set correctly, -1 otherwise
int parseOptions(int argc, char *argv[]) {

    int succesfull = 1;
    // Declare the supported options.
    po::options_description desc("Allowed options");
    desc.add_options()
            (",g", po::value<std::string>(), "MCAST_ADDR")
            (",p", po::value<uint16_t>(), "CMD_PORT")
            (",o", po::value<std::string>(), "OUT_FLDR")
            (",t", po::value<int>()->default_value(client::DEFAULT_TIMEOUT), "TIMEOUT")
            (",help", "produce help message")
            ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("-help")) {
        std::cerr << desc << std::endl;
        return 0;
    }

    if (vm.count("-g")) {
        if(client::debug_ON)
            std::cout << "MCAST_ADDR was set to "
                  << vm["-g"].as<std::string>() << std::endl;
        MCAST_ADDR = vm["-g"].as<std::string>();
    } else {
        std::cerr << "MCAST_ADDR was not set." << std::endl;
        succesfull = -1;
    }

    if (vm.count("-p")) {
        if(client::debug_ON)
            std::cout << "CMD_PORT was set to "
                  << vm["-p"].as<uint16_t>() << std::endl;
        CMD_PORT = vm["-p"].as<uint16_t>();
    } else {
        std::cerr << "CMD_PORT was not set." << std::endl;
        succesfull = -1;
    }

    if (vm.count("-o")) {
        if(client::debug_ON)
            std::cout << "OUT_FLDR was set to "
                      << vm["-o"].as<std::string>() << "." << std::endl;
        OUT_FLDR = vm["-o"].as<std::string>();
    } else {
        std::cerr << "OUT_FLDR was not set." << std::endl;
        succesfull = -1;
    }

    if (vm.count("-t")) {
        if(client::debug_ON)
            std::cout << "TIMEOUT was set to "
                      << vm["-t"].as<int>() << "." << std::endl;
        TIMEOUT = vm["-t"].as<int>();
        if(TIMEOUT < 0 || TIMEOUT > 300){
            succesfull = -1;
            syserr("Bad timeout value");
        }
    } else {
        std::cerr << "TIMEOUT was not set." << std::endl;
        succesfull = -1;
    }

    return succesfull;
}

void syserr(const char *fmt, ...)
{
    va_list fmt_args;

    fprintf(stderr, "ERROR: ");
    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);
    fprintf(stderr, " (%d; %s)\n", errno, strerror(errno));
    exit(2);
}

void fatal(const char *fmt, ...)
{
    va_list fmt_args;

    fprintf(stderr, "ERROR: ");
    va_start(fmt_args, fmt);
    vfprintf(stderr, fmt, fmt_args);
    va_end(fmt_args);
    fprintf(stderr, "\n");
    exit(1);
}