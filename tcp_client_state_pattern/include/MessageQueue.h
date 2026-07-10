#ifndef MESSAGE_QUEUE_H
#define MESSAGE_QUEUE_H

#include <queue>
#include <string>
#include <mutex>
#include <condition_variable>

/**
 * 线程安全的消息队列
 */
class MessageQueue {
private:
    std::queue<std::string> messages;
    mutable std::mutex mutex;
    std::condition_variable cv;
    static const int MAX_QUEUE_SIZE = 1000;

public:
    /**
     * 将消息推入队列
     */
    void push(const std::string& msg) {
        std::lock_guard<std::mutex> lock(mutex);
        if (messages.size() < MAX_QUEUE_SIZE) {
            messages.push(msg);
            cv.notify_one();
        }
    }

    /**
     * 从队列中弹出消息（阻塞）
     */
    std::string pop() {
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] { return !messages.empty(); });
        std::string msg = messages.front();
        messages.pop();
        return msg;
    }

    /**
     * 尝试弹出消息（非阻塞）
     */
    bool tryPop(std::string& msg) {
        std::lock_guard<std::mutex> lock(mutex);
        if (messages.empty()) {
            return false;
        }
        msg = messages.front();
        messages.pop();
        return true;
    }

    /**
     * 检查队列是否为空
     */
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex);
        return messages.empty();
    }

    /**
     * 获取队列大小
     */
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex);
        return messages.size();
    }

    /**
     * 清空队列
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex);
        while (!messages.empty()) {
            messages.pop();
        }
    }
};

#endif // MESSAGE_QUEUE_H
