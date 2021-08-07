#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h> 
#include <string>
#include <fstream>

#define PORT 5000

int main() {
    
    sockaddr_in saddr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr = INADDR_ANY
    };
    
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(serverSocket == 0) {
        std::cout << "Socket error\n";
        return -1;
    }
    // inet_pton(AF_INET, "localhost", &saddr.sin_addr);
    // int opt=1;
    // setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    
    if(connect(serverSocket, (sockaddr*)&saddr, sizeof(saddr)) < 0) {
        std::cout << "Connection error\n";
        return -1;
    }
    
    std::ofstream clientFile("client.txt");
    char data[4096];
    char received[4096];
    std::cout << "Connected! You can send message now!\n\n";
    clientFile << "Connected! You can send message now!\n\n";

    while(true) {
        memset(data, 0, 4096);
        memset(received, 0, 4096);
        
        std::cin.getline(data, 4096);
        
        if(data[0] == 'q' && data[1] == '\0') {
            break;
        }
        send(serverSocket, data, sizeof(data), 0);
        
        std::cout << "Sent message to the server\n";
        clientFile << "Sent message to the server\n";
        
        // read(serverSocket, received, 4096);
        
        int bytesRecv = recv(serverSocket, received, 4096, 0);
        if(bytesRecv == -1) {
            std::cout << "There is a connection issue\n";
            clientFile << "There is a connection issue\n";
            break;
        }
        if(bytesRecv == 0) {
            std::cout << "The server disconnected\n";
            clientFile << "The server disconnected\n";
            break;
        }
        
        std::cout << received << std::endl << std::endl;
        clientFile << received << std::endl << std::endl;
    }
    
    close(serverSocket);
    
    return 0;
}