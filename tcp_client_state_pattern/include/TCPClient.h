#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

#include "State.h"
#include "MessageQueue.h"
#include <string>
#include <thread>
#include <mutex>
#include <memory>
#include <chrono>
#include <queue>

class State;

/**
 * TCP客户端上下文类
 * 管理连接状态和网络操作
 */
class TCPClient {
public:
    TCPClient(const std::string& host, int port, int reconnectIntervalMs = 5000);
    ~TCPClient();

    // 禁止拷贝
    TCPClient(const TCPClient&) = delete;
    TCPClient& operator=(const TCPClient&) = delete;

    // 公共接口
    void connect();
    void send(const std::string& data);
    void receive();
    void disconnect();
    
    // 获取当前状态
    std::string getCurrentState() const;
    bool isConnected() const;
    bool isError() const;

    // 心跳检测
    void startHeartbeat(int intervalMs = 10000);
    void stopHeartbeat();

    // 自动重连
    void startAutoReconnect();
    void stopAutoReconnect();

    // 消息队列相关
    void pushMessage(const std::string& msg);
    std::string popMessage();
    bool hasMessage() const;

    // 状态转移（内部使用）
    void setState(std::unique_ptr<State> state);
    
    // 获取网络相关信息
    std::string getHost() const { return host; }
    int getPort() const { return port; }
    int getSocketFd() const { return socketFd; }
    void setSocketFd(int fd) { socketFd = fd; }

    // 错误处理
    void setError(const std::string& errorMsg);
    std::string getLastError() const { return lastError; }

private:
    std::string host;
    int port;
    int socketFd;
    int reconnectIntervalMs;
    std::string lastError;

    // 状态
    std::unique_ptr<State> currentState;
    mutable std::mutex stateMutex;

    // 心跳相关
    std::thread heartbeatThread;
    bool heartbeatRunning;
    int heartbeatIntervalMs;
    mutable std::mutex heartbeatMutex;

    // 自动重连相关
    std::thread reconnectThread;
    bool reconnectRunning;
    int failureCount;
    mutable std::mutex reconnectMutex;

    // 消息队列
    MessageQueue messageQueue;

    // 私有方法
    void heartbeatLoop();
    void reconnectLoop();
    void internalConnect();
    void internalDisconnect();
    bool sendHeartbeat();
};

#endif // TCP_CLIENT_H
