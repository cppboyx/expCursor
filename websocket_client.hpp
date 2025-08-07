#ifndef WEBSOCKET_CLIENT_HPP
#define WEBSOCKET_CLIENT_HPP

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cstdint>
#include <cassert>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#ifdef USE_ZLIB
#include <zlib.h>
#endif

namespace websocket {

// 错误码定义
enum class ErrorCode {
    SUCCESS = 0,
    INVALID_URL,
    CONNECTION_FAILED,
    HANDSHAKE_FAILED,
    INVALID_FRAME,
    COMPRESSION_ERROR,
    SSL_ERROR,
    TIMEOUT,
    CLOSED,
    INVALID_STATE,
    BUFFER_OVERFLOW,
    INVALID_PARAMETER
};

// 错误信息类
class WebSocketError {
public:
    WebSocketError(ErrorCode code, const std::string& message) 
        : code_(code), message_(message) {}
    
    ErrorCode code() const { return code_; }
    const std::string& message() const { return message_; }
    
    std::string toString() const {
        return "WebSocket Error [" + std::to_string(static_cast<int>(code_)) + "]: " + message_;
    }

private:
    ErrorCode code_;
    std::string message_;
};

// WebSocket帧类型
enum class FrameType {
    CONTINUATION = 0x0,
    TEXT = 0x1,
    BINARY = 0x2,
    CLOSE = 0x8,
    PING = 0x9,
    PONG = 0xA
};

// WebSocket状态
enum class WebSocketState {
    CONNECTING,
    OPEN,
    CLOSING,
    CLOSED
};

// WebSocket配置类
class WebSocketConfig {
public:
    WebSocketConfig() {
        // 默认配置
        timeout_ms_ = 5000;
        max_frame_size_ = 1024 * 1024; // 1MB
        enable_compression_ = false;
        compression_level_ = 6;
        ping_interval_ms_ = 30000; // 30秒
        pong_timeout_ms_ = 10000;  // 10秒
        max_reconnect_attempts_ = 3;
        reconnect_delay_ms_ = 1000;
    }

    // 设置超时时间
    void setTimeout(int timeout_ms) { timeout_ms_ = timeout_ms; }
    int getTimeout() const { return timeout_ms_; }

    // 设置最大帧大小
    void setMaxFrameSize(size_t size) { max_frame_size_ = size; }
    size_t getMaxFrameSize() const { return max_frame_size_; }

    // 启用/禁用压缩
    void enableCompression(bool enable) { enable_compression_ = enable; }
    bool isCompressionEnabled() const { return enable_compression_; }

    // 设置压缩级别
    void setCompressionLevel(int level) { 
        if (level >= 0 && level <= 9) compression_level_ = level; 
    }
    int getCompressionLevel() const { return compression_level_; }

    // 设置ping间隔
    void setPingInterval(int interval_ms) { ping_interval_ms_ = interval_ms; }
    int getPingInterval() const { return ping_interval_ms_; }

    // 设置pong超时
    void setPongTimeout(int timeout_ms) { pong_timeout_ms_ = timeout_ms; }
    int getPongTimeout() const { return pong_timeout_ms_; }

    // 设置重连参数
    void setMaxReconnectAttempts(int attempts) { max_reconnect_attempts_ = attempts; }
    int getMaxReconnectAttempts() const { return max_reconnect_attempts_; }

    void setReconnectDelay(int delay_ms) { reconnect_delay_ms_ = delay_ms; }
    int getReconnectDelay() const { return reconnect_delay_ms_; }

    // 设置自定义头部
    void addHeader(const std::string& key, const std::string& value) {
        headers_[key] = value;
    }
    const std::map<std::string, std::string>& getHeaders() const { return headers_; }

    // 设置扩展参数
    void addExtension(const std::string& name, const std::string& params) {
        extensions_[name] = params;
    }
    const std::map<std::string, std::string>& getExtensions() const { return extensions_; }

private:
    int timeout_ms_;
    size_t max_frame_size_;
    bool enable_compression_;
    int compression_level_;
    int ping_interval_ms_;
    int pong_timeout_ms_;
    int max_reconnect_attempts_;
    int reconnect_delay_ms_;
    std::map<std::string, std::string> headers_;
    std::map<std::string, std::string> extensions_;
};

// 工具类
class Utils {
public:
    // 生成随机字符串
    static std::string generateRandomString(size_t length) {
        static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, chars.size() - 1);
        
