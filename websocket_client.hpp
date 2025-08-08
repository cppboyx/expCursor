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
#include <openssl/sha.h>

#ifdef USE_ZLIB
#include <zlib.h>
#endif

namespace websocket {

// Result codes
enum class ResultCode {
    SUCCESS = 0,
    URL_ERROR,
    CONNECTION_ERROR,
    HANDSHAKE_ERROR,
    FRAME_ERROR,
    COMPRESSION_ERROR,
    SSL_ERROR,
    TIMEOUT,
    CLOSED,
    INVALID_STATE,
    BUFFER_OVERFLOW,
    INVALID_PARAMETER
};

class WebSocketResult {
public:
    WebSocketResult(ResultCode code, const std::string& message) 
        : code_(code), message_(message) {}
    
    WebSocketResult(const WebSocketResult&) = default;
    WebSocketResult(WebSocketResult&&) = default;
    WebSocketResult& operator=(const WebSocketResult&) = default;
    WebSocketResult& operator=(WebSocketResult&&) = default;

    explicit operator bool() const noexcept { return code_ == ResultCode::SUCCESS; }
    explicit bool operator!() const noexcept { return code_ != ResultCode::SUCCESS; }

    ResultCode code() const noexcept { return code_; }
    const std::string& message() const noexcept { return message_; }

private:
    ResultCode code_;
    std::string message_;
};

enum class FrameType : uint8_t {
    CONTINUATION = 0x0,
    TEXT = 0x1,
    BINARY = 0x2,
    CLOSE = 0x8,
    PING = 0x9,
    PONG = 0xA
};

enum class WebSocketState {
    CONNECTING,
    OPEN,
    CLOSING,
    CLOSED
};

// Config
class WebSocketConfig {
public:
    WebSocketConfig() {
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
    std::string scheme() const noexcept { return scheme_; }
    std::string host() const noexcept { return host_; }
    int port() const noexcept { return port_; }
    std::string path() const noexcept { return path_; }
    std::string query() const noexcept { return query_; }

    WebSocketResult parse(const std::string& url) noexcept {
        size_t pos = 0;
        
        // 解析协议
        size_t scheme_end = url.find("://");
        if (scheme_end != std::string::npos) {
            scheme_ = url.substr(0, scheme_end);
            pos = scheme_end + 3;
        } else {
            return WebSocketResult(ResultCode::URL_ERROR,"Invalid URL: missing scheme");
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

            if (std::to_string(port_) != host_port.substr(colon_pos + 1)) {
                return WebSocketResult(ResultCode::URL_ERROR,"Invalid URL: invalid port number");
            }

            if (port_ <= 0 || port_ > 65535) {
                return WebSocketResult(ResultCode::URL_ERROR,"Invalid URL: invalid port number");
            }
        } else {
            host_ = host_port;
            port_ = (scheme_ == "wss") ? 443 : 80;
        }

        if (host_.empty()) {
            return WebSocketResult(ResultCode::URL_ERROR,"Invalid URL: missing host");
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

        return WebSocketResult(ResultCode::SUCCESS, "");
    }

private:
    std::string scheme_;
    std::string host_;
    int port_;
    std::string path_;
    std::string query_;
};

#ifndef _WIN32
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

// 网络连接类
class NetworkConnection {
public:
    NetworkConnection() : socket_(INVALID_SOCKET), ssl_ctx_(nullptr), ssl_(nullptr) {
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

    WebSocketResult connect(const std::string& host, int port, bool use_ssl, int timeout_ms) noexcept {
        // 解析主机地址
        struct addrinfo hints, *result;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        int ret = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result);
        if (ret != 0) {
            #ifdef _WIN32
            return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to resolve host: " + std::string(WSAGetLastError()));
            #else
            return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to resolve host: " + std::string(gai_strerror(ret)));
            #endif
        }

        WebSocketResult result(ResultCode::SUCCESS, "");
        for (rp = result; rp != NULL; rp = rp->ai_next) {
            result = connectInternal(rp);
            if(!result) {
                continue;
            }
        }

        freeaddrinfo(result);

        if (result && use_ssl) {
            result = setupSSL();
        }

        if(!result) {
            close();
        }

        return result;
    }

    WebSocketResult send(const std::string& data) noexcept {
        if (ssl_) {
            size_t written = 0;
            while (SSL_write_ex(ssl_, data.c_str(), data.length(), &written) == 0) {
                int error = SSL_get_error(ssl_, 0);
                if(error == SSL_ERROR_WANT_READ) {
                    fd_set fds;
                    FD_ZERO(&fds);
                    FD_SET(SSL_get_fd(ssl_), &fds);
                    select(SSL_get_fd(ssl_) + 1, &fds, NULL, NULL, NULL);
                    continue;
                } else if(error == SSL_ERROR_WANT_WRITE) {
                    fd_set fds;
                    FD_ZERO(&fds);
                    FD_SET(SSL_get_fd(ssl_), &fds);
                    select(SSL_get_fd(ssl_) + 1, NULL, &fds, NULL, NULL);
                    continue;
                } else {
                    return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to send: " + std::string(ERR_reason_error_string(ERR_get_error())));
                }
            }
        } else {
            while(::send(socket_, data.c_str(), data.length(), 0) == SOCKET_ERROR) {
                #ifdef _WIN32
                if(WSAGetLastError() == WSAEWOULDBLOCK) {
                    fd_set fds;
                    FD_ZERO(&fds);
                    FD_SET(socket_, &fds);
                    select(socket_ + 1, NULL, &fds, NULL, NULL);
                    continue;
                }
                #else
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    fd_set fds;
                    FD_ZERO(&fds);
                    FD_SET(socket_, &fds);
                    select(socket_ + 1, NULL, &fds, NULL, NULL);
                    continue;
                }
                #endif

                #ifndef _WIN32
                return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to send: " + std::string(strerror(errno)));
                #else
                return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to send: " + std::string(WSAGetLastError()));
                #endif
            }
        }

        return WebSocketResult(ResultCode::SUCCESS, "");
    }

    WebSocketResult receive(char* buffer, int size, size& readbytes, int timeout_ms) noexcept {
        timeval time_out = {0};
        time_out.tv_sec = timeout_ms / 1000;
        time_out.tv_usec = (timeout_ms % 1000) * 1000;
            
        if (ssl_) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(SSL_get_fd(ssl_), &fds);
            select(SSL_get_fd(ssl_) + 1, &fds, NULL, NULL, &time_out);
        
            readbytes = 0;
            if (SSL_read_ex(ssl_, buffer, size, &readbytes) == 0) {
                int error = SSL_get_error(ssl_, 0);
                if(error != SSL_ERROR_WANT_READ && error != SSL_ERROR_WANT_WRITE) {
                    return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to recv: " + std::string(ERR_reason_error_string(ERR_get_error())));
                }
            }

            return WebSocketResult(ResultCode::SUCCESS, "");
        } else {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(socket_, &fds);
            select(socket_ + 1, &fds, NULL, NULL, &time_out);

            int ret = ::recv(socket_, buffer, size, 0);
            if(ret == 0) {
                return WebSocketResult(ResultCode::CONNECTION_ERROR,"Connection closed by peer");
            } else if(ret == SOCKET_ERROR) {
                #ifdef _WIN32
                if(WSAGetLastError() != WSAEWOULDBLOCK) {
                    return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to recv: " + std::string(WSAGetLastError()));
                }
                #else
                if (errno != EAGAIN && errno != EWOULDBLOCK) {
                    return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to recv: " + std::string(strerror(errno)));
                }
                #endif
            }

            readbytes = ret;
            return WebSocketResult(ResultCode::SUCCESS, "");
        }
    }

