#include "Client.h"

// variables
static uint16_t CMD_PORT;
static std::string MCAST_ADDR;
static int TIMEOUT;
static std::string OUT_FLDR;
static struct sockaddr_in group_address;

static std::map<std::string, std::unordered_set<std::string>> forFetch;


int main(int argc, char *argv[])
{
    int UDP_sock;

    uint64_tGen gen;

    if(parseOptions(argc, argv) != 1)
        syserr("Some options were not set");

    // Initialize main UDP Socket
    initializeUDPBroadcastSocketClient(UDP_sock);
    group_address.sin_family = AF_INET;
    group_address.sin_port = htons(CMD_PORT);
    if (inet_aton(MCAST_ADDR.c_str(), &group_address.sin_addr) == 0)
        syserr("inet_aton");

    // For reading client input
    std::string line;
    std::string rest;

    // For managing files
    fs::path p;
    check_folder(p);

    while (std::getline (std::cin, line)) {

        int x = parseLine(line, rest);

        if(x == client::DISCOVER) {
            do_discover(UDP_sock, gen);
            continue;
        }

        if (x == client::ERROR) {
            if(client::debug_ON)
                std::cerr << "Incorrect line" << std::endl;
            continue;
        }

        if (x == client::SEARCH) {
            do_search(UDP_sock, gen, rest);

            continue;
        }

        if (x == client::DELETE) {
            std::cout << "DELETE callled" << std::endl;
            do_remove(UDP_sock, gen, rest);
            continue;
        }

        if (x == client::FETCH) {
//            do_search(UDP_sock, gen, rest, forFetch);
//            std::cout << "FETCH called" << std::endl;
            std::thread(do_fetch, std::ref(gen), std::ref(rest)).detach();

            continue;
        }

        if (x == client::EXIT) {
            break;
        }
    }

    if(client::debug_ON)
        std::cout << "PROGRAM IS DONE" << std::endl;

    return 0;
}

// Function for initial handling of folder
void check_folder(fs::path &p) {
    p = fs::path(OUT_FLDR);

    if(!fs::is_directory(p))
        syserr("Bad folder path");
}

// Function for fetch
void do_fetch(uint64_tGen &gen, std::string &rest) {

    std::unordered_set<std::string> *setPtr;

    if(forFetch.find(rest) == forFetch.end()) {
        std::cout << "Not recently in searched" << std::endl;
        return;
    } else {
        setPtr = &forFetch.find(rest)->second;
    }

    int sock;
    normalUDPSocketClient(sock);

    auto item = setPtr->begin();
    uint64_t random_pos = gen.genNum() % setPtr->size();
    std::advance(item, random_pos);

    struct sockaddr_in serverSend;
    serverSend.sin_family = AF_INET;
    serverSend.sin_port = 0;
    if (inet_aton(item->c_str(), &serverSend.sin_addr) == 0)
        syserr("inet_aton");

    auto sendLen = (socklen_t) sizeof(serverSend);
    SIMPL_CMD request;
    memset(&request, 0, sizeof(request));
    strcpy (request.cmd, "GET\0\0\0\0\0\0\0");

    uint64_t specialSeq = gen.genNum();
    request.cmd_seq = htobe64((uint64_t) specialSeq);
    strcpy (request.data, rest.c_str());

    if(sendto(sock, &request, 19 + rest.size(), 0, (struct sockaddr *)&serverSend, sendLen) != (19 + rest.size()))
        syserr("Bad write for remove");


}

// Function for removal
void do_remove(int &sock, uint64_tGen &gen, std::string &rest) {
    auto sendLen = (socklen_t) sizeof(group_address);

    SIMPL_CMD request;
    memset(&request, 0, sizeof(request));
    strcpy (request.cmd, "DEL\0\0\0\0\0\0\0");

    uint64_t specialSeq = gen.genNum();
    request.cmd_seq = htobe64((uint64_t) specialSeq);
    strcpy (request.data, rest.c_str());

    if(sendto(sock, &request, 19 + rest.size(), 0, (struct sockaddr *)&group_address, sendLen) != (19 + rest.size()))
        syserr("Bad write for remove");
}

// Function for search
void do_search(int &sock, uint64_tGen &gen, std::string &rest) {
    auto sendLen = (socklen_t) sizeof(group_address);
    SIMPL_CMD request;
    memset(&request, 0, sizeof(request));
    strcpy (request.cmd, "LIST\0\0\0\0\0\0");

    uint64_t specialSeq = gen.genNum();
    request.cmd_seq = htobe64((uint64_t) specialSeq);
    if(!rest.empty()) {
        strcpy (request.data, rest.c_str());
    }

    if(sendto(sock, &request, 18 + rest.size(), 0, (struct sockaddr *)&group_address, sendLen) != (18 + rest.size()))
        syserr("Bad write for search");

    // Waiting for replies
    struct sockaddr_in server_address;
    SIMPL_CMD reply;
    auto rcvLen = (socklen_t) sizeof(server_address);


    forFetch.clear();
    setTimeout(sock, 0, 100);
    auto start = std::chrono::system_clock::now();

    while(true) {
        auto curr = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed_seconds = curr-start;

        if(elapsed_seconds.count() > TIMEOUT) {
            setTimeout(sock, TIMEOUT, 0);
            if(client::debug_ON)
                std::cout << "Finished waiting in search" << std::endl;
            break;
        }

        //memset(&server_address, 0, sizeof(server_address));
        memset(&reply, 0, sizeof(reply));
        ssize_t length = recvfrom(sock, &reply, sizeof(reply), 0,
                                  (struct sockaddr *) &server_address, &rcvLen);
        if(length < 0){
            if (errno == EAGAIN || errno == EWOULDBLOCK) {  // Timeout
                continue;
            }
            syserr("Error reading MY_LIST from server");
        }
        if(client::debug_ON)
            std::cout << "Received length: " << length << std::endl;

        if(length <= 18)
            printSkipping(ntohs(server_address.sin_port), inet_ntoa(server_address.sin_addr), 2);
        else if(!customStrCheck(reply.cmd, "MY_LIST\0\0\0"))
            printSkipping(ntohs(server_address.sin_port), inet_ntoa(server_address.sin_addr), 0);
        else if(be64toh(reply.cmd_seq) != specialSeq)
            printSkipping(ntohs(server_address.sin_port), inet_ntoa(server_address.sin_addr), 1);
        else {
            //std::cout << "Found " << inet_ntoa(server_address.sin_addr) << " (" << reply.data << ") with free space " << be64toh(reply.param) << std::endl;
            reply.data[strlen(reply.data)] = '\n';
            std::string data(reply.data);

            std::regex words_regex("[^\\n]+");
            auto words_begin =
                    std::sregex_iterator(data.begin(), data.end(), words_regex);
            auto words_end = std::sregex_iterator();

            for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
                std::smatch match = *i;
                std::string match_str = match.str();
                std::cout << match_str << " (" << inet_ntoa(server_address.sin_addr) << ")" << std::endl;

                forFetch[match_str].insert(inet_ntoa(server_address.sin_addr));
            }
        }
    }

}

