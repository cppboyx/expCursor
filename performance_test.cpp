#include "websocket_client.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <iomanip>

class PerformanceTest {
private:
    std::atomic<int> messages_sent_{0};
    std::atomic<int> messages_received_{0};
    std::atomic<int> errors_{0};
    std::chrono::high_resolution_clock::time_point start_time_;
    std::chrono::high_resolution_clock::time_point end_time_;

public:
    void runLatencyTest() {
        std::cout << "=== 延迟测试 ===" << std::endl;
        
        websocket::WebSocketClient client;
        
        client.setMessageCallback([this](const std::string& message) {
            messages_received_++;
        });
        
        client.setErrorCallback([this](const websocket::WebSocketError& error) {
            errors_++;
            std::cout << "错误: " << error.toString() << std::endl;
        });
        
        if (client.connect("wss://echo.websocket.org")) {
            std::cout << "连接成功，开始延迟测试..." << std::endl;
            
            start_time_ = std::chrono::high_resolution_clock::now();
            
            // 发送100条消息测试延迟
            for (int i = 0; i < 100; ++i) {
                std::string message = "Latency test message " + std::to_string(i);
                if (client.send(message)) {
                    messages_sent_++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            // 等待所有响应
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
            end_time_ = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time_ - start_time_);
            
            std::cout << "延迟测试结果:" << std::endl;
            std::cout << "发送消息数: " << messages_sent_.load() << std::endl;
            std::cout << "接收消息数: " << messages_received_.load() << std::endl;
            std::cout << "错误数: " << errors_.load() << std::endl;
            std::cout << "总时间: " << duration.count() << "ms" << std::endl;
            std::cout << "平均延迟: " << (duration.count() / messages_sent_.load()) << "ms/消息" << std::endl;
            
            client.disconnect();
        }
    }
    
    void runThroughputTest() {
        std::cout << "\n=== 吞吐量测试 ===" << std::endl;
        
        websocket::WebSocketClient client;
        
        client.setMessageCallback([this](const std::string& message) {
            messages_received_++;
        });
        
        client.setErrorCallback([this](const websocket::WebSocketError& error) {
            errors_++;
        });
        
        if (client.connect("wss://echo.websocket.org")) {
            std::cout << "连接成功，开始吞吐量测试..." << std::endl;
            
            start_time_ = std::chrono::high_resolution_clock::now();
            
            // 快速发送1000条消息
            for (int i = 0; i < 1000; ++i) {
                std::string message = "Throughput test " + std::to_string(i);
                if (client.send(message)) {
                    messages_sent_++;
                }
            }
            
            // 等待响应
            std::this_thread::sleep_for(std::chrono::seconds(10));
            
            end_time_ = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time_ - start_time_);
            
            std::cout << "吞吐量测试结果:" << std::endl;
            std::cout << "发送消息数: " << messages_sent_.load() << std::endl;
            std::cout << "接收消息数: " << messages_received_.load() << std::endl;
            std::cout << "错误数: " << errors_.load() << std::endl;
            std::cout << "总时间: " << duration.count() << "ms" << std::endl;
            std::cout << "吞吐量: " << (messages_sent_.load() * 1000.0 / duration.count()) << " 消息/秒" << std::endl;
            
            client.disconnect();
        }
    }
    
    void runCompressionPerformanceTest() {
        std::cout << "\n=== 压缩性能测试 ===" << std::endl;
        
        // 测试无压缩
        std::cout << "测试无压缩..." << std::endl;
        testCompression(false);
        
        // 测试有压缩
        std::cout << "测试有压缩..." << std::endl;
        testCompression(true);
    }
    
    void testCompression(bool use_compression) {
        websocket::WebSocketConfig config;
        config.enableCompression(use_compression);
        config.setCompressionLevel(6);
        
        websocket::WebSocketClient client(config);
        
        client.setMessageCallback([this](const std::string& message) {
            messages_received_++;
        });
        
        client.setErrorCallback([this](const websocket::WebSocketError& error) {
            errors_++;
        });
        
        if (client.connect("wss://echo.websocket.org")) {
            start_time_ = std::chrono::high_resolution_clock::now();
            
            // 发送大量数据
            std::string large_data(10000, 'A');
            for (int i = 0; i < 50; ++i) {
                if (client.send(large_data)) {
                    messages_sent_++;
                }
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
            end_time_ = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time_ - start_time_);
            
            std::cout << (use_compression ? "压缩" : "无压缩") << "测试结果:" << std::endl;
            std::cout << "发送消息数: " << messages_sent_.load() << std::endl;
            std::cout << "接收消息数: " << messages_received_.load() << std::endl;
            std::cout << "总时间: " << duration.count() << "ms" << std::endl;
            std::cout << "数据传输量: " << (messages_sent_.load() * 10000) << " 字节" << std::endl;
            std::cout << "传输速率: " << (messages_sent_.load() * 10000.0 / duration.count()) << " 字节/毫秒" << std::endl;
            
            client.disconnect();
        }
    }
    
    void runMemoryTest() {
        std::cout << "\n=== 内存使用测试 ===" << std::endl;
        
        std::vector<std::unique_ptr<websocket::WebSocketClient>> clients;
        
        std::cout << "创建多个WebSocket客户端..." << std::endl;
        
        for (int i = 0; i < 10; ++i) {
            auto client = std::make_unique<websocket::WebSocketClient>();
            
            client->setMessageCallback([](const std::string& message) {
                // 空回调
            });
            
            client->setErrorCallback([](const websocket::WebSocketError& error) {
                // 空回调
            });
            
            clients.push_back(std::move(client));
        }
        
        std::cout << "成功创建 " << clients.size() << " 个WebSocket客户端" << std::endl;
        
        // 连接所有客户端
        for (auto& client : clients) {
            client->connect("wss://echo.websocket.org");
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // 发送消息
        for (auto& client : clients) {
            client->send("Memory test message");
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // 断开所有连接
        for (auto& client : clients) {
            client->disconnect();
        }
        
        std::cout << "内存测试完成" << std::endl;
    }
    
    void runAllPerformanceTests() {
        std::cout << "开始WebSocket客户端性能测试..." << std::endl;
        
        runLatencyTest();
        runThroughputTest();
        runCompressionPerformanceTest();
        runMemoryTest();
        
        std::cout << "\n=== 性能测试总结 ===" << std::endl;
        std::cout << "所有性能测试完成！" << std::endl;
    }
};

int main() {
    PerformanceTest test;
    test.runAllPerformanceTests();
    return 0;
}