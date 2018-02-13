/*
    @author: antriksh
    Version 0: 2/5/2018
*/

#include "header/Socket.hpp"

class Process {

    /*
        Process: part of the lamport's mutual exclusion algorithm
        * connects to all servers and stores a list of all other clients
        * maintains mutual exclusion while attempting to read and write
        * Has a clock (Lamport's clock)
        * Has a unique ID (given by the user)
    */

public:
    priority_queue<Message> readRequestQueue;
    priority_queue<Message> writeRequestQueue;

    bool pendingEnquiry, pendingRead, pendingWrite;

    Process(char *argv[], vector<ProcessInfo> clients, vector<ProcessInfo> servers)
    : Socket(argv, clients, servers) {
        /*
            Initiates a socket connection
            Finds the port(s) to connect to for the servers
            initializes the list of clients
            sends requests for read and write of files to server while maintaining mutual exclusion
        */

        id = argv[1];
        logfile = processId;
        logger.open("logs/" + logfile + ".txt", ios::app | ios::out);
        // port = atoi(argv[2]);

        this->sayHello();
        this->enquiry();
    }

    int connectTo(string hostname, int portno) {

        /*
            attempts to connect to hostname:portno
            throws exceptions wherever it fails
        */

        // Create a socket using socket() system call
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
            throw "ERROR opening socket";

        // Search for host
        struct hostent *host = gethostbyname(hostname.c_str());
        if (host == NULL) {
            throw "ERROR, no such host";
        }
        Logger(hostname + ":" + to_string(portno)  + " found !", this->clock);

        struct sockaddr_in host_addr, cli_addr;
        bzero((char *) &host_addr, sizeof(host_addr));
        host_addr.sin_family = AF_INET;
        bcopy((char *)host->h_addr,
              (char *)&host_addr.sin_addr.s_addr,
              host->h_length);
        host_addr.sin_port = htons(portno);

        // Attempting to connect to host
        if (connect(fd, (struct sockaddr *) &host_addr, sizeof(host_addr)) < 0)
            throw "error(ERROR connecting, this->clock)";

        return fd;
    }

    void sayHello() {
        for (ProcessInfo server: allServers) {
            try {
                Logger("Connecting to " + server.processID + " at " + server.hostname, this->clock);
                int fd = this->connectTo(server.hostname, server.port);
                this->send(personalfd, fd, "hi");
                // Message *m = this->receive(fd);
            }
            catch (const char* e) {
                Logger(e, this->clock);
                continue;
            }
        }
        for (ProcessInfo client: allClients) {
            try {
                if (client.processID != processId) {
                    Logger("Connecting to " + client.processID + " at " + client.hostname, this->clock);
                    int fd = this->connectTo(client.hostname, client.port);
                    this->send(personalfd, fd, "hi");
                    // Message *m = this->receive(fd);
                }
            }
            catch (const char* e) {
                Logger(e, this->clock);
                continue;
            }
        }
    }

    void send(int source, int destination, string message, int rw=0, string filename="NULL") {

        /*
            Message send: sends 'message' from source to destination
            source: int value of source fd
            destination: int value of destination fd
            message: string to send
        */

        Message *msg = new Message(false, rw, message, source, processId, destination, this->clock, filename);
        this->setClock();

        n = write(destination, messageString(msg).c_str(), 1024);
        if (n < 0)
            error("ERROR writing to socket", this->clock);

        if (destination == personalfd)
            Logger("[SENT TO SERVER ]: " + message, this->clock);
        else
            Logger("[SENT TO " + to_string(destination) + "]: " + message, this->clock);
    }

    Message* receive(int source) {
        /*
            Message receive: attempts receipt of a 'message' from source
            source: int value of source fd
        */

        char msg[1024];
        bzero(msg, 1024);
        n = read(source, msg, 1024);
        if (n < 0) {
            error("ERROR reading from socket", this->clock);
        }

        Message *message = getMessage(msg);
        this->setClock(message->timestamp);

        if (source == personalfd)
            Logger("[RECEIVED FROM SERVER ]: " + message->message, this->clock);
        else
            Logger("[RECEIVED FROM " + to_string(source) + "]: " + message->message, this->clock);

        return message;
    }

    void acceptConnection() {
        while (1) {
            // Accept a connection with the accept() system call
            int newsockfd = accept(personalfd, (struct sockaddr *) &cli_addr, &clilen);
            if (newsockfd < 0) {
                // continue;
                error("ERROR on accept", this->clock);
            }
            Logger("New connection " + to_string(newsockfd), this->clock);

            // this->introduce(newsockfd);
            Message *message = this->receive(newsockfd);
            messageQueue.push(message);

            close(newsockfd);
        }
    }