// Function for discover
void do_discover(int &sock, uint64_tGen &gen) {
    auto sendLen = (socklen_t) sizeof(group_address);

    SIMPL_CMD bla;
    memset(&bla, 0, sizeof(bla));
    strcpy (bla.cmd, "HELLO\0\0\0\0\0");

    uint64_t specialSeq = gen.genNum();

    bla.cmd_seq = htobe64((uint64_t) specialSeq);

    if(sendto(sock, &bla, 18, 0, (struct sockaddr *)&group_address, sendLen) != 18)
        syserr("Bad write for discover");

    // Waiting for replies
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
                std::cout << "Finished waiting in discover" << std::endl;
            break;
        }

        memset(&server_address, 0, sizeof(server_address));
        memset(&reply, 0, sizeof(reply));
        ssize_t length = recvfrom(sock, &reply, sizeof(reply), 0,
                                  (struct sockaddr *) &server_address, &rcvLen);
        if(length < 0){
            if (errno == EAGAIN || errno == EWOULDBLOCK) {  // Timeout
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

// Generate random uint64_t constructor
uint64_tGen::uint64_tGen() : gen(rd()) {}

// Generate random uint64_t
uint64_t uint64_tGen::genNum() {
    return dis(gen);
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
        std::cerr << "[PCKG ERROR]  Skipping invalid package " << badAddress << ":" << badPort << ". Bad cmd field" << std::endl;

    // Bad cmd sequence
    if(messageType == 1)
        std::cerr << "[PCKG ERROR]  Skipping invalid package " << badAddress << ":" << badPort << ". Bad cmd sequence" << std::endl;

    // Bad package length
    if(messageType == 2)
        std::cerr << "[PCKG ERROR]  Skipping invalid package " << badAddress << ":" << badPort << ". Bad package length" << std::endl;
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
void initializeUDPBroadcastSocketClient(int &sock) {
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
}

// Create additional UDP socket
void normalUDPSocketClient(int &sock) {
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        syserr("socket");
}

// Checks what client types and returns code representing typed command
int parseLine(std::string &line, std::string &rest) {

    if (testLine(line, std::string("discover"), rest, 1)) {
        return client::DISCOVER;
    } else if (testLine(line, std::string("exit"), rest, 1)) {
        return client::EXIT;
    } else if (testLine(line, std::string("search"), rest, 0)) {
        return client::SEARCH;
    } else if (testLine(line, std::string("remove"), rest, -1)) {
        return client::DELETE;
    } else if (testLine(line, std::string("fetch"), rest, -1)) {
        return client::FETCH;
    } else {
        return client::ERROR;
    }
}


// Tests if line starts with a command, sets rest of the line when appropriate flag is set
// -1 - requires something for rest
// 0 - something can be in rest
// 1 - nothing for rest
bool testLine(std::string &line, std::string &&command, std::string &rest, int flag) {
    rest = "";

    std::string lowercase = line;
    std::transform(lowercase.begin(), lowercase.end(), lowercase.begin(), ::tolower);

    if(lowercase.compare(0, command.length(), command) != 0)
        return false;

    rest = line.substr(command.length());
    std::wstring::size_type firstPos = rest.find_first_not_of(' ');
    std::wstring::size_type lastPos = rest.find_last_not_of(' ');

    if((firstPos == lastPos && flag == -1 && lastPos == std::string::npos) || ((firstPos != lastPos && flag == 1) || (lastPos != std::string::npos && flag == 1))){
        rest = "";
        return false;
    }

    if(flag == 1) {
        rest = "";
        return true;
    }

    if(flag == 0 && firstPos == lastPos && lastPos == std::string::npos) {
        rest = "";
        return true;
    }

    if(lastPos == std::string::npos)
        rest = rest.substr(firstPos);
    else
        rest = rest.substr(firstPos, (lastPos - firstPos + 1));
    return true;
}

// 1 if options were set correctly, -1 otherwise
int parseOptions(int argc, char *argv[]) {

    int succesfull = 1;
    // Declare the supported options.
    po::options_description desc("Allowed options");
    desc.add_options()
            (",g", po::value<std::string>()->required(), "MCAST_ADDR")
            (",p", po::value<uint16_t>()->required(), "CMD_PORT")
            (",o", po::value<std::string>()->required(), "OUT_FLDR")
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
