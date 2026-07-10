#include "TCPClient.h"
#include "ConcreteStates.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <algorithm>

TCPClient::TCPClient(const std::string& host, int port, int reconnectIntervalMs)
    : host(host),
      port(port),
      socketFd(-1),
      reconnectIntervalMs(reconnectIntervalMs),
      currentState(std::make_unique<DisconnectedState>()),
      heartbeatRunning(false),
      heartbeatIntervalMs(10000),
      reconnectRunning(false),
      failureCount(0) {
    std::cout << "[TCPClient] Initialized with " << host << ":" << port << std::endl;
}

TCPClient::~TCPClient() {
    stopHeartbeat();
    stopAutoReconnect();
    disconnect();
}

void TCPClient::connect() {
    std::lock_guard<std::mutex> lock(stateMutex);
    currentState->connect(this);
}

void TCPClient::send(const std::string& data) {
    std::lock_guard<std::mutex> lock(stateMutex);
    currentState->send(this, data);
}

void TCPClient::receive() {
    std::lock_guard<std::mutex> lock(stateMutex);
    currentState->receive(this);
}

void TCPClient::disconnect() {
    std::lock_guard<std::mutex> lock(stateMutex);
    currentState->disconnect(this);
}

std::string TCPClient::getCurrentState() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return currentState->getStateName();
}

bool TCPClient::isConnected() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return currentState->getStateName() == "Connected";
}

bool TCPClient::isError() const {
    std::lock_guard<std::mutex> lock(stateMutex);
    return currentState->getStateName() == "Error";
}

void TCPClient::setState(std::unique_ptr<State> state) {
    std::lock_guard<std::mutex> lock(stateMutex);
    std::cout << "[TCPClient] State transition: " 
              << currentState->getStateName() << " -> " 
              << state->getStateName() << std::endl;
    currentState = std::move(state);
}

void TCPClient::setError(const std::string& errorMsg) {
    lastError = errorMsg;
    std::cout << "[TCPClient] Error: " << errorMsg << std::endl;
}

void TCPClient::pushMessage(const std::string& msg) {
    messageQueue.push(msg);
}

std::string TCPClient::popMessage() {
    return messageQueue.pop();
}

bool TCPClient::hasMessage() const {
    return !messageQueue.empty();
}

void TCPClient::startHeartbeat(int intervalMs) {
    {
        std::lock_guard<std::mutex> lock(heartbeatMutex);
        if (heartbeatRunning) {
            std::cout << "[TCPClient] Heartbeat already running" << std::endl;
            return;
        }
        heartbeatRunning = true;
        heartbeatIntervalMs = intervalMs;
    }

    heartbeatThread = std::thread(&TCPClient::heartbeatLoop, this);
    std::cout << "[TCPClient] Heartbeat started (interval: " << intervalMs << "ms)" << std::endl;
}

void TCPClient::stopHeartbeat() {
    {
        std::lock_guard<std::mutex> lock(heartbeatMutex);
        heartbeatRunning = false;
    }

    if (heartbeatThread.joinable()) {
        heartbeatThread.join();
    }
    std::cout << "[TCPClient] Heartbeat stopped" << std::endl;
}

void TCPClient::heartbeatLoop() {
    while (true) {
        {
            std::lock_guard<std::mutex> lock(heartbeatMutex);
            if (!heartbeatRunning) {
                break;
            }
        }

        if (isConnected()) {
            if (!sendHeartbeat()) {
                std::cout << "[TCPClient] Heartbeat failed, marking as error" << std::endl;
                setError("Heartbeat failed");
                setState(std::make_unique<ErrorState>("Heartbeat failed"));
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(heartbeatIntervalMs));
    }
}

bool TCPClient::sendHeartbeat() {
    if (socketFd == -1) {
        return false;
    }
    
    const char* heartbeatMsg = "PING";
    ssize_t sent = ::send(socketFd, heartbeatMsg, strlen(heartbeatMsg), MSG_DONTWAIT);
    return sent > 0;
}

void TCPClient::startAutoReconnect() {
    {
        std::lock_guard<std::mutex> lock(reconnectMutex);
        if (reconnectRunning) {
            std::cout << "[TCPClient] Auto-reconnect already running" << std::endl;
            return;
        }
        reconnectRunning = true;
        failureCount = 0;
    }

    reconnectThread = std::thread(&TCPClient::reconnectLoop, this);
    std::cout << "[TCPClient] Auto-reconnect started (interval: " << reconnectIntervalMs << "ms)" << std::endl;
}

void TCPClient::stopAutoReconnect() {
    {
        std::lock_guard<std::mutex> lock(reconnectMutex);
        reconnectRunning = false;
    }

    if (reconnectThread.joinable()) {
        reconnectThread.join();
    }
    std::cout << "[TCPClient] Auto-reconnect stopped" << std::endl;
}

void TCPClient::reconnectLoop() {
    while (true) {
        {
            std::lock_guard<std::mutex> lock(reconnectMutex);
            if (!reconnectRunning) {
                break;
            }
        }

        if (isError() || (getCurrentState() == "Disconnected" && failureCount > 0)) {
            std::cout << "[TCPClient] Attempting to reconnect (attempt " 
                      << (failureCount + 1) << ")..." << std::endl;
            connect();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(reconnectIntervalMs));
    }
}

void TCPClient::internalConnect() {
    // TODO: 实现实际的TCP连接
    std::cout << "[TCPClient] Internal connect implementation" << std::endl;
}

void TCPClient::internalDisconnect() {
    if (socketFd != -1) {
        ::close(socketFd);
        socketFd = -1;
    }
}
