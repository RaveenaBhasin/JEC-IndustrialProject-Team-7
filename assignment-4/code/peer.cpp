#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h> 
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <pthread.h>
#include <math.h>
#include <sys/stat.h>
#include <map>
#include <csignal>
#include "./sha256.h"

#define PORT 8000
#define MAX_CLIENTS 15
#define CLIENT_FILE "client.txt"

pthread_mutex_t clientMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t downloadFileMutex = PTHREAD_MUTEX_INITIALIZER;

class Client_Data {
    public:
    int socket;
    char* data;
    Client_Data(int socket, char* clientData) {
        this->socket = socket;
        this->data = clientData;
    }
};
class Download_File_Data {
    public:
    int socket, start, end;
};

//personal info
std::string myName="", myUid="";
int trackerSocket, serverSocket, exit_program=0;
char myFile[4096]={0}; //filename:uid:blocks:totalSize:hash
std::map<std::string, int> connectedPeers;

void writeInFile(const char msg[]) {
    pthread_mutex_lock(&clientMutex);
    std::ofstream client(CLIENT_FILE, std::ios::app);
    client << msg << std::endl;
    client.close();
    pthread_mutex_unlock(&clientMutex);
}

std::vector<std::string> split(const char str[], char a) {
    std::vector<std::string> res;
    std::stringstream ss(str);
    std::string temp;
    while(getline(ss, temp, a)) {
        res.push_back(temp);
    }
    return res;
}

void* receiving(void* arg) {
    Client_Data* client = (Client_Data*) arg;
    int clientSocket = client->socket;
    char* clientData = client->data;
    char msg[4096] = {0};
    
    while(true) {
        memset(msg, 0, 4096);
        int bytesRecv = recv(clientSocket, msg, 4096, 0);
        if(bytesRecv <= 0) {
            std::cout << clientData << " disconnected!\n";
            break;
        }
        std::cout << clientData << "> " << msg << std::endl;
        std::vector<std::string> res = split(msg, ':');
        
        if(res[0].compare("download_file")==0) {
            send(clientSocket, myFile, strlen(myFile), 0);
            int start=0, end=-1;
            if(res.size()==2) {
                std::vector<std::string> limit =split(res[1].c_str(), '-');
                start=stoi(limit[0]);
                end=stoi(limit[1]);
            }
            
            res = split(myFile, ':'); //filename:uid:blocks:totalSize:shaOEachBlock
            std::ifstream file(res[0].c_str());
            int blocks = stoi(res[2]);
            end = (end==-1) ? blocks : end;
            for(int i=1; i<=blocks; i++) {
                memset(msg, 0, 4096);
                file.read(msg, 4095);
                if(i>=start && i<=end)
                    send(clientSocket, msg, sizeof(msg), 0);
            };
            file.close();
        }
    }
    pthread_exit(NULL);
}
void* server(void* arg) {
    char* port = (char*) arg;
    int portNo = atoi(port);
    
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(serverSocket == 0) {
        std::cout << "Can't create a socket!\n";
        pthread_exit(NULL);
    }
    sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(portNo);
    
    inet_pton(AF_INET, "127.0.0.1", &saddr.sin_addr);
    
    // bind server with ip/port
    if(bind(serverSocket, (sockaddr*)&saddr, sizeof(saddr))<0) {
        std::cout << "Bind Error\n";
        pthread_exit(NULL);
    }
    
    //Listening
    if(listen(serverSocket, SOMAXCONN) < 0) {
        std::cout << "Listening Error\n" << std::endl;
        pthread_exit(NULL);
    }
    
    send(trackerSocket, port, strlen(port), 0);
    std::cout << "Started Server on " << portNo << std::endl << std::endl;
    
    pthread_t threads[MAX_CLIENTS];
    
    for(int i=0; i<MAX_CLIENTS; i++) {
        sockaddr_in caddr;
        socklen_t socklen = sizeof(caddr);
        int clientSocket = accept(serverSocket, (sockaddr*)&caddr, &socklen);
        
        if(exit_program==1) 
            pthread_exit(NULL);
            
        char clientData[1024] = {0};
        recv(clientSocket, clientData, 1024, 0);
        std::cout << clientData << " is connected!\n";
        
        Client_Data* data = new Client_Data(clientSocket, clientData);
        
        if(pthread_create(&threads[i], NULL, receiving, (void*)data)) {
            std::cout << "Error creating thread\n";
            continue;
        }
    }
    for(int i=0; i<MAX_CLIENTS; i++) 
        pthread_join(threads[i], NULL);
    
    pthread_exit(NULL);
}

