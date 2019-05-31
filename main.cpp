#include "Client.h"

// variables
static uint16_t CMD_PORT;
static std::string MCAST_ADDR;
static int TIMEOUT;
static std::string OUT_FLDR;
static fs::path sharedFolerPath;
static struct sockaddr_in group_address;

// Map from file to servers that have it, and mutex to protect it
static std::map<std::string, std::unordered_set<std::string>> forFetch;
std::shared_mutex sharedMutexForFetch;

// Preventing output from mixing
std::mutex outputMutex;

// Random number generator
uint64_tGen gen;

// Custom server to space comparator
bool compareServ(const std::pair<std::string, uint64_t>& serv1, const std::pair<std::string, uint64_t>& serv2) {
    return serv1.second > serv2.second;
}

int main(int argc, char *argv[])
{
    int UDP_sock;
    init_logging();

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
    check_folder();

    while (std::getline (std::cin, line)) {

        int x = parseLine(line, rest);

        if(x == client::DISCOVER) {








            struct sockaddr_in serverSend;
            memset(&serverSend, 0, sizeof(serverSend));
            serverSend.sin_family = AF_INET;
            serverSend.sin_port = htons(CMD_PORT);
            if (inet_aton(MCAST_ADDR.c_str(), &serverSend.sin_addr) == 0) {
                syserr("inet_aton");
            }

            auto sendLen = (socklen_t) sizeof(serverSend);
            SIMPL_CMD request;
            memset(&request, 0, sizeof(request));
            strcpy (request.cmd, "HELLO\0\0\0\0\0");

            uint64_t specialSeq = gen.genNum();
            request.cmd_seq = htobe64((uint64_t) specialSeq);
            strcpy (request.data, std::string("").c_str());

            if(sendto(UDP_sock, &request, 19, 0, (struct sockaddr *)&serverSend, sendLen) != (19)) {
                syserr("Bad write");
            }












            std::set<std::pair<std::string, uint64_t>, decltype(&compareServ)> justAnArgument(&compareServ);
            do_discover(UDP_sock, true, justAnArgument);
            continue;
        }

        if (x == client::ERROR) {
            BOOST_LOG_TRIVIAL(debug) << "INCORRECT LINE TYPE AGAIN";
            continue;
        }

        if (x == client::SEARCH) {
            do_search(UDP_sock, rest);

            continue;
        }

        if (x == client::DELETE) {
            do_remove(UDP_sock, rest);
            continue;
        }

        if (x == client::FETCH) {
            std::thread(do_fetch, rest).detach();
            continue;
        }

        if (x == client::UPLOAD) {
            std::thread(do_upload, rest).detach();
            continue;
        }

        if (x == client::EXIT) {
            break;
        }
    }

    BOOST_LOG_TRIVIAL(debug) << "{MAIN} PROGRAM IS DONE";

    return 0;
}


// Function for initial handling of folder
void check_folder() {
    sharedFolerPath = fs::path(OUT_FLDR);

    if(!fs::is_directory(sharedFolerPath))
        syserr("Bad folder path");
}

// Function for upload
void do_upload(std::string rest) {

    // Opening a given file
    std::ifstream is(rest, std::ifstream::binary);
    if(is) {
        BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} File is successfully opened";
    } else {
        outputMutex.lock();
        std::cout << "File " << rest << " does not exist" << std::endl;
        outputMutex.unlock();
        return;
    }

    // Initializing extra UDP socket (not to interfere with main socket)
    int UDPSocket;
    initializeUDPBroadcastSocketClient(UDPSocket);

    // Discover to find out what servers are available (sort them by space in descending order)
    std::set<std::pair<std::string, uint64_t>, decltype(&compareServ)> setServerSpace(&compareServ);
    do_discover(UDPSocket, false, setServerSpace);

    // Go through all of them and ask them to upload
    for(auto servInfo: setServerSpace) {
        // Send request
        uint64_t specialSeq = sendComplex(UDPSocket, CMD_PORT, servInfo.first, "ADD\0\0\0\0\0\0\0", servInfo.second, rest, true);

        // Wait for reply
        CMPLX_CMD reply;
        struct sockaddr_in server_address;
        ssize_t length = receiveSth(UDPSocket, &reply, server_address, true);

        // Check if data was valid + return on timeout
        if(!clientValidateRecv(length, "UPLOAD", false))
            return;

        // Verify data received from reply
        if(!verifyReceived(server_address, !customStrCheck(reply.cmd, "CAN_ADD\0\0\0"), be64toh(reply.cmd_seq) != specialSeq, length != 27, strcmp(reply.data, "") != 0)) {
            return;
        }

        BOOST_LOG_TRIVIAL(debug) << "{UPLOAD} Time to open TCP and wait for server on port: " << be64toh(reply.param);
    }
}

