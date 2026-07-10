#include "ConcreteStates.h"
#include "TCPClient.h"
#include <iostream>
#include <sstream>
#include <sys/socket.h>
#include <unistd.h>

// ==================== DisconnectedState ====================

void DisconnectedState::connect(TCPClient* client) {
    std::cout << "[DisconnectedState] Attempting to connect..." << std::endl;
    
    // 转移到连接中状态
    client->setState(std::make_unique<ConnectingState>());
    
    // 执行实际的连接操作
    // 这里会在ConnectingState中进行网络操作
}

void DisconnectedState::send(TCPClient* client, const std::string& data) {
    std::cout << "[DisconnectedState] ERROR: Cannot send data - not connected!" << std::endl;
}

void DisconnectedState::receive(TCPClient* client) {
    std::cout << "[DisconnectedState] ERROR: Cannot receive data - not connected!" << std::endl;
}

void DisconnectedState::disconnect(TCPClient* client) {
    std::cout << "[DisconnectedState] Already disconnected." << std::endl;
}

// ==================== ConnectingState ====================

void ConnectingState::connect(TCPClient* client) {
    std::cout << "[ConnectingState] Connection in progress..." << std::endl;
}

void ConnectingState::send(TCPClient* client, const std::string& data) {
    std::cout << "[ConnectingState] WARNING: Connection not established yet, buffering message..." << std::endl;
    client->pushMessage(data);
}

void ConnectingState::receive(TCPClient* client) {
    std::cout << "[ConnectingState] Waiting for connection to establish..." << std::endl;
}

void ConnectingState::disconnect(TCPClient* client) {
    std::cout << "[ConnectingState] Cancelling connection attempt..." << std::endl;
    client->setState(std::make_unique<DisconnectedState>());
}

// ==================== ConnectedState ====================

void ConnectedState::connect(TCPClient* client) {
    std::cout << "[ConnectedState] Already connected to " 
              << client->getHost() << ":" << client->getPort() << std::endl;
}

void ConnectedState::send(TCPClient* client, const std::string& data) {
    std::cout << "[ConnectedState] Sending data: " << data << std::endl;
    
    int socketFd = client->getSocketFd();
    if (socketFd != -1) {
        // 实际发送数据
        ssize_t sent = ::send(socketFd, data.c_str(), data.length(), 0);
        if (sent < 0) {
            std::cout << "[ConnectedState] Send failed!" << std::endl;
            client->setError("Send failed");
            client->setState(std::make_unique<ErrorState>("Send failed"));
        }
    }
}

void ConnectedState::receive(TCPClient* client) {
    std::cout << "[ConnectedState] Ready to receive data..." << std::endl;
    
    int socketFd = client->getSocketFd();
    if (socketFd != -1) {
        char buffer[4096] = {0};
        ssize_t received = ::recv(socketFd, buffer, sizeof(buffer) - 1, 0);
        
        if (received > 0) {
            buffer[received] = '\0';
            std::cout << "[ConnectedState] Received: " << buffer << std::endl;
            client->pushMessage(std::string(buffer));
        } else if (received == 0) {
            std::cout << "[ConnectedState] Connection closed by peer" << std::endl;
            client->setState(std::make_unique<DisconnectedState>());
        } else {
            std::cout << "[ConnectedState] Receive error" << std::endl;
            client->setError("Receive error");
            client->setState(std::make_unique<ErrorState>("Receive error"));
        }
    }
}

void ConnectedState::disconnect(TCPClient* client) {
    std::cout << "[ConnectedState] Disconnecting..." << std::endl;
    client->setState(std::make_unique<ClosingState>());
}

// ==================== ClosingState ====================

void ClosingState::connect(TCPClient* client) {
    std::cout << "[ClosingState] Cannot connect - currently closing" << std::endl;
}

void ClosingState::send(TCPClient* client, const std::string& data) {
    std::cout << "[ClosingState] Cannot send - connection is closing" << std::endl;
}

void ClosingState::receive(TCPClient* client) {
    std::cout << "[ClosingState] Cannot receive - connection is closing" << std::endl;
}

void ClosingState::disconnect(TCPClient* client) {
    std::cout << "[ClosingState] Finalizing disconnection..." << std::endl;
    client->setState(std::make_unique<DisconnectedState>());
}

// ==================== ErrorState ====================

void ErrorState::connect(TCPClient* client) {
    std::cout << "[ErrorState] Cannot connect - previous error: " << errorMsg << std::endl;
    std::cout << "[ErrorState] Attempting to recover..." << std::endl;
    client->setState(std::make_unique<DisconnectedState>());
}

void ErrorState::send(TCPClient* client, const std::string& data) {
    std::cout << "[ErrorState] Cannot send - in error state: " << errorMsg << std::endl;
}

void ErrorState::receive(TCPClient* client) {
    std::cout << "[ErrorState] Cannot receive - in error state: " << errorMsg << std::endl;
}

void ErrorState::disconnect(TCPClient* client) {
    std::cout << "[ErrorState] Disconnecting from error state..." << std::endl;
    client->setState(std::make_unique<DisconnectedState>());
}
