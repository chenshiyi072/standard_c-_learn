#ifndef CONCRETE_STATES_H
#define CONCRETE_STATES_H

#include "State.h"
#include <string>

/**
 * 1. 未连接状态（Disconnected State）
 */
class DisconnectedState : public State {
public:
    void connect(TCPClient* client) override;
    void send(TCPClient* client, const std::string& data) override;
    void receive(TCPClient* client) override;
    void disconnect(TCPClient* client) override;
    std::string getStateName() const override { return "Disconnected"; }
};

/**
 * 2. 连接中状态（Connecting State）
 */
class ConnectingState : public State {
public:
    void connect(TCPClient* client) override;
    void send(TCPClient* client, const std::string& data) override;
    void receive(TCPClient* client) override;
    void disconnect(TCPClient* client) override;
    std::string getStateName() const override { return "Connecting"; }
};

/**
 * 3. 已连接状态（Connected State）
 */
class ConnectedState : public State {
public:
    void connect(TCPClient* client) override;
    void send(TCPClient* client, const std::string& data) override;
    void receive(TCPClient* client) override;
    void disconnect(TCPClient* client) override;
    std::string getStateName() const override { return "Connected"; }
};

/**
 * 4. 关闭中状态（Closing State）
 */
class ClosingState : public State {
public:
    void connect(TCPClient* client) override;
    void send(TCPClient* client, const std::string& data) override;
    void receive(TCPClient* client) override;
    void disconnect(TCPClient* client) override;
    std::string getStateName() const override { return "Closing"; }
};

/**
 * 5. 错误状态（Error State）
 */
class ErrorState : public State {
private:
    std::string errorMsg;

public:
    ErrorState(const std::string& msg = "") : errorMsg(msg) {}
    
    void connect(TCPClient* client) override;
    void send(TCPClient* client, const std::string& data) override;
    void receive(TCPClient* client) override;
    void disconnect(TCPClient* client) override;
    std::string getStateName() const override { return "Error"; }
    std::string getErrorMessage() const { return errorMsg; }
};

#endif // CONCRETE_STATES_H