    void close() noexcept {
        if (ssl_) {
            while ((int ret = SSL_shutdown(ssl)) < 0) {
                int error = SSL_get_error(ssl_, ret);
                if(error == SSL_ERROR_WANT_READ) {
                    fd_set fds;
                    FD_ZERO(&fds);
                    FD_SET(SSL_get_fd(ssl_), &fds);
                    select(SSL_get_fd(ssl_) + 1, &fds, NULL, NULL, NULL);
                    continue;
                } else if(error == SSL_ERROR_WANT_WRITE) {
                    fd_set fds;
                    FD_ZERO(&fds);
                    FD_SET(SSL_get_fd(ssl_), &fds);
                    select(SSL_get_fd(ssl_) + 1, NULL, &fds, NULL, NULL);
                    continue;
                }

                break;
            }

            SSL_free(ssl_);
            ssl_ = nullptr;
        }
        if (ssl_ctx_) {
            SSL_CTX_free(ssl_ctx_);
            ssl_ctx_ = nullptr;
        }
        if (socket_ != INVALID_SOCKET) {
            #ifdef _WIN32
            closesocket(socket_);
            #else
            ::close(socket_);
            #endif
            
            socket_ = INVALID_SOCKET;
        }
    }

    bool isConnected() const noexcept {
        return socket_ != INVALID_SOCKET;
    }

private:
    WebSocketResult connectInternal(struct addrinfo* result, bool use_ssl, int timeout_ms) noexcept {
        // 创建socket
        socket_ = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (socket_ == INVALID_SOCKET) {
            #ifdef _WIN32
            return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to create socket: " + std::string(WSAGetLastError()));
            #else
            return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to create socket: " + std::string(strerror(errno)));
            #endif
        }

        // 设置非阻塞模式
        #ifdef _WIN32
        u_long mode = 1;
        if (ioctlsocket(socket_, FIONBIO, &mode) == SOCKET_ERROR) {
            return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to set non-blocking mode: " + std::string(WSAGetLastError()));
        }
        #else
        int flags = fcntl(socket_, F_GETFL, 0);
        if (flags < 0) {
            return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to get socket flags: " + std::string(strerror(errno)));
        }

        if (fcntl(socket_, F_SETFL, flags | O_NONBLOCK) == -1) {
            return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to set non-blocking mode: " + std::string(strerror(errno)));
        }
        #endif

        // 连接
        ret = ::connect(socket_, result->ai_addr, result->ai_addrlen);
        if (ret == SOCKET_ERROR) {
             #ifdef _WIN32
            if (WSAGetLastError() != WSAEWOULDBLOCK) {
                return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to connect: " + std::string(WSAGetLastError()));
            }
            #else
            if (errno != EINPROGRESS) {
                return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to connect: " + std::string(strerror(errno)));
            }
            #endif

            fd_set setw, sete;
            FD_ZERO(&setw);
            FD_SET(socket_, &setw);
            FD_ZERO(&sete);
            FD_SET(socket_, &sete);

            timeval time_out = {0};
            time_out.tv_sec = timeout_ms / 1000;
            time_out.tv_usec = (timeout_ms % 1000) * 1000;
            
            ret = select(socket_ + 1, NULL, &setw, &sete, &time_out);
            if (ret < 0) {
                #ifdef _WIN32
                return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to connect: " + std::string(WSAGetLastError()));
                #else
                return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to connect: " + std::string(strerror(errno)));
                #endif
            } else if(ret == 0) {
                //time out
                return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to connect: timeout");
            } else {
                if (FD_ISSET(socket_, &sete)) {
                    int so_error;
                    socklen_t len = sizeof(so_error);
                    ret = getsockopt(socket_, SOL_SOCKET, SO_ERROR, &so_error, &len);
                    if(ret == SOCKET_ERROR) {
                        #ifdef _WIN32
                        return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to connect: " + std::string(WSAGetLastError()));
                        #else
                        return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to connect: " + std::string(strerror(errno)));
                        #endif
                    }
                    
                    #ifdef _WIN32
                    WSASetLastError(so_error);
                    return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to connect: " + std::string(WSAGetLastError()));
                    #else
                    errno = so_error;
                    return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to connect: " + std::string(strerror(errno)));
                    #endif
                }
            }
        }

        return WebSocketResult(ResultCode::SUCCESS, "");
    }

