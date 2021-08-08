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

#define PORT 8080

std::vector<std::string> split(char str[], char a) {
    std::vector<std::string> res;
    std::stringstream ss(str);
    std::string temp;
    while(getline(ss, temp, a)) {
        res.push_back(temp);
    }
    return res;
}

std::string greet(std::string name) {
    std::string t = "Welcome from server, ";
    
    return t+name;
}

int main() {
    
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(serverSocket == 0) {
        std::cout << "Can't create a socket!\n";
        return -1;
    }
    sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(PORT);
    // saddr.sin_addr.s_addr = INADDR_ANY
    
    inet_pton(AF_INET, "127.0.0.1", &saddr.sin_addr);
    
    // int opt=1;
    // if(setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)) == 0) {
    //     std::cout << "setsockopt error\n";
    //     return -1;
    // }
    
    // bind server with ip/port
    if(bind(serverSocket, (sockaddr*)&saddr, sizeof(saddr))<0) {
        std::cout << "Bind Error\n";
        return -1;
    }
    
    //Listening
    if(listen(serverSocket, SOMAXCONN) < 0) {
        std::cout << "istening Error\n" << std::endl;
        return -1;
    }
    
    std::ofstream serverFile("server.txt", std::ios::app);
    
    std::cout << "Started listening on: " << PORT << std::endl;
    serverFile << "Started listening on: " << PORT << std::endl;
    
    sockaddr_in caddr;
    socklen_t clientSize = sizeof(caddr);
    
    int clientSocket = accept(serverSocket, (sockaddr*)&caddr, &clientSize);

    if(clientSocket == -1) {
        std::cout << "Problem with client connecting!\n";
        return -1;
    }
    
    // To get the user data
    char host[NI_MAXHOST];
    char serv[NI_MAXSERV];
    memset(host, 0, NI_MAXHOST);
    memset(serv, 0, NI_MAXSERV);
    
    int result = getnameinfo((sockaddr*)&caddr, clientSize, host, NI_MAXHOST, serv, NI_MAXSERV, 0);
    
    if(result) {
        std::cout << host << " connecting on " << serv << std::endl << std::endl;
        serverFile << host << " connecting on " << serv << std::endl << std::endl;
    } else {
        inet_ntop(AF_INET, &caddr.sin_addr, host, NI_MAXHOST);
        std::cout << host << " connecting on " << ntohs(caddr.sin_port) << std::endl << std::endl;
        serverFile << host << " connecting on " << ntohs(caddr.sin_port) << std::endl << std::endl;
    }
    
    // Exchanging data
    char buf[4096];
    while(true) {
        memset(buf, 0, 4096);
        
        int bytesRecv = recv(clientSocket, buf, 4096, 0);
        if(bytesRecv == -1) {
            std::cout << "There is a connection issue\n";
            serverFile << "There is a connection issue\n";
            break;
        }
        if(bytesRecv == 0) {
            std::cout << "The client disconnected\n";
            serverFile << "The client disconnected\n";
            break;
        }

        std::cout << "Received from client: " << buf;
        serverFile << "Received from client: " << buf;
        
        std::vector<std::string> data = split(buf, ' ');
        std::string str = greet(data[data.size()-1]);
        
        send(clientSocket, str.c_str(), str.size(), 0);
        std::cout << "\nMessage sent to the client!\n\n";
        serverFile << "\nMessage sent to the client!\n\n";
        
        data.clear();
    }
    serverFile << "-------------------------------------------------\n\n";
    serverFile.close();
    close(clientSocket);
    close(serverSocket);
    
    return 0;
}