    void processMessages() {
        while (1) {
            if (!messageQueue.empty()) {
                Message *message = messageQueue.front();
                messageQueue.pop();
                this->checkMessage(message);
            }
        }
    }

    void writeMessage(Message *m, string text, int rw=0, string f="NULL") {
        ProcessInfo client = getFd(m);
        try {
            int fd = connectTo(client.hostname, client.port);
            this->send(m->destination, fd, text, rw, f);
            close(fd);
        } catch (const char* e) {
            Logger(e, this->clock);
            return;
        }
    }

    void checkMessage(Message *m) {
        if (m->message == "hi") {
            // otherFds[numBeings++] = m->source;
            // others[m->source] = m->sourceID;
            writeMessage(m, "hello");
        } else if (m->message == "hello") {
            // if(others[m->source].empty()){
            //     otherFds[numBeings++] = m->source;
            //     others[m->source] = m->sourceID;
            // }
        } else if (m->message == "received") {
        } else if (m->message == "request") {
            // if (m->readWrite == 2) {
            //     Logger("[WRITE REQUEST FOR FILE]: " + m->filename, this->clock);
            // } else if (m->readWrite == 1) {
            //     Logger("[READ REQUEST FOR FILE]: " + m->filename, this->clock);
            // }
            // readRequestQueue.push(m);
        } else if (m->message == "reply") {
        } else if (m->message == "release") {
        } else if (m->message == "") {
        } else if (m->readWrite == 2) {
            Logger("[WRITTEN TO FILE]: " + m->message, this->clock);
        } else if (m->readWrite == 1) {
            Logger("[READ LINE]: " + m->message, this->clock);
        } else if (m->readWrite == 3) {
            Logger("[ALL FILES RECEIVED]: " + m->message, this->clock);
            istringstream line_stream(m->message);
            string token;
            while (getline(line_stream, token, ':')) {
                allFiles.push_back(token);
            }
        }
        return;
    }

    ProcessInfo getFd(Message *m) {
        string sid = m->sourceID;
        try {
            ProcessInfo client = findInVector(allClients, sid);
            return client;
        } catch (const char* e) {}
        try {
            ProcessInfo server = findInVector(allServers, sid);
            return server;
        } catch (const char* e) {
            throw e;
        }
    }

    void enquiry() {
        for (ProcessInfo server: allServers) {
            try {
                Logger("Connecting to " + server.processID + " at " + server.hostname, this->clock);
                int fd = this->connectTo(server.hostname, server.port);
                this->send(personalfd, fd, "enquiry", 3);
                // Message *m = this->receive(fd);
                break;
            }
            catch (const char* e) {
                Logger(e, this->clock);
                continue;
            }
        }
        pendingEnquiry = true;
    }

    void readRequest() {
        for (ProcessInfo client: allClients) {
            try {
                if (client.processID != processId) {
                    Logger("Connecting to " + client.processID + " at " + client.hostname, this->clock);
                    int fd = this->connectTo(client.hostname, client.port);
                    this->send(personalfd, fd, "request", 1, "file1");
                    // Message *m = this->receive(fd);
                }
            }
            catch (const char* e) {
                Logger(e, this->clock);
                continue;
            }
        }
    }

    void writeRequest() {

    }

    void criticalSection() {

    }

    void setClock() {
        this->clock += D;
    }

    void setClock(int timestamp) {
        this->clock = max(this->clock + D, timestamp);
    }

    void resetClock() {
        this->clock = 0;
    }

    ~Process() {
        close(personalfd);
    }

};

void io(Process *client) {
    int rw = rand() % 1;
    sleep(15);
    client->readRequest();
}

int main(int argc, char *argv[])
{


    if (argc < 3) {
        fprintf(stderr, "usage %s ID port", argv[0]);
        // Logger("usage " + string(argv[0]) + " ID hostname port");
        exit(0);
    }

    vector<ProcessInfo> allClients, allServers;
    allClients = readClients(allClients, "clients.csv");
    allServers = readClients(allServers, "servers.csv");

    Process *client = new Process(argv, allClients, allServers);

    std::thread processThread(&Process::processMessages, client);
    std::thread connectionThread(&Process::acceptConnection, client);

    io(client);

    processThread.join();
    connectionThread.join();

    logger.close();
    return 0;
}