    WebSocketResult setupSSL() noexcept {
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();

        ssl_ctx_ = SSL_CTX_new(TLS_client_method());
        if (!ssl_ctx_) {
            return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to create SSL context: " + std::string(ERR_reason_error_string(ERR_get_error())));
        }

        ssl_ = SSL_new(ssl_ctx_);
        if (!ssl_) {
            return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to create SSL: " + std::string(ERR_reason_error_string(ERR_get_error())));
        }

        if (SSL_set_fd(ssl_, socket_) != 1) {
            return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to set SSL socket: " + std::string(ERR_reason_error_string(ERR_get_error())));
        }

        if (SSL_set_tlsext_host_name(ssl_, host.c_str()) != 1) {
            return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to set SSL host name: " + std::string(ERR_reason_error_string(ERR_get_error())));
        }

        while(SSL_connect(ssl_) <= 0) {
            ret = SSL_get_error(ssl_,ret);
            if(ret == SSL_ERROR_WANT_READ) {
                fd_set fds;
                FD_ZERO(&fds);
                FD_SET(SSL_get_fd(ssl_), &fds);
                select(SSL_get_fd(ssl_) + 1, &fds, NULL, NULL, NULL);
                continue;
            } else if(ret == SSL_ERROR_WANT_WRITE) {
                fd_set fds;
                FD_ZERO(&fds);
                FD_SET(SSL_get_fd(ssl_), &fds);
                select(SSL_get_fd(ssl_) + 1, NULL, &fds, NULL, NULL);
                continue;
            } else {
                return WebSocketResult(ResultCode::CONNECTION_ERROR,"Failed to connect SSL: " + std::string(ERR_reason_error_string(ERR_get_error())));
            }
        }

        return WebSocketResult(ResultCode::SUCCESS, "");
    }

private:
    int socket_;
    SSL_CTX* ssl_ctx_;
    SSL* ssl_;
};

#ifndef _WIN32
#undef INVALID_SOCKET
#undef SOCKET_ERROR
#endif


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

