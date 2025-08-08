#include "websocket_client.hpp"
#include <iostream>
#include <chrono>
#include <thread>

int main() {
    // Build WebSocket config
    websocket::WebSocketConfig config;
    config.setTimeout(5000);
    config.enableCompression(true);
    config.setCompressionLevel(6);
    config.setPingInterval(30000);
    config.addHeader("User-Agent", "WebSocket-Client/1.0");
    config.addExtension("permessage-deflate", "client_max_window_bits=15");

    // Create client
    websocket::WebSocketClient client(config);

    // Callbacks
    client.setOnText([](const std::string &message) {
        std::cout << "Received text: " << message << std::endl;
    });
    client.setOnError([](const websocket::WebSocketResult &err) {
        std::cout << "Error: (" << static_cast<int>(err.code()) << ") " << err.message() << std::endl;
    });
    client.setOnOpen([]() {
        std::cout << "State: OPEN" << std::endl;
    });
    client.setOnClose([]() {
        std::cout << "State: CLOSED" << std::endl;
    });

    // Connect to WebSocket server (public echo)
    std::cout << "Connecting to WebSocket server..." << std::endl;
    if (client.connect("wss://echo.websocket.org")) {
        std::cout << "Connected successfully!" << std::endl;

        // Send text
        std::string message = "Hello, WebSocket!";
        if (client.send(message)) {
            std::cout << "Sent message: " << message << std::endl;
        }

        // Send binary
        std::string binary_data = "Binary data test";
        if (client.sendBinary(binary_data)) {
            std::cout << "Sent binary data" << std::endl;
        }

        // Ping
        if (client.ping("ping test")) {
            std::cout << "Sent ping" << std::endl;
        }

        // Wait for responses
        std::this_thread::sleep_for(std::chrono::seconds(5));

        // Disconnect
        std::cout << "Disconnecting..." << std::endl;
        client.disconnect();
    } else {
        std::cout << "Failed to connect!" << std::endl;
    }

    return 0;
}