        std::string result;
        result.reserve(length);
        for (size_t i = 0; i < length; ++i) {
            result += chars[dis(gen)];
        }
        return result;
    }

    // Base64编码
    static std::string base64Encode(const std::string& input) {
        static const std::string chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string result;
        int val = 0, valb = -6;
        
        for (unsigned char c : input) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                result.push_back(chars[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        if (valb > -6) result.push_back(chars[((val << 8) >> (valb + 8)) & 0x3F]);
        while (result.size() % 4) result.push_back('=');
        return result;
    }

    // SHA1哈希
    static std::string sha1(const std::string& input) {
        unsigned char hash[20];
        SHA1(reinterpret_cast<const unsigned char*>(input.c_str()), input.length(), hash);
        
        std::stringstream ss;
        for (int i = 0; i < 20; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }
        return ss.str();
    }

    // 字符串分割
    static std::vector<std::string> split(const std::string& str, char delimiter) {
        std::vector<std::string> tokens;
        std::stringstream ss(str);
        std::string token;
        while (std::getline(ss, token, delimiter)) {
            tokens.push_back(token);
        }
        return tokens;
    }

    // 字符串修剪
    static std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }

    // 字符串转小写
    static std::string toLower(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }
};

// URL解析类
class URL {
public:
    URL(const std::string& url) {
        parse(url);
    }

    std::string scheme() const { return scheme_; }
    std::string host() const { return host_; }
    int port() const { return port_; }
    std::string path() const { return path_; }
    std::string query() const { return query_; }

private:
    void parse(const std::string& url) {
        size_t pos = 0;
        
        // 解析协议
        size_t scheme_end = url.find("://");
        if (scheme_end != std::string::npos) {
            scheme_ = url.substr(0, scheme_end);
            pos = scheme_end + 3;
        }

        // 解析主机和端口
        size_t host_end = url.find('/', pos);
        if (host_end == std::string::npos) {
            host_end = url.length();
        }

        std::string host_port = url.substr(pos, host_end - pos);
        size_t colon_pos = host_port.find(':');
        if (colon_pos != std::string::npos) {
            host_ = host_port.substr(0, colon_pos);
            port_ = std::stoi(host_port.substr(colon_pos + 1));
        } else {
            host_ = host_port;
            port_ = (scheme_ == "wss") ? 443 : 80;
        }

        // 解析路径和查询
        if (host_end < url.length()) {
            std::string path_query = url.substr(host_end);
            size_t query_pos = path_query.find('?');
            if (query_pos != std::string::npos) {
                path_ = path_query.substr(0, query_pos);
                query_ = path_query.substr(query_pos + 1);
            } else {
                path_ = path_query;
            }
        }

        if (path_.empty()) path_ = "/";
    }

    std::string scheme_;
    std::string host_;
    int port_;
    std::string path_;
    std::string query_;
};

// 网络连接类
class NetworkConnection {
public:
    NetworkConnection() : socket_(-1), ssl_ctx_(nullptr), ssl_(nullptr) {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    }

    ~NetworkConnection() {
        close();
#ifdef _WIN32
        WSACleanup();
#endif
    }

    bool connect(const std::string& host, int port, bool use_ssl) {
        // 解析主机地址
        struct addrinfo hints, *result;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result) != 0) {
            return false;
        }

        // 创建socket
        socket_ = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (socket_ == -1) {
            freeaddrinfo(result);
            return false;
        }

        // 连接
        if (::connect(socket_, result->ai_addr, result->ai_addrlen) != 0) {
            close();
            freeaddrinfo(result);
            return false;
        }

        freeaddrinfo(result);

        // 设置非阻塞模式
        setNonBlocking(true);

        if (use_ssl) {
            return setupSSL(host);
        }

