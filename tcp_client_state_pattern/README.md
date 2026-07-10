# TCP客户端 + State设计模式

一个完整的C++ TCP客户端实现，使用State设计模式管理连接生命周期。

## 项目结构

```
tcp_client_state_pattern/
├── include/
│   ├── State.h              # 状态基类
│   ├── ConcreteStates.h     # 具体状态类声明
│   ├── TCPClient.h          # TCP客户端上下文
│   └── MessageQueue.h       # 线程安全消息队列
├── src/
│   ├── ConcreteStates.cpp   # 具体状态类实现
│   └── TCPClient.cpp        # TCP客户端实现
├── main.cpp                 # 示例程序
├── CMakeLists.txt           # 构建配置
├── README.md                # 本文件
└── DESIGN.md                # 详细设计文档
```

## 核心特性

### 1. State设计模式

#### 状态转移流程
```
Disconnected
    ↓
Connecting
    ↓
Connected
    ↓
Closing
    ↓
Disconnected

(任何状态可转到 Error)
```

#### 状态说明

| 状态 | 描述 | 允许操作 |
|------|------|----------|
| **DisconnectedState** | 未连接 | 只能发起connect |
| **ConnectingState** | 连接中 | 等待连接完成或取消 |
| **ConnectedState** | 已连接 | 可以send/receive |
| **ClosingState** | 关闭中 | 等待完全关闭 |
| **ErrorState** | 错误 | 需要recovery |

### 2. 消息队列（Thread-Safe）

```cpp
class MessageQueue {
public:
    void push(const std::string& msg);           // 阻塞式入队
    std::string pop();                            // 阻塞式出队
    bool tryPop(std::string& msg);               // 非阻塞出队
    bool empty() const;                          // 检查是否为空
    size_t size() const;                         // 获取队列大小
    void clear();                                // 清空队列
};
```

### 3. 心跳检测

- 定期发送PING信号
- 自动检测僵尸连接
- 可配置检测间隔
- 线程安全实现

```cpp
client.startHeartbeat(10000);  // 每10秒发送一次心跳
client.stopHeartbeat();
```

### 4. 自动重连机制

- 连接失败时自动重连
- 可配置重连间隔
- 失败计数统计
- 指数退避策略可选

```cpp
client.startAutoReconnect();  // 启动自动重连
client.stopAutoReconnect();
```

### 5. 线程安全

- 所有public方法都是线程安全的
- 使用互斥锁保护状态转移
- 使用条件变量同步消息队列
- 心跳和重连运行在独立线程

## 使用示例

### 基础用法

```cpp
#include "TCPClient.h"

int main() {
    // 创建客户端
    TCPClient client("127.0.0.1", 8080);

    // 连接到服务器
    client.connect();

    // 等待连接建立
    while (!client.isConnected()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 发送数据
    client.send("Hello Server");

    // 接收数据
    client.receive();
    if (client.hasMessage()) {
        std::string msg = client.popMessage();
        std::cout << "Received: " << msg << std::endl;
    }

    // 断开连接
    client.disconnect();

    return 0;
}
```

### 启用心跳和自动重连

```cpp
TCPClient client("127.0.0.1", 8080, 5000);  // 5秒重连间隔

// 启用心跳检测（每10秒一次）
client.startHeartbeat(10000);

// 启用自动重连
client.startAutoReconnect();

// 业务逻辑
client.connect();
while (isRunning) {
    if (client.isConnected()) {
        client.send("data");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

// 清理
client.stopHeartbeat();
client.stopAutoReconnect();
client.disconnect();
```

### 错误处理

```cpp
if (client.isError()) {
    std::cout << "Error: " << client.getLastError() << std::endl;
    // 自动重连机制会尝试恢复
}

if (!client.isConnected()) {
    std::cout << "State: " << client.getCurrentState() << std::endl;
}
```

## 编译和运行

### 使用CMake

```bash
mkdir build
cd build
cmake ..
make
./tcp_client_demo
```

### 手动编译

```bash
g++ -std=c++17 -pthread -I./include \
    src/TCPClient.cpp src/ConcreteStates.cpp main.cpp \
    -o tcp_client_demo

./tcp_client_demo
```

## 设计模式优势

### 1. 消除条件判断

**不使用State模式的问题：**
```cpp
// 大量的if-else判断
if (state == DISCONNECTED) {
    // 处理未连接状态的逻辑
} else if (state == CONNECTING) {
    // 处理连接中的逻辑
} else if (state == CONNECTED) {
    // 处理已连接的逻辑
}
```

**使用State模式的优势：**
```cpp
// 清晰的状态转移
currentState->connect(this);
// 不同状态会调用不同的实现
```

### 2. 便于扩展

添加新状态只需：
1. 创建新的State子类
2. 实现虚函数
3. 在合适的地方转移到新状态

### 3. 单一职责原则

每个状态类只负责一种状态的行为，易于维护和测试

### 4. 开闭原则

对扩展开放，对修改关闭。添加新功能不需要修改现有代码

## 扩展建议

### 1. SSL/TLS支持

```cpp
class SecureConnectedState : public ConnectedState {
    // 使用OpenSSL加密通信
};
```

### 2. 连接超时

```cpp
class ConnectingState : public State {
private:
    std::chrono::steady_clock::time_point startTime;
    static const int CONNECT_TIMEOUT_MS = 30000;
};
```

### 3. 自定义协议解析

```cpp
class ProtocolParser {
    Message parseMessage(const std::string& raw);
    std::string serializeMessage(const Message& msg);
};
```

### 4. 连接池管理

```cpp
class TCPClientPool {
    std::vector<TCPClient> clients;
    TCPClient* getConnection();
};
```

### 5. 性能监控

```cpp
struct Statistics {
    int messagesSent;
    int messagesReceived;
    int connectionAttempts;
    double avgResponseTime;
};
```

## 性能考虑

### 内存占用

```
TCPClient对象:       ~200字节
MessageQueue(满):    ~1000条消息 × 50字节 ≈ 50KB
线程栈:               每个线程 8MB
总体:                <20MB（正常使用）
```

### CPU占用

- 主线程：处理业务逻辑
- 心跳线程：每10秒唤醒一次（基本不占CPU）
- 重连线程：每5秒唤醒一次（基本不占CPU）

## 线程安全性

所有public方法都是线程安全的：
- `connect()`, `send()`, `receive()`, `disconnect()`
- `getCurrentState()`, `isConnected()`, `isError()`
- `startHeartbeat()`, `stopHeartbeat()`
- `startAutoReconnect()`, `stopAutoReconnect()`
- 消息队列操作都是原子的

## 使用场景

1. **游戏服务器连接** - 长连接、心跳检测、自动重连
2. **分布式系统** - 节点间通信、服务发现
3. **IoT设备管理** - 不稳定网络、自动重连
4. **实时数据传输** - 行情推送、消息队列
5. **监控告警系统** - 长连接、状态管理

## 许可证

MIT License

## 贡献

欢迎提交Issue和Pull Request
