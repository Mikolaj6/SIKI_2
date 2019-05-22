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

    // --------- Check if was successful
    parseOptions(argc, argv);

    initializeUDPSocket(UDP_sock);


    std::string line;

    while (std::getline (std::cin, line)) {

        int x = parseLine(line);

        if(x == 1) {
            std::cout << "DISCOVER" << std::endl;
            do_discover(UDP_sock);

            continue;
        }

        if (x == -1) {
            std::cout << "ERROR" << std::endl;
            break;
        }

        if (x == 0) {
            std::cout << "EXIT" << std::endl;
            break;
        }
    }

    if(client::debug_ON)
        std::cout << "PROGRAM IS DONE" << std::endl;

    return 0;
}

void do_discover(int &sock) {
    auto sendLen = (socklen_t) sizeof(group_address);

    SIMPL_CMD bla;
    strcpy (bla.cmd, "HELLO\0\0\0\0\0");
    // -------------NEED TO SET GOOD CMD SEQ
    bla.cmd_seq = htobe64((uint64_t) 1);

    struct timeval tv;
    tv.tv_sec = TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        syserr("Error");
    }

    if(sendto(sock, &bla, 18, 0, (struct sockaddr *)&group_address, sendLen) != 18)
        syserr("Bad write for discover");

    struct sockaddr_in server_address;
    CMPLX_CMD reply;
    auto rcvLen = (socklen_t) sizeof(server_address);





    auto start = std::chrono::system_clock::now();
    // Some computation here
    auto end = std::chrono::system_clock::now();

    std::chrono::duration<double> elapsed_seconds = end-start;
    std::time_t end_time = std::chrono::system_clock::to_time_t(end);

    std::cout << "finished computation at " << std::ctime(&end_time)
              << "elapsed time: " << elapsed_seconds.count() << "s\n";


    while(true) {



        memset(&server_address, 0, sizeof(server_address));
        ssize_t length = recvfrom(sock, &reply, sizeof(reply), 0,
                                  (struct sockaddr *) &server_address, &rcvLen);




        // -------------NEED TO SKIP AND CHECK CMD SEQUENCE
        if(strcmp(reply.cmd, "GOOD DAY") != 0 || reply.cmd_seq != 1)
            std::cout << "BAD sequence\n";


        std::cout << "Found " << inet_ntoa(server_address.sin_addr) << " (" << reply.data << ") with free space " << be64toh(reply.param) << std::endl;

        break;
    }


}

int initializeUDPSocket(int &sock) {
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

    group_address.sin_family = AF_INET;
    group_address.sin_port = htons(CMD_PORT);
    if (inet_aton(MCAST_ADDR.c_str(), &group_address.sin_addr) == 0)
        syserr("inet_aton");
}

int parseLine(std::string &line) {

    if (line.compare("discover") == 0) {
        return client::DISCOVER;
    } else if (line.compare("exit") == 0) {
        return client::EXIT;
    } else {
        return client::ERROR;
    }
}

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
        std::cout << desc << "\n";
        return 0;
    }

    if (vm.count("-g")) {
        if(client::debug_ON)
            std::cout << "MCAST_ADDR was set to "
                  << vm["-g"].as<std::string>() << ".\n";
        MCAST_ADDR = vm["-g"].as<std::string>();
    } else {
        std::cout << "MCAST_ADDR was not set.\n";
        succesfull = -1;
    }

    if (vm.count("-p")) {
        if(client::debug_ON)
            std::cout << "CMD_PORT was set to "
                  << vm["-p"].as<uint16_t>() << ".\n";
        CMD_PORT = vm["-p"].as<uint16_t>();
    } else {
        std::cout << "CMD_PORT was not set.\n";
        succesfull = -1;
    }

    if (vm.count("-o")) {
        if(client::debug_ON)
            std::cout << "OUT_FLDR was set to "
                      << vm["-o"].as<std::string>() << ".\n";
        OUT_FLDR = vm["-o"].as<std::string>();
    } else {
        std::cout << "OUT_FLDR was not set.\n";
        succesfull = -1;
    }

    if (vm.count("-t")) {
        if(client::debug_ON)
            std::cout << "TIMEOUT was set to "
                      << vm["-t"].as<int>() << ".\n";
        TIMEOUT = vm["-t"].as<int>();
    } else {
        std::cout << "TIMEOUT was not set.\n";
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