        return true;
    }

    bool send(const std::string& data) {
        if (ssl_) {
            return SSL_write(ssl_, data.c_str(), data.length()) > 0;
        } else {
            return ::send(socket_, data.c_str(), data.length(), 0) > 0;
        }
    }

    int receive(char* buffer, int size) {
        if (ssl_) {
            return SSL_read(ssl_, buffer, size);
        } else {
            return ::recv(socket_, buffer, size, 0);
        }
    }

    void close() {
        if (ssl_) {
            SSL_shutdown(ssl_);
            SSL_free(ssl_);
            ssl_ = nullptr;
        }
        if (ssl_ctx_) {
            SSL_CTX_free(ssl_ctx_);
            ssl_ctx_ = nullptr;
        }
        if (socket_ != -1) {
#ifdef _WIN32
            closesocket(socket_);
#else
            ::close(socket_);
#endif
            socket_ = -1;
        }
    }

    bool isConnected() const {
        return socket_ != -1;
    }

private:
    bool setupSSL(const std::string& host) {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();

        ssl_ctx_ = SSL_CTX_new(TLS_client_method());
        if (!ssl_ctx_) {
            return false;
        }

        ssl_ = SSL_new(ssl_ctx_);
        if (!ssl_) {
            return false;
        }

        SSL_set_fd(ssl_, socket_);
        SSL_set_tlsext_host_name(ssl_, host.c_str());

        if (SSL_connect(ssl_) != 1) {
            return false;
        }

        return true;
    }

    void setNonBlocking(bool nonblocking) {
#ifdef _WIN32
        u_long mode = nonblocking ? 1 : 0;
        ioctlsocket(socket_, FIONBIO, &mode);
#else
        int flags = fcntl(socket_, F_GETFL, 0);
        if (nonblocking) {
            fcntl(socket_, F_SETFL, flags | O_NONBLOCK);
        } else {
            fcntl(socket_, F_SETFL, flags & ~O_NONBLOCK);
        }
#endif
    }

    int socket_;
    SSL_CTX* ssl_ctx_;
    SSL* ssl_;
};

#ifdef USE_ZLIB
// 压缩/解压类
class Compression {
public:
    Compression(int level = 6) : level_(level) {
        initCompressor();
        initDecompressor();
    }

    ~Compression() {
        deflateEnd(&compressor_);
        inflateEnd(&decompressor_);
    }

    std::string compress(const std::string& data) {
        if (data.empty()) return data;

        std::string result;
        compressor_.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data.c_str()));
        compressor_.avail_in = data.length();

        do {
            char buffer[1024];
            compressor_.next_out = reinterpret_cast<Bytef*>(buffer);
            compressor_.avail_out = sizeof(buffer);

            int ret = deflate(&compressor_, Z_SYNC_FLUSH);
            if (ret == Z_STREAM_ERROR) {
                return "";
            }

            size_t compressed_size = sizeof(buffer) - compressor_.avail_out;
            result.append(buffer, compressed_size);
        } while (compressor_.avail_out == 0);

        return result;
    }

    std::string decompress(const std::string& data) {
        if (data.empty()) return data;

        std::string result;
        decompressor_.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data.c_str()));
        decompressor_.avail_in = data.length();

        do {
            char buffer[1024];
            decompressor_.next_out = reinterpret_cast<Bytef*>(buffer);
            decompressor_.avail_out = sizeof(buffer);

            int ret = inflate(&decompressor_, Z_SYNC_FLUSH);
            if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR) {
                return "";
            }

            size_t decompressed_size = sizeof(buffer) - decompressor_.avail_out;
            result.append(buffer, decompressed_size);
        } while (decompressor_.avail_out == 0);

        return result;
    }

private:
    void initCompressor() {
        compressor_.zalloc = Z_NULL;
        compressor_.zfree = Z_NULL;
        compressor_.opaque = Z_NULL;
        deflateInit2(&compressor_, level_, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
    }

    void initDecompressor() {
        decompressor_.zalloc = Z_NULL;
        decompressor_.zfree = Z_NULL;
        decompressor_.opaque = Z_NULL;
        inflateInit2(&decompressor_, -15);
    }

    int level_;
    z_stream compressor_;
    z_stream decompressor_;
};
#endif