// Function for fetch
void do_fetch(std::string rest) {
    // Pointer to value of forFetch
    std::unordered_set<std::string> *setPtr;
    // Chosen server
    std::string serverAddress;

    // Reading from ForFetch
    sharedMutexForFetch.lock_shared();
    if(forFetch.find(rest) == forFetch.end()) {
        outputMutex.lock();
        std::cout << "Not recently in searched/searched nor used" << std::endl;
        outputMutex.unlock();
        return;
    } else {
        setPtr = &forFetch.find(rest)->second;
    }

    // Extra socket not to interrupt main
    int sock;
    normalUDPSocketClient(sock);

    // Choose random server that has value needed
    auto item = setPtr->begin();
    uint64_t random_pos = gen.genNum() % setPtr->size();
    std::advance(item, random_pos);
    serverAddress = *item;

    sharedMutexForFetch.unlock_shared(); // Finished using ForFetch

    struct sockaddr_in serverSend;
    serverSend.sin_family = AF_INET;
    serverSend.sin_port = htons(CMD_PORT);
    if (inet_aton(serverAddress.c_str(), &serverSend.sin_addr) == 0)
        syserr("inet_aton");

    // Send GET request
    uint64_t specialSeq = sendSimple(sock, CMD_PORT, serverAddress, "GET\0\0\0\0\0\0\0", rest, true);

    // Wait for response
    CMPLX_CMD reply;
    struct sockaddr_in server_address;
    ssize_t length = receiveSth(sock, &reply, server_address, true);

    // Check if data was valid + return on timeout
    if(!clientValidateRecv(length, "FETCH", false))
        return;

    // Wait for response
    if(!verifyReceived(server_address, !customStrCheck(reply.cmd, "CONNECT_ME"), be64toh(reply.cmd_seq) != specialSeq, length <= 26, strcmp(reply.data, rest.c_str()) != 0)) {
        return;
    }

    // Create TCP socket and connect to it
    int tcpSocket;
    createTCPClientSocket(tcpSocket, (uint16_t) be64toh(reply.param), server_address);

    // Generate absolute file path
    std::string filePath = sharedFolerPath.string() + "/" + rest;

    // Create a file if doesn't exists
    fs::ofstream file(filePath);
    file.close();

    // Receive the file over TCP and save it
    if (saveFile(tcpSocket, filePath)) {
        outputMutex.lock();
        std::cout << "File " << rest << " downloaded (" << inet_ntoa(server_address.sin_addr) << ":" << ntohs(server_address.sin_port) << ")" << std::endl;
        outputMutex.unlock();
    } else {
        outputMutex.lock();
        std::cout << "File " << rest << " downloading failed (" << inet_ntoa(server_address.sin_addr) << ":" << ntohs(server_address.sin_port) << ") Bad read" << std::endl;
        outputMutex.unlock();
    }
}

// Function for removal
void do_remove(int &sock, std::string &rest) {
    // Sending request delete file
    sendSimple(sock, CMD_PORT, MCAST_ADDR, "DEL\0\0\0\0\0\0\0", rest, true);
}

