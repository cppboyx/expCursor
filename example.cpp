#include "websocket_client.hpp"
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    // 创建WebSocket配置
    websocket::WebSocketConfig config;
    config.setTimeout(5000);
    config.enableCompression(true);
    config.setCompressionLevel(6);
    config.setPingInterval(30000);
    config.addHeader("User-Agent", "WebSocket-Client/1.0");
    config.addExtension("permessage-deflate", "client_max_window_bits=15");

    // 创建WebSocket客户端
    websocket::WebSocketClient client(config);

    // 设置消息回调
    client.setMessageCallback([](const std::string& message) {
        std::cout << "Received message: " << message << std::endl;
    });

    // 设置错误回调
    client.setErrorCallback([](const websocket::WebSocketError& error) {
        std::cout << "Error: " << error.toString() << std::endl;
    });

    // 设置状态变化回调
    client.setStateChangeCallback([](websocket::WebSocketState state) {
        std::string state_str;
        switch (state) {
            case websocket::WebSocketState::CONNECTING:
                state_str = "CONNECTING";
                break;
            case websocket::WebSocketState::OPEN:
                state_str = "OPEN";
                break;
            case websocket::WebSocketState::CLOSING:
                state_str = "CLOSING";
                break;
            case websocket::WebSocketState::CLOSED:
                state_str = "CLOSED";
                break;
        }
        std::cout << "State changed to: " << state_str << std::endl;
    });

    // 连接到WebSocket服务器
    std::cout << "Connecting to WebSocket server..." << std::endl;
    
    // 使用一个公共的WebSocket echo服务器进行测试
    if (client.connect("wss://echo.websocket.org")) {
        std::cout << "Connected successfully!" << std::endl;

        // 发送文本消息
        std::string message = "Hello, WebSocket!";
        if (client.send(message)) {
            std::cout << "Sent message: " << message << std::endl;
        }

        // 发送二进制数据
        std::string binary_data = "Binary data test";
        if (client.sendBinary(binary_data)) {
            std::cout << "Sent binary data" << std::endl;
        }

        // 发送ping
        if (client.ping("ping test")) {
            std::cout << "Sent ping" << std::endl;
        }

        // 等待一段时间接收响应
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // 断开连接
        std::cout << "Disconnecting..." << std::endl;
        client.disconnect();
    } else {
        std::cout << "Failed to connect!" << std::endl;
    }

    return 0;
}