// WebSocket帧类
class WebSocketFrame {
public:
    WebSocketFrame() : fin_(true), opcode_(0), masked_(false), payload_length_(0) {}

    void setFin(bool fin) { fin_ = fin; }
    void setOpcode(uint8_t opcode) { opcode_ = opcode; }
    void setMasked(bool masked) { masked_ = masked; }
    void setPayload(const std::string& payload) { payload_ = payload; payload_length_ = payload.length(); }
    void setMaskKey(const std::string& key) { mask_key_ = key; }

    bool isFin() const { return fin_; }
    uint8_t getOpcode() const { return opcode_; }
    bool isMasked() const { return masked_; }
    const std::string& getPayload() const { return payload_; }
    size_t getPayloadLength() const { return payload_length_; }

    std::string serialize() const {
        std::string frame;
        
        // 第一个字节
        uint8_t first_byte = (fin_ ? 0x80 : 0x00) | (opcode_ & 0x0F);
        frame.push_back(first_byte);

        // 第二个字节
        uint8_t second_byte = masked_ ? 0x80 : 0x00;
        if (payload_length_ < 126) {
            second_byte |= payload_length_;
            frame.push_back(second_byte);
        } else if (payload_length_ < 65536) {
            second_byte |= 126;
            frame.push_back(second_byte);
            frame.push_back((payload_length_ >> 8) & 0xFF);
            frame.push_back(payload_length_ & 0xFF);
        } else {
            second_byte |= 127;
            frame.push_back(second_byte);
            for (int i = 7; i >= 0; --i) {
                frame.push_back((payload_length_ >> (i * 8)) & 0xFF);
            }
        }

        // 掩码密钥
        if (masked_) {
            frame.append(mask_key_);
        }

        // 载荷数据
        if (!payload_.empty()) {
            if (masked_) {
                std::string masked_payload = payload_;
                for (size_t i = 0; i < payload_.length(); ++i) {
                    masked_payload[i] = payload_[i] ^ mask_key_[i % 4];
                }
                frame.append(masked_payload);
            } else {
                frame.append(payload_);
            }
        }

        return frame;
    }

    static WebSocketFrame parse(const std::string& data) {
        WebSocketFrame frame;
        size_t pos = 0;

        if (data.length() < 2) {
            throw WebSocketError(ErrorCode::INVALID_FRAME, "Frame too short");
        }

        // 解析第一个字节
        uint8_t first_byte = data[pos++];
        frame.fin_ = (first_byte & 0x80) != 0;
        frame.opcode_ = first_byte & 0x0F;

        // 解析第二个字节
        uint8_t second_byte = data[pos++];
        frame.masked_ = (second_byte & 0x80) != 0;
        uint64_t payload_length = second_byte & 0x7F;

        if (payload_length == 126) {
            if (data.length() < pos + 2) {
                throw WebSocketError(ErrorCode::INVALID_FRAME, "Frame too short for 16-bit length");
            }
            payload_length = (static_cast<uint8_t>(data[pos]) << 8) | static_cast<uint8_t>(data[pos + 1]);
            pos += 2;
        } else if (payload_length == 127) {
            if (data.length() < pos + 8) {
                throw WebSocketError(ErrorCode::INVALID_FRAME, "Frame too short for 64-bit length");
            }
            payload_length = 0;
            for (int i = 0; i < 8; ++i) {
                payload_length = (payload_length << 8) | static_cast<uint8_t>(data[pos + i]);
            }
            pos += 8;
        }

        // 解析掩码密钥
        if (frame.masked_) {
            if (data.length() < pos + 4) {
                throw WebSocketError(ErrorCode::INVALID_FRAME, "Frame too short for mask key");
            }
            frame.mask_key_ = data.substr(pos, 4);
            pos += 4;
        }

        // 解析载荷数据
        if (data.length() < pos + payload_length) {
            throw WebSocketError(ErrorCode::INVALID_FRAME, "Frame too short for payload");
        }

        std::string payload = data.substr(pos, payload_length);
        if (frame.masked_) {
            for (size_t i = 0; i < payload.length(); ++i) {
                payload[i] = payload[i] ^ frame.mask_key_[i % 4];
            }
        }

        frame.payload_ = payload;
        frame.payload_length_ = payload.length();

        return frame;
    }

private:
    bool fin_;
    uint8_t opcode_;
    bool masked_;
    std::string mask_key_;
    std::string payload_;
    size_t payload_length_;
};

