#include <iostream>
#include <thread>
#include <chrono>
#include "include/TCPClient.h"

int main() {
    std::cout << "========== TCP Client with State Pattern Demo ==========" << std::endl;
    std::cout << std::endl;

    // 创建TCP客户端实例
    TCPClient client("127.0.0.1", 8080, 5000);

    std::cout << "\n[DEMO] 1. 初始状态" << std::endl;
    std::cout << "Current State: " << client.getCurrentState() << std::endl;

    std::cout << "\n[DEMO] 2. 尝试发送数据（未连接）" << std::endl;
    client.send("Hello Server");

    std::cout << "\n[DEMO] 3. 发起连接" << std::endl;
    client.connect();
    std::cout << "Current State: " << client.getCurrentState() << std::endl;

    std::cout << "\n[DEMO] 4. 启动心跳检测" << std::endl;
    client.startHeartbeat(5000);

    std::cout << "\n[DEMO] 5. 启动自动重连" << std::endl;
    client.startAutoReconnect();

    std::cout << "\n[DEMO] 6. 等待3秒..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));

    std::cout << "\n[DEMO] 7. 发送数据" << std::endl;
    client.send("Test message 1");

    std::cout << "\n[DEMO] 8. 接收数据" << std::endl;
    client.receive();

    std::cout << "\n[DEMO] 9. 再次尝试发送" << std::endl;
    client.send("Test message 2");

    std::cout << "\n[DEMO] 10. 等待2秒..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "\n[DEMO] 11. 断开连接" << std::endl;
    client.disconnect();
    std::cout << "Current State: " << client.getCurrentState() << std::endl;

    std::cout << "\n[DEMO] 12. 停止心跳和自动重连" << std::endl;
    client.stopHeartbeat();
    client.stopAutoReconnect();

    std::cout << "\n========== Demo Complete ==========" << std::endl;

    return 0;
}