    WebSocketResult compress(const std::string& data,std::string& result) noexcept {
        if (data.empty()) {
            result = data;
            return WebSocketResult(ResultCode::SUCCESS, "");
        }

        result.clear();
        compressor_.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data.c_str()));
        compressor_.avail_in = data.length();

        do {
            char buffer[1024];
            compressor_.next_out = reinterpret_cast<Bytef*>(buffer);
            compressor_.avail_out = sizeof(buffer);

            int ret = deflate(&compressor_, Z_SYNC_FLUSH);
            if(ret == Z_STREAM_END) {
                break;
            } else if(ret != Z_OK) {
                return WebSocketResult(ResultCode::COMPRESSION_ERROR,"Failed to compress: " + std::string(zlibError(ret)));
            }

            size_t compressed_size = sizeof(buffer) - compressor_.avail_out;
            result.append(buffer, compressed_size);
        } while (compressor_.avail_out == 0);

        return WebSocketResult(ResultCode::SUCCESS, "");
    }

    WebSocketResult decompress(const std::string& data,std::string& result)  noexcept {
        if (data.empty()) {
            result = data;
            return WebSocketResult(ResultCode::SUCCESS, "");
        }

        result.clear();
        decompressor_.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(data.c_str()));
        decompressor_.avail_in = data.length();

        do {
            char buffer[1024];
            decompressor_.next_out = reinterpret_cast<Bytef*>(buffer);
            decompressor_.avail_out = sizeof(buffer);

            int ret = inflate(&decompressor_, Z_SYNC_FLUSH);
            if(ret == Z_STREAM_END) {
                break;
            } else if(ret != Z_OK) {
                return WebSocketResult(ResultCode::COMPRESSION_ERROR,"Failed to decompress: " + std::string(zlibError(ret)));
            }

            size_t decompressed_size = sizeof(buffer) - decompressor_.avail_out;
            result.append(buffer, decompressed_size);
        } while (decompressor_.avail_out == 0);

        return WebSocketResult(ResultCode::SUCCESS, "");
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

    static WebSocketResult parse(const std::string& data,WebSocketFrame& frame) noexcept {
        size_t pos = 0;

        if (data.length() < 2) {
            return WebSocketResult(ResultCode::FRAME_ERROR, "Frame too short");
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
                return WebSocketResult(ResultCode::FRAME_ERROR, "Frame too short for 16-bit length");
            }
            payload_length = (static_cast<uint8_t>(data[pos]) << 8) | static_cast<uint8_t>(data[pos + 1]);
            pos += 2;
        } else if (payload_length == 127) {
            if (data.length() < pos + 8) {
                return WebSocketResult(ResultCode::FRAME_ERROR, "Frame too short for 64-bit length");
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
                return WebSocketResult(ResultCode::FRAME_ERROR, "Frame too short for mask key");
            }
            frame.mask_key_ = data.substr(pos, 4);
            pos += 4;
        }

        // 解析载荷数据
        if (data.length() < pos + payload_length) {
            return WebSocketResult(ResultCode::FRAME_ERROR, "Frame too short for payload");
        }

        std::string payload = data.substr(pos, payload_length);
        if (frame.masked_) {
            for (size_t i = 0; i < payload.length(); ++i) {
                payload[i] = payload[i] ^ frame.mask_key_[i % 4];
            }
        }

        frame.payload_ = payload;
        frame.payload_length_ = payload.length();

        return WebSocketResult(ResultCode::SUCCESS, "");
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
    static WebSocketResult createHandshakeRequest(const URL& url, const WebSocketConfig& config, std::string& request, std::string& accept_key) noexcept {
        std::string key = Utils::generateRandomString(16);
        accept_key = Utils::base64Encode(Utils::sha1(key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));

        request.clear();
        request = "GET " + url.path();
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
        return WebSocketResult(ResultCode::SUCCESS, "");
    }

    static WebSocketResult parseHandshakeResponse(const std::string& response, const std::string& accept_key) noexcept {
        std::vector<std::string> lines = Utils::split(response, '\n');
        if (lines.empty()) {
            return WebSocketResult(ResultCode::HANDSHAKE_ERROR, "Empty response");
        }

        // 检查状态行
        std::string status_line = Utils::trim(lines[0]);
        if (status_line.find("HTTP/1.1 101") == std::string::npos) {
            return WebSocketResult(ResultCode::HANDSHAKE_ERROR, "Invalid status line : " + status_line);
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
                if (value != accept_key) {
                    return WebSocketResult(ResultCode::HANDSHAKE_ERROR, "Invalid accept key : " + value);
                }

                has_accept = true;
            }
        }

        if (!has_upgrade) {
            return WebSocketResult(ResultCode::HANDSHAKE_ERROR, "Missing upgrade header");
        }
        if (!has_connection) {
            return WebSocketResult(ResultCode::HANDSHAKE_ERROR, "Missing connection header");
        }
        if (!has_accept) {
            return WebSocketResult(ResultCode::HANDSHAKE_ERROR, "Missing accept header");
        }

        return WebSocketResult(ResultCode::SUCCESS, "");
    }
};