// Function for search
void do_search(int &sock, std::string &rest) {

    // Sending request
    uint64_t specialSeq = sendSimple(sock, CMD_PORT, MCAST_ADDR, "LIST\0\0\0\0\0\0", rest, true);

    // Waiting for replies holders
    SIMPL_CMD reply;
    struct sockaddr_in server_address;


    // Changing Fetch lock it
    sharedMutexForFetch.lock();
    forFetch.clear();
    setTimeout(sock, 0, 100);
    auto start = std::chrono::system_clock::now();

    while(true) {
        auto curr = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed_seconds = curr-start;

        if(elapsed_seconds.count() > TIMEOUT) {
            setTimeout(sock, TIMEOUT, 0);
            BOOST_LOG_TRIVIAL(debug) << "{SEARCH} Finished waiting";
            break;
        }

        ssize_t length = receiveSth(sock, &reply, server_address, true);

        // Check if data was valid + return on timeout
        if(!clientValidateRecv(length, "SEARCH", true))
            continue;

        BOOST_LOG_TRIVIAL(debug) << "{SEARCH} Received length: " << length << std::endl;

        // Wait for response
        if(!verifyReceived(server_address, !customStrCheck(reply.cmd, "MY_LIST\0\0\0"), be64toh(reply.cmd_seq) != specialSeq, length <= 18, false)) {
            return;
        }

        // Anlyze received data from server
        reply.data[strlen(reply.data)] = '\n';
        std::string data(reply.data);

        std::regex words_regex("[^\\n]+");
        auto words_begin =
                std::sregex_iterator(data.begin(), data.end(), words_regex);
        auto words_end = std::sregex_iterator();

        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::smatch match = *i;
            std::string match_str = match.str();

            // Printing line
            outputMutex.lock();
            std::cout << match_str << " (" << inet_ntoa(server_address.sin_addr) << ")" << std::endl;
            outputMutex.unlock();

            forFetch[match_str].insert(inet_ntoa(server_address.sin_addr));
        }
    }
    sharedMutexForFetch.unlock();
}