// WebSocket握手类
class WebSocketHandshake {
public:
    static std::string createHandshakeRequest(const URL& url, const WebSocketConfig& config) {
        std::string key = Utils::generateRandomString(16);
        std::string accept_key = Utils::base64Encode(Utils::sha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));

        std::string request = "GET " + url.path();
        if (!url.query().empty()) {
            request += "?" + url.query();
        }
        request += " HTTP/1.1\r\n";
        request += "Host: " + url.host();
        if (url.port() != (url.scheme() == "wss" ? 443 : 80)) {
            request += ":" + std::to_string(url.port());
        }
        request += "\r\n";
        request += "Upgrade: websocket\r\n";
        request += "Connection: Upgrade\r\n";
        request += "Sec-WebSocket-Key: " + Utils::base64Encode(key) + "\r\n";
        request += "Sec-WebSocket-Version: 13\r\n";

        // 添加自定义头部
        for (const auto& header : config.getHeaders()) {
            request += header.first + ": " + header.second + "\r\n";
        }

        // 添加扩展
        if (!config.getExtensions().empty()) {
            std::string extensions;
            for (const auto& ext : config.getExtensions()) {
                if (!extensions.empty()) extensions += ", ";
                extensions += ext.first;
                if (!ext.second.empty()) {
                    extensions += "; " + ext.second;
                }
            }
            request += "Sec-WebSocket-Extensions: " + extensions + "\r\n";
        }

        request += "\r\n";
        return request;
    }

    static bool parseHandshakeResponse(const std::string& response) {
        std::vector<std::string> lines = Utils::split(response, '\n');
        if (lines.empty()) return false;

        // 检查状态行
        std::string status_line = Utils::trim(lines[0]);
        if (status_line.find("HTTP/1.1 101") == std::string::npos) {
            return false;
        }

        // 检查必需的头部
        bool has_upgrade = false, has_connection = false, has_accept = false;
        for (size_t i = 1; i < lines.size(); ++i) {
            std::string line = Utils::trim(lines[i]);
            if (line.empty()) break;

            size_t colon_pos = line.find(':');
            if (colon_pos == std::string::npos) continue;

            std::string key = Utils::toLower(Utils::trim(line.substr(0, colon_pos)));
            std::string value = Utils::trim(line.substr(colon_pos + 1));

            if (key == "upgrade" && Utils::toLower(value) == "websocket") {
                has_upgrade = true;
            } else if (key == "connection" && Utils::toLower(value).find("upgrade") != std::string::npos) {
                has_connection = true;
            } else if (key == "sec-websocket-accept") {
                has_accept = true;
            }
        }

        return has_upgrade && has_connection && has_accept;
    }
};

// 消息回调类型定义
using MessageCallback = std::function<void(const std::string&)>;
using ErrorCallback = std::function<void(const WebSocketError&)>;
using StateChangeCallback = std::function<void(WebSocketState)>;

// WebSocket客户端主类
class WebSocketClient {
public:
    WebSocketClient() : state_(WebSocketState::CLOSED), config_(WebSocketConfig()) {
        initCallbacks();
    }

    explicit WebSocketClient(const WebSocketConfig& config) : state_(WebSocketState::CLOSED), config_(config) {
        initCallbacks();
    }

    ~WebSocketClient() {
        disconnect();
    }

    // 设置回调函数
    void setMessageCallback(MessageCallback callback) { message_callback_ = callback; }
    void setErrorCallback(ErrorCallback callback) { error_callback_ = callback; }
    void setStateChangeCallback(StateChangeCallback callback) { state_change_callback_ = callback; }

    // 连接方法
    bool connect(const std::string& url) {
        try {
            URL parsed_url(url);
            return connectInternal(parsed_url);
        } catch (const std::exception& e) {
            handleError(ErrorCode::INVALID_URL, "Invalid URL: " + std::string(e.what()));
            return false;
        }
    }