class TaskRunner {
public:
    TaskRunner() : run_(false) {}
    ~TaskRunner() {
        stop();
    }

    void start () noexcept {
        std::unique_lock<std::mutex> lock(mtx_);
        if (run_) {
            return;
        }

        run_ = true;
        worker_ = std::thread([this] { run(); });
    }

    void stop() noexcept {
        {
            std::unique_lock<std::mutex> lock(mtx_);
            if (!run_) {
                return;
            }

            run_ = false;
        }

        cv_.notify_all();
        worker_.join();
    }

    void push_task(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(mtx_);
            tasks_.push(std::move(task));
        }
        cv_.notify_one();
    }

    void clear() {
        {
            std::unique_lock<std::mutex> lock(mtx_);
            tasks_.clear();
        }
        cv_.notify_all();
    }

private:
    void run() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mtx_);
                cv_.wait(lock, [this] { return !run_ || !tasks_.empty(); });

                if (!run_ && tasks_.empty()) {
                    return;
                }

                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
    }

    std::thread worker_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::queue<std::function<void()>> tasks_;
    bool run_;
};


// WebSocket客户端主类
class WebSocketClient {
public:
    WebSocketClient() : state_(WebSocketState::CLOSED), config_(WebSocketConfig()) {
    }

    explicit WebSocketClient(const WebSocketConfig& config) : state_(WebSocketState::CLOSED), config_(config) {
    }

    ~WebSocketClient() {
        disconnect();
    }

