#ifndef STATE_H
#define STATE_H

class TCPClient;

/**
 * 状态基类（抽象状态）
 * 定义各个状态都应该实现的接口
 */
class State {
public:
    virtual ~State() = default;

    // 连接操作
    virtual void connect(TCPClient* client) = 0;
    
    // 发送数据
    virtual void send(TCPClient* client, const std::string& data) = 0;
    
    // 接收数据
    virtual void receive(TCPClient* client) = 0;
    
    // 断开连接
    virtual void disconnect(TCPClient* client) = 0;
    
    // 获取状态名称
    virtual std::string getStateName() const = 0;
};

#endif // STATE_H
