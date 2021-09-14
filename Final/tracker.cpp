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
#include <map>
#include <csignal>

#define PORT 8000
#define MAX_CLIENT 10
#define TRACKER_FILE "tracker.txt"
#define SERVER_FILE "server.txt"

pthread_mutex_t serverMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t registrationMutex = PTHREAD_MUTEX_INITIALIZER;

void writeInFile(std::string msg) {
    pthread_mutex_lock(&serverMutex);
    // system(std::string("echo "+msg+" >> "+SERVER_FILE).c_str());
    std::ofstream serverFile(SERVER_FILE, std::ios::app);
    serverFile << msg << std::endl;
    serverFile.close();
    pthread_mutex_unlock(&serverMutex);
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

class Client {
    public:
    int id, socket, portNo;
    std::string name, password, uid, servPort;
    Client(int id, int clientSocket) {
        this->id = id;
        this->socket = clientSocket;
    }
    int registerClient();
    int loginClient();
    int changePassword();
    std::string getDetails() {
        return (this->uid+":"+std::to_string(this->portNo));
    }
    void welcome() {
        std::string str = "server> Welcome " + this->name + ", Enter name of the file you wanna share!";
        send(this->socket, str.c_str(), str.size()+1, 0);
    }
};

std::map<std::string, Client*> onlinePeers;
std::map<std::string, std::vector<std::string> > totalPeers;  //vector<string> contains: name, password, uid, portNo

int Client::registerClient() {
    //getting user data in format (peername:password:uniqueID)
    
    char userData[1024] = {0};
    int sizeofUserData = recv(this->socket, userData, 1024, 0);
    if(sizeofUserData <= 0) {
        return -1;
    }
    
    std::vector<std::string> res = split(userData, ':');
    
    if(res.size() != 3) {
        send(this->socket, "server> Invalid! Try again...", 30, 0);
        return this->registerClient();
    }
    
    //checking if uniqueID is actually unique
    if(totalPeers.find(res[2]) != totalPeers.end()) {
        send(this->socket, "server> uniqueID is already in use. Try using different ID", 59, 0);
        return this->registerClient();
    }
    
    //adding user to totalPeers
    res.push_back(std::to_string(this->portNo));
    totalPeers[this->uid] = res;
    
    this->name = res[0];
    this->password = res[1];
    this->uid = res[2];
    
    return 0;
}

int Client::loginClient() {
    //getting userdata in formmat (uniqueID:password)
    
    char userData[1024] = {0};
    int sizeofuserData = recv(this->socket, userData, 1024, 0);
    if(sizeofuserData <= 0) {
        return -1;
    }
    
    std::vector<std::string> res = split(userData, ':');
    
    if(res.size() != 2) {
        send(this->socket, "server> Invalid! Try again...", 30, 0);
        return this->loginClient();
    }
    
    //checking if client is already logged in
    if(onlinePeers.find(res[0]) != onlinePeers.end()) {
        send(this->socket, "server> Already logged in somewhere else", 41, 0);
        return this->loginClient();
    }
     
    //updating totalPeers and client info
    if(totalPeers.find(res[0])!=totalPeers.end()) {
        if(totalPeers[res[0]][1].compare(res[1])==0) {
            totalPeers[res[0]].pop_back();
            totalPeers[res[0]].push_back(std::to_string(this->portNo));

            this->name = totalPeers[res[0]][0];
            this->password = totalPeers[res[0]][1];
            this->uid = totalPeers[res[0]][2];
            return 0;
        }
    }
    
    send(this->socket, "server> Credentials do not match! Try again", 44, 0);
    return this->loginClient();
}

int Client::changePassword() {
    // format(current_password:new_password)
    char data[1024] = {0};
    int recvBytes = recv(this->socket, data, 1024, 0);
    if(recvBytes <= 0) {
        std::cout << "The client " << this->id << " has disconnected\n";
        return -1;
    }
    std::vector<std::string> res = split(data, ':');
    
    if(res.size()!=2) {
        send(this->socket, "server> Invalid format. Try again...", 37, 0);
        return this->changePassword();
    }
    
    if(res[0].compare(this->password)!=0) {
        send(this->socket, "server> Incorrect password. Try again...", 41, 0);
        return this->changePassword();
    }
    
    this->password = res[1];
    
    //updating in totalPeers
    totalPeers[this->uid][1] = this->password;
    
    return 0;
}

std::string greet(std::string name) {
    return "server> Welcome from server, "+name;
}


std::string getOnlinePeers() {
    std::string res = "";
    for(auto &it: onlinePeers) {
        res += it.first + ":";
    }
    res[res.size()-1] = '\0';
    return res;
}

int authenticateClient(Client* client) {
    int clientSocket = client->socket;
    int clientID = client->id;
    char msg[1024];
    while(true) {
        memset(msg, 0, 1024);
        int bytesrecv = recv(clientSocket, msg, 1024, 0);
        if(bytesrecv <=0 ) {
            return -1;
        }
        if(strcmp(msg, "Register") == 0 || strcmp(msg, "register") == 0) {
            //register client
            pthread_mutex_lock(&registrationMutex);
            send(clientSocket, "server> Registration format(Name:Password:uniqueID)", 52, 0);
            int res = client->registerClient();
            pthread_mutex_unlock(&registrationMutex);
            return res;
        } else if(strcmp(msg, "Login") == 0 || strcmp(msg, "login") == 0){
            //login client
            send(clientSocket, "server> Login format(uniqueID:Password)", 40, 0);
            int res = client->loginClient();
            return res;
        } else {
            send(clientSocket, "server> Retry...", 17, 0);
        }
    }
    return 0;
}
std::map<std::string, std::vector<std::string>> filesData;  //filename->details

void *Messaging(void* arg) {
    Client* client = (Client*)arg;
    int clientID = client->id;
    int clientSocket = client->socket;
    char msg[4096] = {0};
    
    std::cout << "Waiting for user input...\n";
    
    if(authenticateClient(client) == -1) {
        std::cout << "The client " << clientID << " has disconnected\n";
        close(clientSocket);
        pthread_exit(NULL);
    }
    std::cout << "Client successfully logged in!\n";
    
    //adding client to online peers
    onlinePeers[client->uid] = client;
        
    client->welcome();
    
    //get file details
    recv(clientSocket, msg, 4096, 0);
    std::vector<std::string> fileDetails = split(msg, ':');
    
    //saving file details in filesData(map)
    filesData[fileDetails[0]].push_back(std::string(msg));
    
    //get the server port of the peer
    memset(msg, 0, 4096);
    recv(clientSocket, msg, 4096, 0);
    client->servPort = msg;

    std::cout << "The Client " << clientID << " is ready to exchange messages\n\n";
    while(true) {
        memset(msg, 0, 4096);
        
        int bytesRecv = recv(clientSocket, msg, 4096, 0);
        if(bytesRecv == -1) {
            std::cout << "There is a connection issue with client " << clientID << "\n";
            break;
        }
        if(bytesRecv == 0) {
            std::cout << "The client " << clientID << " has disconnected\n";
            break;
        }

        std::cout << "Received from client " << clientID << ": " << msg << std::endl;
        writeInFile("Received from client "+std::to_string(clientID)+": "+std::string(msg));
        std::vector<std::string> recv_data = split(msg, ':');
        
        if(strcmp(msg, "get my details")==0) {
            std::string details = "server> " + client->getDetails();
            send(clientSocket, details.c_str(), details.size(), 0);
        } else if(recv_data[0].compare("get online peers")==0) {
            //get online peers
            std::string res;
            res = getOnlinePeers();
            res = "server> " + res;
            send(clientSocket, res.c_str(), res.size(), 0);
        } else if(recv_data[0].compare("connect")==0) {
            //connect:uid
            if(recv_data.size()!=2) {
                send(clientSocket, "server> Invalid.. Try again!", 29, 0);
            } else if(onlinePeers.find(recv_data[1]) == onlinePeers.end()) {
                send(clientSocket, "server> Client is offline", 26, 0);
            } else {
                std::string port = onlinePeers[recv_data[1]]->servPort;
                std::cout << onlinePeers[recv_data[1]]->servPort << std::endl;
                send(clientSocket, port.c_str(), port.size(), 0);
            }
        } else if(strcmp(msg, "change password")==0) {
            send(clientSocket, "server> Format(curr_password:new_password)", 43, 0);
            int result = client->changePassword();
            if(result == -1) break;
            else {
                send(clientSocket, "server> Password changed successfully!", 39, 0);
            }
        } else if(strcmp(msg, "commands")==0) {
            std::string cmds = "server> 1. get my details\n\t2. get online peers\n\t3. connect:uid\n\t4. get files\n\t5. msg:uid:message (first connect to the peer)\n\t6. disconnect:uid\n\t7. change password";
            send(clientSocket, cmds.c_str(), cmds.size(), 0);
        } else if(strcmp(msg, "get files")==0) {
            //get files
            std::string res = "server> ";
            for(auto &it: filesData) {
                res+=it.first+"\n\t";
            }
            send(clientSocket, res.c_str(), res.size()-2, 0);
            memset(msg, 0, 4096);
            recv(clientSocket, msg, 4096, 0); // gets the name of the file to download
            std::vector<std::string> fileData = filesData[std::string(msg)];
            res="server> ";
            for(auto &it: fileData) {
                res += it + "\n\t";
            }
            send(clientSocket, res.c_str(), res.size(), 0);
        } else {
            std::string str;
            if(recv_data.size() != 0) {
                str = greet(recv_data[recv_data.size()-1]);
            } else {
                str = "server> Invaild";
            }
            send(clientSocket, str.c_str(), str.size(), 0);
        }
        recv_data.clear();
        std::cout << "Message sent to the client " << clientID << "\n\n";
        writeInFile("Message sent to the client "+std::to_string(clientID));
    }
    
    //removing client from online peers
    onlinePeers.erase(client->uid);
    
    close(clientSocket);
    pthread_exit(NULL);
}

void signal_handler(int signum) {
    std::ofstream trackerFile(TRACKER_FILE, std::ios::trunc);
    for(auto &it: totalPeers) {
        std::vector<std::string> user = it.second;
        trackerFile << user[0]+":"+user[1]+":"+user[2]+":"+user[3] << std::endl;
    }
    trackerFile.close();
    exit(signum);
}

int main() {
    
    signal(SIGINT, signal_handler); //calls signal_handler when someone presses control+c
    
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
        std::cout << "Listening Error\n" << std::endl;
        return -1;
    }
    
    std::ifstream tracker(TRACKER_FILE); 
    std::string user;
    while(getline(tracker, user)) {
        std::vector<std::string> data = split(user.c_str(), ':');
        totalPeers[data[2]] = data;
    }
    tracker.close();
    
    std::cout << "Started listening on: " << PORT << std::endl;
    writeInFile("Started listening on: "+std::to_string(PORT));

    pthread_t threads[MAX_CLIENT];
    
    for(int i=0; i<MAX_CLIENT; i++) {
        
        sockaddr_in caddr;
        socklen_t clientSize = sizeof(caddr);
        int clientSocket = accept(serverSocket, (sockaddr*)&caddr, &clientSize);
        
        if(clientSocket == -1) {
            std::cout << "Problem with client connecting!\n";
            continue;
        }
        std::cout << "\nNew Client connected\n";
        writeInFile("New Client connected");
        
        // To get the user data
        char host[NI_MAXHOST] = {0};
        char serv[NI_MAXSERV] = {0};
        
        int result = getnameinfo((sockaddr*)&caddr, clientSize, host, NI_MAXHOST, serv, NI_MAXSERV, 0);
        
        if(result) {
            std::cout << host << " connecting on " << serv << std::endl << std::endl;
            writeInFile(std::string(std::string(host)+" connecting on "+std::string(serv)));
        } else {
            inet_ntop(AF_INET, &caddr.sin_addr, host, NI_MAXHOST);
            std::cout << host << " connecting on " << ntohs(caddr.sin_port) << std::endl << std::endl;
            writeInFile(std::string(std::string(host)+" connecting on "+std::to_string(ntohs(caddr.sin_port))));
        }
        
        Client* client = new Client(i+1, clientSocket);
        send(clientSocket, serv, sizeof(serv), 0);
        client->portNo = atoi(serv);
        
        if(pthread_create(&threads[i], NULL, Messaging, (void*)client)) {
            std::cout << "Error creating thread\n";
            continue;
        }
    }
    for(int i=0; i<MAX_CLIENT; i++) {
        pthread_join(threads[i], NULL);
    }
    close(serverSocket);
    raise(SIGINT);
    return 0;
}