// Function for discover
// When modeNormal == true then just run a regular discover
// When mode Noraml == false then don't write anything + update setServerSpace
void do_discover(int &sock, bool modeNormal, std::set<std::pair<std::string, uint64_t>, bool (*)(const std::pair<std::string, uint64_t>&, const std::pair<std::string, uint64_t>&)> &setServerSpace) {

    // Sending request
    uint64_t specialSeq = sendSimple(sock, CMD_PORT, MCAST_ADDR, "HELLO\0\0\0\0\0", "", true);

    // Waiting for replies
    struct sockaddr_in server_address;
    CMPLX_CMD reply;


    setTimeout(sock, 0, 100);
    auto start = std::chrono::system_clock::now();

    while(true) {
        auto curr = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsed_seconds = curr-start;

        if(elapsed_seconds.count() > TIMEOUT) {
            setTimeout(sock, TIMEOUT, 0);
            BOOST_LOG_TRIVIAL(debug) << "{DISCOVER} Finished waiting";
            break;
        }


        ssize_t length = receiveSth(sock, &reply, server_address, true);

        // Check if data was valid + return on timeout
        if(!clientValidateRecv(length, "DISCOVER", true))
            continue;

        // Wait for response
        if(!verifyReceived(server_address, !customStrCheck(reply.cmd, "GOOD_DAY\0\0"), be64toh(reply.cmd_seq) != specialSeq, length < 26, false)) {
            return;
        }

        if(modeNormal) {
            // Printing line
            outputMutex.lock();
            std::cout << "Found " << inet_ntoa(server_address.sin_addr) << " (" << reply.data << ") with free space " << be64toh(reply.param) << std::endl;
            outputMutex.unlock();
        } else {
            setServerSpace.insert(std::make_pair(inet_ntoa(server_address.sin_addr), be64toh(reply.param)));
        }
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

// Sets timeout with given seconds and micro seconds
void setTimeout(int sock, time_t sec, suseconds_t micro) {
    struct timeval tv;
    tv.tv_sec = sec;
    tv.tv_usec = micro;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
        syserr("Setting timeout failed");
}

// Creates TCP socket for client and connects it to server
void createTCPClientSocket(int &socketTCP, uint16_t port, struct sockaddr_in serverAddr) {
    struct sockaddr_in serverConnAddr; // For connecting to server address
    socketTCP = socket(AF_INET, SOCK_STREAM, 0);
    if(socketTCP < 0)
        syserr("Failed to create a TCP socket");

    serverConnAddr.sin_family = AF_INET;
    serverConnAddr.sin_port = htons(port);
    serverConnAddr.sin_addr = serverAddr.sin_addr;

    if (connect(socketTCP, (struct sockaddr*)&serverConnAddr, sizeof(serverConnAddr)) != 0) {
        syserr("Connection for fetch with the server failed");
    } else {
        BOOST_LOG_TRIVIAL(debug) << "{FETCH/UPLOAD} Connected to the server, ready to receive the file...";
    }
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

// -------------- TIMEOUT
// Create additional UDP socket
void normalUDPSocketClient(int &sock) {
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
        syserr("socket");

    setTimeout(sock, TIMEOUT, 0);
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
    } else if (testLine(line, std::string("upload"), rest, -1)) {
        return client::UPLOAD;
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

// True   -> Success
// False  -> Timeout
bool clientValidateRecv(ssize_t length, std::string &&where, bool inLoop) {
    if(length < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {  // Timeout
            if(!inLoop)
                BOOST_LOG_TRIVIAL(debug) << "TIMEOUT in " << where;
            return false;
        }
        syserr("Error in UDP read");
    }
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
            ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("-g")) {
        BOOST_LOG_TRIVIAL(debug) << "MCAST_ADDR was set to " << vm["-g"].as<std::string>();
        MCAST_ADDR = vm["-g"].as<std::string>();
    } else {
        std::cerr << "MCAST_ADDR was not set." << std::endl;
        succesfull = -1;
    }

    if (vm.count("-p")) {
        BOOST_LOG_TRIVIAL(debug) << "CMD_PORT was set to " << vm["-p"].as<uint16_t>();
        CMD_PORT = vm["-p"].as<uint16_t>();
    } else {
        std::cerr << "CMD_PORT was not set." << std::endl;
        succesfull = -1;
    }

    if (vm.count("-o")) {
        BOOST_LOG_TRIVIAL(debug) << "OUT_FLDR was set to " << vm["-o"].as<std::string>();
        OUT_FLDR = vm["-o"].as<std::string>();
    } else {
        std::cerr << "OUT_FLDR was not set." << std::endl;
        succesfull = -1;
    }

    if (vm.count("-t")) {
        BOOST_LOG_TRIVIAL(debug) << "TIMEOUT was set to " << vm["-t"].as<int>();
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

// Reads data of length from socket
// -1 -> Received info about close connection
// 0  -> Successfully read another chunk of data
// 1  -> Error reading occured
int readSomething(void *to_read, int socket, uint32_t length) {
    unsigned int prev_len = 0;
    unsigned int remains = 0;

    while (prev_len < length) {
        remains = length - prev_len;
        ssize_t len = read(socket, ((char *) to_read) + prev_len, remains);
        if (len < 0) {
            return 1;
        }

        if (len == 0) {
            return -1;
        }

        prev_len += len;
        if (prev_len == length)
            return 0;
    }
}

// Save file read through TCP socket in OUT_FOLDER
// False -> Error reading from a file True
bool saveFile(int TcpSocket, std::string &filePath) {
    //sharedFolerPath.string() + "/" + fileName
    // albo dam ścieżkę po prostu
    std::ofstream outfile(filePath, std::ofstream::trunc | std::ofstream::binary);

    char buff[client::bufferSize + 1];

    int success;
    do {
        memset(&buff, 0 , client::bufferSize + 1);
        success = readSomething(buff, TcpSocket, client::bufferSize);

        if (success == 1) {
            return false;
        }

        if(success)
            outfile.write(buff, client::bufferSize);
        else {
            if (buff[0] != '0')
                outfile.write(buff, strlen(buff));
        }
    } while(success == 0);

    return true;
}

// 1. Stworzyć podsieci
// 2. Połączyć je ruterami
// 3. Zapenić kontrolę dostępu (firewall domyślnie wszytko odzrucane ale dal niektórych przechodzi)
// 4. (Dia do rysowania)

// Sends simple message to given coordinates, returns code sequence (0 when error occured)
uint64_t sendSimple(int UDPsocket, uint16_t port, std::string servAddr, const char *messageType, std::string dataFieled, bool isClient) {
    struct sockaddr_in serverSend;
    serverSend.sin_family = AF_INET;
    serverSend.sin_port = htons(port);
    if (inet_aton(servAddr.c_str(), &serverSend.sin_addr) == 0) {
        if(isClient)
            syserr("inet_aton");
        else
            return 0;
    }
    BOOST_LOG_TRIVIAL(debug) << "{DISCOVER} port: " << port << " servAddr: " << servAddr << " messageType: " << messageType;

    auto sendLen = (socklen_t) sizeof(serverSend);
    SIMPL_CMD request;
    memset(&request, 0, sizeof(request));
    strcpy (request.cmd, messageType);

    uint64_t specialSeq = gen.genNum();
    request.cmd_seq = htobe64((uint64_t) specialSeq);
    strcpy (request.data, dataFieled.c_str());

    if(sendto(UDPsocket, &request, 19 + dataFieled.size(), 0, (struct sockaddr *)&serverSend, sendLen) != (19 + dataFieled.size())) {
        if(isClient)
            syserr("Bad write");
        else
            return 0;
    }

    return specialSeq;
}

// Sends simple message to given coordinates, returns code sequence
uint64_t sendComplex(int UDPsocket, uint16_t port, std::string servAddr, const char *messageType, uint64_t param, std::string dataFieled, bool isClient) {
    struct sockaddr_in serverSend;
    serverSend.sin_family = AF_INET;
    serverSend.sin_port = htons(port);
    if (inet_aton(servAddr.c_str(), &serverSend.sin_addr) == 0) {
        if(isClient)
            syserr("inet_aton");
        else
            return 0;
    }

    auto sendLen = (socklen_t) sizeof(serverSend);
    CMPLX_CMD request;
    memset(&request, 0, sizeof(request));
    strcpy (request.cmd, messageType);

    uint64_t specialSeq = gen.genNum();
    request.cmd_seq = htobe64((uint64_t) specialSeq);
    strcpy (request.data, dataFieled.c_str());
    request.param = param;

    if(sendto(UDPsocket, &request, 27 + dataFieled.size(), 0, (struct sockaddr *)&serverSend, sendLen) != (27 + dataFieled.size())) {
        if(isClient)
            syserr("Bad write");
        else
            return 0;
    }

    return specialSeq;
}

// For receiving simple or complex message, returns received size
ssize_t receiveSth (int UDPSocket, void *ptr, struct sockaddr_in &receivingFrom, bool isComplex) {
    ssize_t length;
    memset(&receivingFrom, 0, sizeof(receivingFrom));
    auto rcvLen = (socklen_t) sizeof(receivingFrom);

    if (isComplex) {
        memset(ptr, 0, sizeof(*((CMPLX_CMD *) ptr)));
        length = recvfrom(UDPSocket, ptr, sizeof(*((CMPLX_CMD *) ptr)), 0, (struct sockaddr *) &receivingFrom, &rcvLen);
    } else {
        memset(ptr, 0, sizeof(*((SIMPL_CMD *) ptr)));
        length = recvfrom(UDPSocket, ptr, sizeof(*((SIMPL_CMD *) ptr)), 0, (struct sockaddr *) &receivingFrom, &rcvLen);
    }

    return length;
}

// Prints skipping for client
bool printSkipping(struct sockaddr_in &receivingAddr, int messageType) {
    uint16_t badPort = ntohs(receivingAddr.sin_port);
    std::string badAddress = inet_ntoa(receivingAddr.sin_addr);

    // Bad cmd field
    if(messageType == 0)
        std::cerr << "[PCKG ERROR]  Skipping invalid package " << badAddress << ":" << badPort << ". Bad cmd field" << std::endl;

    // Bad cmd sequence
    if(messageType == 1)
        std::cerr << "[PCKG ERROR]  Skipping invalid package " << badAddress << ":" << badPort << ". Bad cmd sequence" << std::endl;

    // Bad package length
    if(messageType == 2)
        std::cerr << "[PCKG ERROR]  Skipping invalid package " << badAddress << ":" << badPort << ". Bad package length" << std::endl;

    // Bad package length
    if(messageType == 3)
        std::cerr << "[PCKG ERROR]  Skipping invalid package " << badAddress << ":" << badPort << ". Wrong data field" << std::endl;
}

// Werify and print skiping (false -> skipped, true otherwise
// When Bool is True print skipping for given type
bool verifyReceived(struct sockaddr_in &receiving_address, bool verify0, bool verify1, bool verify2, bool verify3) {
    if(verify0) {
        printSkipping(receiving_address, 0);
        return false;
    } else if(verify1) {
        printSkipping(receiving_address, 1);
        return false;
    } else if(verify2) {
        printSkipping(receiving_address, 2);
        return false;
    } else if(verify3) {
        printSkipping(receiving_address, 3);
        return false;
    } else {
        return true;
    }
}