    // 断开连接
    void disconnect() {
        if (state_ == WebSocketState::CLOSED) return;

        setState(WebSocketState::CLOSING);
        
        // 发送关闭帧
        sendCloseFrame();
        
        // 停止工作线程
        stopWorker();
        
        // 关闭网络连接
        connection_.close();
        
        setState(WebSocketState::CLOSED);
    }

    // 发送消息
    bool send(const std::string& message) {
        if (state_ != WebSocketState::OPEN) {
            handleError(ErrorCode::INVALID_STATE, "WebSocket is not open");
            return false;
        }

        return sendFrame(FrameType::TEXT, message);
    }

    bool sendBinary(const std::string& data) {
        if (state_ != WebSocketState::OPEN) {
            handleError(ErrorCode::INVALID_STATE, "WebSocket is not open");
            return false;
        }

        return sendFrame(FrameType::BINARY, data);
    }

    // 发送ping
    bool ping(const std::string& data = "") {
        if (state_ != WebSocketState::OPEN) {
            handleError(ErrorCode::INVALID_STATE, "WebSocket is not open");
            return false;
        }

        return sendFrame(FrameType::PING, data);
    }

    // 获取状态
    WebSocketState getState() const { return state_; }
    const WebSocketConfig& getConfig() const { return config_; }

    // 更新配置
    void updateConfig(const WebSocketConfig& config) {
        config_ = config;
    }

private:
    void initCallbacks() {
        message_callback_ = [](const std::string&) {};
        error_callback_ = [](const WebSocketError&) {};
        state_change_callback_ = [](WebSocketState) {};
    }

    bool connectInternal(const URL& url) {
        setState(WebSocketState::CONNECTING);

        // 连接网络
        bool use_ssl = (url.scheme() == "wss");
        if (!connection_.connect(url.host(), url.port(), use_ssl)) {
            handleError(ErrorCode::CONNECTION_FAILED, "Failed to connect to " + url.host() + ":" + std::to_string(url.port()));
            return false;
        }

        // 执行握手
        if (!performHandshake(url)) {
            connection_.close();
            handleError(ErrorCode::HANDSHAKE_FAILED, "WebSocket handshake failed");
            return false;
        }

        setState(WebSocketState::OPEN);

        // 启动工作线程
        startWorker();

        return true;
    }

    bool performHandshake(const URL& url) {
        // 发送握手请求
        std::string request = WebSocketHandshake::createHandshakeRequest(url, config_);
        if (!connection_.send(request)) {
            return false;
        }

        // 接收握手响应
        std::string response;
        char buffer[1024];
        int bytes_received;
        
        while ((bytes_received = connection_.receive(buffer, sizeof(buffer))) > 0) {
            response.append(buffer, bytes_received);
            if (response.find("\r\n\r\n") != std::string::npos) {
                break;
            }
        }

        if (response.empty()) {
            return false;
        }

        // 解析响应
        return WebSocketHandshake::parseHandshakeResponse(response);
    }

