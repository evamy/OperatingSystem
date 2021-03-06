/*
    @author: antriksh
    Version 0: 3/14/2018
*/

#include "header/MetaInfo.hpp"
#include "header/Socket.h"

// Server:
//         * Manages files and file paths
//         * Takes client requests to read and write
//         * Does not do anything for maintaining mutual exclusion
class Server : protected Socket {
   private:
    string directory;
    int mserverfd;
    string mserverID;
    vector<string> files;

   public:
    Server(char *argv[]) : Socket(argv) { directory = argv[3]; }

    // Infinite thread sending heartbeat messages to mserver
    void heartBeat() {
        while (1) {
            ProcessInfo p = mserver[0];
            int mserverfd = this->connectTo(p.hostname, p.port);
            Logger("[HEARTBEAT]");
            files = readDirectory(directory);
            send(personalfd, mserverfd, "heartbeat", makeFileTuple(files),
                 p.processID);
            close(mserverfd);
            sleep(5);
        }
    }

    // Infinite thread to accept connection and detach a thread as
    // a receiver and checker of messages
    void listener() {
        while (1) {
            // Accept a connection with the accept() system call
            int newsockfd =
                accept(personalfd, (struct sockaddr *)&cli_addr, &clilen);
            if (newsockfd < 0) {
                error("ERROR on accept");
            }

            std::thread connectedThread(&Server::processMessages, this,
                                        newsockfd);
            connectedThread.detach();
        }
    }

    // Starts as a thread which receives a message and checks the message
    // @newsockfd - fd socket stream from which message would be received
    void processMessages(int newsockfd) {
        try {
            Message *message = this->receive(newsockfd);
            this->checkMessage(message, newsockfd);
        } catch (const char *e) {
            Logger(e);
            close(newsockfd);
        }
    }

    // Checks the message for different types of incoming messages
    // 1. hi - just a hello from some client/server (not in use)
    // 2. something else (means a request to be granted)
    // @m - Message just received
    // @newsockfd - socket stream it was received from
    void checkMessage(Message *m, int newsockfd) {
        if (m->type == "create") {
            createEmptyChunk(m);
        } else {
            this->checkReadWrite(m, newsockfd);
        }
        throw "BREAKING CONNECTION";
    }

    // Checks the request type
    // 1 - Read: reads the last line of the file and sends it to the
    // process 2 - Write: Writes to the file and replies 3 - Create: replies
    // with a list of files
    // @m - Message just received
    // @newsockfd - socket stream it was received from
    void checkReadWrite(Message *m, int newsockfd) {
        switch (m->readWrite) {
            case 1: {
                string line = readFile(directory + "/" + m->fileName, m->offset,
                                       m->byteCount);
                writeReply(m, newsockfd, "reply", line);
                break;
            }
            case 2: {
                string writeMessage = m->message;
                if (m->offset > 0) {
                    writeMessage = m->message.substr(m->offset, m->byteCount);
                } else if (m->byteCount > 0) {
                    writeMessage = m->message.substr(0, m->byteCount);
                }
                writeToFile(directory + "/" + m->fileName, writeMessage);
                writeReply(m, newsockfd, "reply", m->message);
                break;
            }
            default: {
                string line = "UNRECOGNIZED MESSAGE !";
                connectAndReply(m, "", line);
                break;
            }
        }
    }

    // Creates an empty chunk in the server directory
    void createEmptyChunk(Message *m) {
        string name = m->fileName;

        Logger("[CREATING NEW CHUNK]: " + name);

        ofstream fs;
        fs.open(directory + "/" + name, ios::out);
        fs.close();
        files.push_back(name);
    }

    ~Server() { close(mserverfd); }
};

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "usage %s ID port directory_path\n", argv[0]);
        exit(1);
    }

    Server *server = new Server(argv);

    std::thread listenerThread(&Server::listener, server);
    std::thread heartbeatThread(&Server::heartBeat, server);
    heartbeatThread.join();
    listenerThread.join();

    logger.close();
    return 0;
}