    // 设置回调函数
    void setOnMsgText(std::function<void(const std::string&)> callback) { text_message_callback_ = callback; }
    void setOnMsgBinary(std::function<void(const std::vector<uint8_t>&)> callback) { binary_message_callback_ = callback; }
    void setOnError(std::function<void(const std::string& reason)> callback) { error_callback_ = callback; }
    void setOnOpen(std::function<void()> callback) { open_callback_ = callback; }
    void setOnClose(std::function<void(const std::string& reason)> callback) { close_callback_ = callback; }



    // 连接方法
    WebSocketResult connect_sync(const std::string& url) noexcept {
        WebSocketState state = WebSocketState::CLOSED;
        if (_state.compare_exchange_strong(state, WebSocketState::CONNECTING)) {
            return WebSocketResult(ResultCode::INVALID_STATE, "WebSocket is already connecting");
        }

        URL u;
        if (auto res = u.parse(url); !res) {
            _state = WebSocketState::CLOSED;
            return res;
        }

        // 连接网络
        if (auto res = connection_.connect(url.host(), url.port(), (url.scheme() == "wss")); !res) {
            _state = WebSocketState::CLOSED;
            return res;
        }

        // 执行握手
        if (auto res = performHandshake(url); !res) {
            connection_.close();

            _state = WebSocketState::CLOSED;
            return res;
        }

        // 启动工作线程
        startWorker();

        _state = WebSocketState::OPEN;
        onOpen();

        return WebSocketResult(ResultCode::SUCCESS, "");
    }

    void connect_async(const std::string& url, const std::function<void(WebSocketResult)>& callback) noexcept {
        task_runner_.start();
        task_runner_.push_task([this, url, callback] {
             callback(connect_sync(url));
        });
    }

    // 断开连接
    void disconnect() {
        if (state_ == WebSocketState::OPEN) {
            setState(WebSocketState::CLOSING);
            
            // 发送关闭帧
            sendCloseFrame();
        }

        // 停止工作线程
        stopWorker();

        setState(WebSocketState::CLOSED);
        
        // 关闭网络连接
        connection_.close();
    }

    // 发送消息
    WebSocketResult send(const std::string& message) {
        if (state_ != WebSocketState::OPEN) {
            return WebSocketResult(ResultCode::INVALID_STATE, "WebSocket is not open");
        }

        return sendFrame(FrameType::TEXT, message);
    }

    WebSocketResult sendBinary(const std::string& data) {
        if (state_ != WebSocketState::OPEN) {
            return WebSocketResult(ResultCode::INVALID_STATE, "WebSocket is not open");
        }

        return sendFrame(FrameType::BINARY, data);
    }

    // 发送ping
    WebSocketResult ping(const std::string& data = "") {
        if (state_ != WebSocketState::OPEN) {
            return WebSocketResult(ResultCode::INVALID_STATE, "WebSocket is not open");
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

    WebSocketResult performHandshake(const URL& url) noexcept {
        // 发送握手请求
        std::string accept_key;
        std::string request = WebSocketHandshake::createHandshakeRequest(url, config_, accept_key);
        if (auto res = connection_.send(request); !res) {
            return res;
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
            return WebSocketResult(ResultCode::HANDSHAKE_ERROR, "Empty handshake response");
        }

        // 解析响应
        return WebSocketHandshake::parseHandshakeResponse(response, accept_key);
    }

    void startWorker() {
        _runner.start();

        //do for recv
        //... 

        //do for ping
        //...

    }

    void stopWorker() {
        _runner.stop();
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

    void onError(const WebSocketResult& result) {
        if (error_callback_) {
            error_callback_(result);
        }
    }

    void onOpen() {
        if (open_callback_) {
            open_callback_();
        }
    }

    void onClose() {
        if (close_callback_) {
            close_callback_();
        }
    }

    void onTextMessage(const std::string& message) {
        if (text_message_callback_) {
            text_message_callback_(message);
        }
    }

    void onBinaryMessage(const std::vector<uint8_t>& message) {
        if (binary_message_callback_) {
            binary_message_callback_(message);
        }
    }



    std::atomic<WebSocketState> state_;
    WebSocketConfig config_;
    NetworkConnection connection_;
    
    #ifdef USE_ZLIB
    Compression compression_;
    #endif
    
    TaskRunner runner_;
};

} // namespace websocket

#endif // WEBSOCKET_CLIENT_HPP