    void startWorker() {
        worker_thread_ = std::thread([this]() {
            while (state_ == WebSocketState::OPEN) {
                try {
                    if (!receiveFrame()) {
                        break;
                    }
                } catch (const WebSocketError& e) {
                    handleError(e.code(), e.message());
                    break;
                }
            }
        });

        // 启动ping线程
        if (config_.getPingInterval() > 0) {
            ping_thread_ = std::thread([this]() {
                while (state_ == WebSocketState::OPEN) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(config_.getPingInterval()));
                    if (state_ == WebSocketState::OPEN) {
                        ping();
                    }
                }
            });
        }
    }

    void stopWorker() {
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
        if (ping_thread_.joinable()) {
            ping_thread_.join();
        }
    }

    bool receiveFrame() {
        char buffer[1024];
        std::string frame_data;
        
        // 接收帧头（至少2字节）
        int bytes_received = connection_.receive(buffer, 2);
        if (bytes_received <= 0) return false;
        
        frame_data.append(buffer, bytes_received);
        
        // 解析帧头以获取载荷长度
        if (frame_data.length() < 2) return false;
        
        uint8_t second_byte = static_cast<uint8_t>(frame_data[1]);
        uint64_t payload_length = second_byte & 0x7F;
        size_t header_length = 2;
        
        if (payload_length == 126) {
            header_length = 4;
        } else if (payload_length == 127) {
            header_length = 10;
        }
        
        // 接收完整的帧头
        while (frame_data.length() < header_length) {
            bytes_received = connection_.receive(buffer, header_length - frame_data.length());
            if (bytes_received <= 0) return false;
            frame_data.append(buffer, bytes_received);
        }
        
        // 解析掩码密钥
        bool masked = (second_byte & 0x80) != 0;
        if (masked) {
            header_length += 4;
            while (frame_data.length() < header_length) {
                bytes_received = connection_.receive(buffer, header_length - frame_data.length());
                if (bytes_received <= 0) return false;
                frame_data.append(buffer, bytes_received);
            }
        }
        
        // 接收载荷数据
        while (frame_data.length() < header_length + payload_length) {
            size_t remaining = header_length + payload_length - frame_data.length();
            bytes_received = connection_.receive(buffer, std::min(remaining, sizeof(buffer)));
            if (bytes_received <= 0) return false;
            frame_data.append(buffer, bytes_received);
        }
        
        // 解析帧
        WebSocketFrame frame = WebSocketFrame::parse(frame_data);
        handleFrame(frame);
        
        return true;
    }

    void handleFrame(const WebSocketFrame& frame) {
        switch (static_cast<FrameType>(frame.getOpcode())) {
            case FrameType::TEXT:
            case FrameType::BINARY: {
                std::string payload = frame.getPayload();
                
#ifdef USE_ZLIB
                if (config_.isCompressionEnabled() && !payload.empty()) {
                    payload = compression_.decompress(payload);
                }
#endif
                
                message_callback_(payload);
                break;
            }
            case FrameType::CLOSE: {
                setState(WebSocketState::CLOSING);
                sendCloseFrame();
                setState(WebSocketState::CLOSED);
                break;
            }
            case FrameType::PING: {
                sendFrame(FrameType::PONG, frame.getPayload());
                break;
            }
            case FrameType::PONG: {
                // 处理pong响应
                break;
            }
            default:
                break;
        }
    }

    bool sendFrame(FrameType type, const std::string& payload) {
        std::string data_to_send = payload;
        
#ifdef USE_ZLIB
        if (config_.isCompressionEnabled() && !payload.empty() && 
            (type == FrameType::TEXT || type == FrameType::BINARY)) {
            data_to_send = compression_.compress(payload);
        }
#endif
        
        WebSocketFrame frame;
        frame.setFin(true);
        frame.setOpcode(static_cast<uint8_t>(type));
        frame.setMasked(true);
        frame.setPayload(data_to_send);
        frame.setMaskKey(Utils::generateRandomString(4));
        
        std::string frame_data = frame.serialize();
        return connection_.send(frame_data);
    }

    void sendCloseFrame() {
        WebSocketFrame frame;
        frame.setFin(true);
        frame.setOpcode(static_cast<uint8_t>(FrameType::CLOSE));
        frame.setMasked(true);
        frame.setPayload("");
        frame.setMaskKey(Utils::generateRandomString(4));
        
        std::string frame_data = frame.serialize();
        connection_.send(frame_data);
    }

    void setState(WebSocketState state) {
        if (state_ != state) {
            state_ = state;
            state_change_callback_(state);
        }
    }

    void handleError(ErrorCode code, const std::string& message) {
        WebSocketError error(code, message);
        error_callback_(error);
    }

    WebSocketState state_;
    WebSocketConfig config_;
    NetworkConnection connection_;
    
#ifdef USE_ZLIB
    Compression compression_;
#endif
    
    std::thread worker_thread_;
    std::thread ping_thread_;
    
    MessageCallback message_callback_;
    ErrorCallback error_callback_;
    StateChangeCallback state_change_callback_;
};

} // namespace websocket

#endif // WEBSOCKET_CLIENT_HPP