void* fileDownload(void* arg) {
    Download_File_Data* obj = (Download_File_Data*) arg;
    
    int start=obj->start, end=obj->end;
    int peerSock = obj->socket;
    char received[4096] = {0};
    
    recv(peerSock, received, 4096, 0);   //filename:uid:blocks:totalSize:shaOfEachBlock
    std::vector<std::string> res = split(received, ':');
    std::vector<std::string> shaCodes = split(res[4].c_str(), '_');
    
    bool success = 1;
    char msg[4096]={0};
    
    pthread_mutex_lock(&downloadFileMutex);
    std::ofstream file(res[0].c_str(), std::ios::app);
    for(int i=start; i<=end; i++) {
        memset(msg, 0, 4096);
        recv(peerSock, msg, sizeof(msg), 0);
        std::string sha = sha256(msg);
        if(shaCodes[i-1].compare(sha)!=0) {
            std::cout << "File Corrupted "<< i << "\n";
            success=0;
        }
        file << msg;
    }
    file.close();
    pthread_mutex_unlock(&downloadFileMutex);
    
    if(success)
        std::cout << "File successfully downloaded from block "<<start<<" to "<<end<<"\n\n";
    else
        std::cout << "File Corrupted\n\n";
    
    pthread_exit(NULL);
}

void connectPeer(std::string peer_uid, int port) {
    
    int peerSock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);

    if(inet_pton(AF_INET, "127.0.0.1", &saddr.sin_addr) < 0) {
        std::cout << "Error\n";
        pthread_exit(NULL);
    }
    
    if(peerSock == 0) {
        std::cout << "Error in creating socket!\n";
        pthread_exit(NULL);
    }
    connect(peerSock, (sockaddr*)&saddr, sizeof(saddr));
    // if(connect(peerSock, (sockaddr*)&saddr, sizeof(saddr)) < 0) {
    //     std::cout << "Connection error\n";
    //     pthread_exit(NULL);
    // }
    std::cout << "Connection established!\n";
    std::string myData = myName+":"+myUid;
    send(peerSock, myData.c_str(), myData.size(), 0);
    
    connectedPeers[peer_uid] = peerSock;
}

int authentication() {
    char data[4096];
    char received[4096];
    
    while(true) {
        memset(data, 0, 4096);
        memset(received, 0, 4096);
        std::cin.getline(data, 4096);
        
        
        if(strcmp(data, "quit")==0) {
            // close(serverSocket);
            return -1;
        }
        send(trackerSocket, data, strlen(data), 0);
        
        std::cout << "Message sent!\n";
        
        int bytesRecv = recv(trackerSocket, received, 4096, 0);
        if(bytesRecv == -1) {
            std::cout << "There is a connection issue\n";
            return -1;
        }
        if(bytesRecv == 0) {
            std::cout << "The server disconnected\n";
            return -1;
        }
        std::cout << received << std::endl << std::endl;

        std::vector<std::string> res = split(received, ' ');
        if(myName.compare("")==0 && res[1].compare("Welcome")==0) {
            myName = res[2];
            myName = myName.substr(0, myName.size()-1);
            res = split(data, ':');
            myUid = res[0];
            break;
        }
    }
    return 0;
}

long GetFileSize(std::string filename)
{
    struct stat stat_buf;
    int rc = stat(filename.c_str(), &stat_buf);
    return rc == 0 ? stat_buf.st_size : -1;
}

void signalHandler(int signal) {
    // for(auto& it: connectedPeers) {
    //     close(it.second);
    // }
    exit_program=1;
    close(trackerSocket);
    close(serverSocket);
    exit(signal);
}

