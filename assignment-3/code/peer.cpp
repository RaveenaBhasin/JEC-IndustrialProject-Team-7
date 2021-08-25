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

#define PORT 8000
#define MAX_CLIENTS 10

class Client_Data {
    public:
    int socket;
};

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
    Client_Data* data = (Client_Data*) arg;
    int clientSocket = data->socket;
    
    char msg[1024] = {0};
    
    while(true) {
        memset(msg, 0, 1024);
        int bytesRecv = recv(clientSocket, msg, 1024, 0);
        if(bytesRecv <= 0) {
            std::cout << "Client disconnected!" << std::endl;
            break;
        }
        std::cout << msg << std::endl;
        // memset(msg, 0, 1024);
        // std::cin.getline(msg, 1024);
    }
    pthread_exit(NULL);
}

void* server(void* arg) {
    char* port = (char*) arg;
    int portNo = atoi(port);
    
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
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
    std::cout << "Strated Listening on " << portNo << std::endl;
    
    pthread_t threads[MAX_CLIENTS];
    
    for(int i=0; i<MAX_CLIENTS; i++) {
        
        sockaddr_in caddr;
        socklen_t socklen = sizeof(caddr);
        int clientSocket = accept(serverSocket, (sockaddr*)&caddr, &socklen);
        std::cout << "someone is connected!" << std::endl;
        
        Client_Data* data = new Client_Data;
        data->socket = clientSocket;
        
        if(pthread_create(&threads[i], NULL, receiving, (void*)data)) {
            std::cout << "Error creating thread\n";
            continue;
        }
        
    }
    for(int i=0; i<MAX_CLIENTS; i++) {
        pthread_join(threads[i], NULL);
    }
    pthread_exit(NULL);
}
void* tempclient(void* arg) {
    char* servPort = (char*) arg;
    int port = atoi(servPort);
    int tempServerSock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(port);
    
    if(inet_pton(AF_INET, "127.0.0.1", &saddr.sin_addr) < 0) {
        std::cout << "Error\n";
        pthread_exit(NULL);
    }
    
    if(tempServerSock == 0) {
        std::cout << "Error in creating socket!\n";
        pthread_exit(NULL);
    }
    
    if(connect(tempServerSock, (sockaddr*)&saddr, sizeof(saddr)) < 0) {
        std::cout << "Connection error\n";
        pthread_exit(NULL);
    }
    std::cout << "Connection established!\n";
    char msg[1024];
    
    while(true) {
        memset(msg, 0, 1024);
        std::cin.getline(msg, 1024);
        if((msg[0] == 'q' && msg[1] == '\0')) {
            break;
        }
        send(tempServerSock, msg, strlen(msg), 0);
    }
    close(tempServerSock);
    pthread_exit(NULL);
}
void* client(void* arg) {
    char data[4096] = {0};
    char received[4096] = {0};
    Client_Data* obj = (Client_Data*) arg;
    int serverSocket = obj->socket;
    while(true) {
        memset(data, 0, 4096);
        memset(received, 0, 4096);
        
        std::cin.getline(data, 4096);
        
        std::vector<std::string> res = split(data, ':');
        
        if((data[0] == 'q' && data[1] == '\0') || data[0] == '\0') {
            break;
        }
        if(res[0].compare("connect")==0) {
            std::cout << "Trying to connect" << std::endl;
            
            send(serverSocket, data, strlen(data), 0);
            memset(data, 0, 4096);
            read(serverSocket, data, 4096);
            
            pthread_t tempcli;
            pthread_create(&tempcli, NULL, tempclient, data);
            pthread_join(tempcli, NULL);
            std::cout << "Joining back!\n" << std::endl;
            continue;
        }
        send(serverSocket, data, strlen(data), 0);
        
        std::cout << "Message sent!\n";
        
        // read(serverSocket, received, 4096);
        
        int bytesRecv = recv(serverSocket, received, 4096, 0);
        if(bytesRecv == -1) {
            std::cout << "There is a connection issue\n";
            break;
        }
        if(bytesRecv == 0) {
            std::cout << "The server disconnected\n";
            break;
        }
        
        std::cout << "server> " << received << std::endl << std::endl;
    }
    pthread_exit(NULL);
}

int main() {
    
    sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(PORT);
    // saddr.sin_addr.s_addr = INADDR_ANY;
    
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(serverSocket == 0) {
        std::cout << "Socket error\n";
        return -1;
    }
    
    inet_pton(AF_INET, "127.0.0.1", &saddr.sin_addr);
    
    if(connect(serverSocket, (sockaddr*)&saddr, sizeof(saddr)) < 0) {
        std::cout << "Connection error\n";
        return -1;
    }
    
    Client_Data* obj = new Client_Data;
    obj->socket = serverSocket;
    
    std::ofstream clientFile("client.txt", std::ios::app);
    char received[4096] = {0};
    char servPort[10] = {0};
    
    read(serverSocket, received, 4096);
    read(serverSocket, servPort, 10);
    
    std::cout << "Connected on port "<< received <<"\n\nRegister | Login\n\n";
    clientFile << "Connected on port "<< received <<"\n\nRegister | Login\n\n";
    
    pthread_t serverThread, clientThread;
    pthread_create(&serverThread, NULL, server, servPort);
    
    pthread_create(&clientThread, NULL, client, (void*)obj);
    
    pthread_join(serverThread, 0);
    pthread_join(clientThread, 0);
    
    clientFile.close();
    close(serverSocket);
    
    return 0;
}