int main(int argv, char* argc[]) {
    
    signal(SIGINT, signalHandler);
    
    sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(PORT);
    // saddr.sin_addr.s_addr = INADDR_ANY;
    
    trackerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(trackerSocket == 0) {
        std::cout << "Socket error\n";
        return -1;
    }
    inet_pton(AF_INET, "127.0.0.1", &saddr.sin_addr);
    
    if(connect(trackerSocket, (sockaddr*)&saddr, sizeof(saddr)) < 0) {
        std::cout << "Connection error\n";
        return -1;
    }
    
    std::ofstream clientFile("client.txt", std::ios::app);
    
    char received[4096] = {0};
    char data[4096] = {0};
    
    read(trackerSocket, received, 4096);
    
    std::cout << "Connected on port "<< received <<"\n\nRegister | Login\n\n";
    clientFile << "Connected on port "<< received <<"\n\nRegister | Login\n\n";
    clientFile.close();
    
    if(authentication()==-1) {
        close(trackerSocket);
        exit(0);
    }
    //store file details
    std::cin.getline(myFile, 4096);
    std::ifstream file(myFile);

    long fileSize = GetFileSize(myFile);
    int blocks = ceil(fileSize/4096.0);
    std::string shaOfEachBlock = "";

    for(int _=0; _<blocks; _++) {
        memset(data, 0, 4096);
        file.read(data, 4095);
        std::string sha = sha256(data);
        shaOfEachBlock += sha + "_";
    }
    shaOfEachBlock[shaOfEachBlock.size()-1]='\0';

    std::string addData = ":"+myUid+":"+std::to_string(blocks)+":"+std::to_string(fileSize)+":"+shaOfEachBlock;
    strcat(myFile, addData.c_str());
    send(trackerSocket, myFile, strlen(myFile), 0);
    file.close();
    
    pthread_t serverThread;
    pthread_create(&serverThread, NULL, server, argc[1]);
    
    while(true) {
        memset(data, 0, 4096);
        memset(received, 0, 4096);
        
        std::cin.getline(data, 4096);
        
        std::vector<std::string> res = split(data, ':');
        
        if(strcmp(data, "quit")==0) {
            // close(serverSocket);
            break;
        }
        
        if(res[0].compare("connect")==0) {
            //connect:uid
            if(res[1].compare(myUid)==0) {
                std::cout << "You can not connect to yourself!\n";
                continue;
            }
            std::cout << "Trying to connect" << std::endl;
            
            send(trackerSocket, data, strlen(data), 0);
            recv(trackerSocket, received, 4096, 0);
            try {
                int port = atoi(received);
                if(port==0) throw port;
            }
            catch(int port) {
                std::cout << received << std::endl << std::endl;
                continue;
            }
            connectPeer(res[1], atoi(received));
        } else if(res[0].compare("disconnect")==0) {
            //disconnect:uid
            if(res[1].compare(myUid)==0) {
                std::cout << "You can not disconnect to yourself!\n";
                continue;
            }
            close(connectedPeers[res[1]]);
            connectedPeers.erase(res[1]);
            std::cout << "Disconnected peer: " << res[1] << std::endl<<std::endl;
        } else if(res[0].compare("msg")==0) {
            // msg:uid:message
            if(res.size()<3) {
                std::cout << "Invalid..\n\n";
                continue;
            }
            int peerSock = connectedPeers[res[1]];
            int infolen = (res[0]+":"+res[1]+":").size();
            send(peerSock, data+(infolen), strlen(data)-infolen, 0);
            std::cout << "Message sent\n\n";
            
            if(res[2].compare("download_file")==0) {
                // msg:uid:download_file:inilen-finlen
                Download_File_Data* obj = new Download_File_Data;
                std::vector<std::string> limit = split(res[3].c_str(), '-');
                obj->start = stoi(limit[0]);
                obj->end = stoi(limit[1]);
                obj->socket = peerSock;
                
                pthread_t downloadThread;
                pthread_create(&downloadThread, NULL, fileDownload, (void*)obj);
            }
        } else if(strcmp(data, "connections")==0) {
            //You can message only these peers
            std::cout << "You can send messages to only these peers\n";
            for(auto& it: connectedPeers) {
                std::cout << it.first << ":";
            }
            std::cout << "\n\n";
        } else {
            send(trackerSocket, data, strlen(data), 0);
            int bytesRecv = recv(trackerSocket, received, 4096, 0);
            if(bytesRecv == -1) {
                std::cout << "There is a connection issue\n";
                break;
            }
            if(bytesRecv == 0) {
                std::cout << "The server disconnected\n";
                break;
            }
            std::cout << received << std::endl << std::endl;
        }
    }

    // pthread_join(serverThread, 0);
    raise(SIGINT);
    
    return 0;
}