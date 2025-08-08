#ifndef WEBSOCKET_CLIENT_HPP
#define WEBSOCKET_CLIENT_HPP

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>

#ifdef USE_ZLIB
#include <zlib.h>
#endif

namespace websocket {

// ------------------------ Result Types ------------------------

enum class ResultCode {
    SUCCESS = 0,
    URL_ERROR,
    CONNECTION_ERROR,
    HANDSHAKE_ERROR,
    FRAME_ERROR,
    SSL_ERROR,
    TIMEOUT,
    CLOSED,
    INVALID_STATE,
    INVALID_PARAMETER
};

class WebSocketResult {
public:
    WebSocketResult(ResultCode code = ResultCode::SUCCESS, const std::string &message = "")
        : code_(code), message_(message) {}

    explicit operator bool() const noexcept { return code_ == ResultCode::SUCCESS; }
    ResultCode code() const noexcept { return code_; }
    const std::string &message() const noexcept { return message_; }

private:
    ResultCode code_;
    std::string message_;
};

// ------------------------ Config ------------------------

enum class WebSocketState { CONNECTING, OPEN, CLOSING, CLOSED };

class WebSocketConfig {
public:
    WebSocketConfig()
        : timeout_ms_(5000),
          max_frame_size_(1024 * 1024),
          enable_compression_(false),
          compression_level_(6),
          ping_interval_ms_(30000),
          pong_timeout_ms_(10000) {}

    void setTimeout(int ms) { timeout_ms_ = ms; }
    int getTimeout() const { return timeout_ms_; }

    void setMaxFrameSize(size_t s) { max_frame_size_ = s; }
    size_t getMaxFrameSize() const { return max_frame_size_; }

    void enableCompression(bool en) { enable_compression_ = en; }
    bool isCompressionEnabled() const { return enable_compression_; }

    void setCompressionLevel(int lvl) { if (lvl >= 0 && lvl <= 9) compression_level_ = lvl; }
    int getCompressionLevel() const { return compression_level_; }

    void setPingInterval(int ms) { ping_interval_ms_ = ms; }
    int getPingInterval() const { return ping_interval_ms_; }

    void setPongTimeout(int ms) { pong_timeout_ms_ = ms; }
    int getPongTimeout() const { return pong_timeout_ms_; }

    void addHeader(const std::string &k, const std::string &v) { headers_[k] = v; }
    const std::map<std::string, std::string> &getHeaders() const { return headers_; }

    void addExtension(const std::string &name, const std::string &params) { extensions_[name] = params; }
    const std::map<std::string, std::string> &getExtensions() const { return extensions_; }

private:
    int timeout_ms_;
    size_t max_frame_size_;
    bool enable_compression_;
    int compression_level_;
    int ping_interval_ms_;
    int pong_timeout_ms_;
    std::map<std::string, std::string> headers_;
    std::map<std::string, std::string> extensions_;
};

// ------------------------ Utils ------------------------

class Utils {
public:
    static std::string randomBytes(size_t length) {
        std::string out(length, '\0');
        RAND_bytes(reinterpret_cast<unsigned char *>(&out[0]), static_cast<int>(length));
        return out;
    }

    static std::string base64Encode(const std::string &in) {
        static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        int val = 0, valb = -6;
        for (unsigned char c : in) {
            val = (val << 8) + c;
            valb += 8;
            while (valb >= 0) {
                out.push_back(table[(val >> valb) & 0x3F]);
                valb -= 6;
            }
        }
        if (valb > -6) out.push_back(table[((val << 8) >> (valb + 8)) & 0x3F]);
        while (out.size() % 4) out.push_back('=');
        return out;
    }

    static std::string trim(const std::string &s) {
        size_t b = s.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) return "";
        size_t e = s.find_last_not_of(" \t\r\n");
        return s.substr(b, e - b + 1);
        }

    static std::vector<std::string> split(const std::string &s, char delim) {
        std::vector<std::string> parts;
        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, delim)) parts.push_back(item);
        return parts;
    }

    static std::string toLower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return s;
    }
};

// ------------------------ URL ------------------------

class URL {
public:
    const std::string &scheme() const { return scheme_; }
    const std::string &host() const { return host_; }
    int port() const { return port_; }
    const std::string &path() const { return path_; }
    const std::string &query() const { return query_; }

    WebSocketResult parse(const std::string &url) {
        size_t pos = url.find("://");
        if (pos == std::string::npos) return {ResultCode::URL_ERROR, "invalid url: missing scheme"};
        scheme_ = url.substr(0, pos);
        std::string rest = url.substr(pos + 3);

        size_t slash = rest.find('/');
        std::string hostport = slash == std::string::npos ? rest : rest.substr(0, slash);
        if (slash == std::string::npos) {
            path_ = "/";
        } else {
            path_ = rest.substr(slash);
        }

        size_t colon = hostport.find(':');
        if (colon == std::string::npos) {
            host_ = hostport;
            port_ = (scheme_ == "wss") ? 443 : 80;
        } else {
            host_ = hostport.substr(0, colon);
            std::string p = hostport.substr(colon + 1);
            if (p.empty() || p.find_first_not_of("0123456789") != std::string::npos)
                return {ResultCode::URL_ERROR, "invalid url: bad port"};
            port_ = std::atoi(p.c_str());
            if (port_ <= 0 || port_ > 65535) return {ResultCode::URL_ERROR, "invalid url: bad port"};
        }

        if (host_.empty()) return {ResultCode::URL_ERROR, "invalid url: missing host"};
        if (scheme_ != "ws" && scheme_ != "wss") return {ResultCode::URL_ERROR, "invalid url: scheme must be ws/wss"};
        return {ResultCode::SUCCESS, ""};
    }

private:
    std::string scheme_;
    std::string host_;
    int port_ = 0;
    std::string path_ = "/";
    std::string query_;
};

#ifndef _WIN32
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

// ------------------------ NetworkConnection ------------------------

class NetworkConnection {
public:
    NetworkConnection() : socket_(INVALID_SOCKET), ssl_ctx_(nullptr), ssl_(nullptr) {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    }

    ~NetworkConnection() { close();
#ifdef _WIN32
        WSACleanup();
#endif
    }

    WebSocketResult connect(const std::string &host, int port, bool use_ssl, int timeout_ms) {
        host_ = host;
        struct addrinfo hints; std::memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        struct addrinfo *res = nullptr;
        int gr = getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res);
        if (gr != 0) {
#ifndef _WIN32
            return {ResultCode::CONNECTION_ERROR, std::string("getaddrinfo: ") + gai_strerror(gr)};
#else
            return {ResultCode::CONNECTION_ERROR, "getaddrinfo failed"};
#endif
        }

        WebSocketResult last_error(ResultCode::CONNECTION_ERROR, "connect failed");
        for (struct addrinfo *rp = res; rp != nullptr; rp = rp->ai_next) {
            socket_ = ::socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (socket_ == INVALID_SOCKET) continue;

            // set non-blocking temporarily for connect timeout
#ifndef _WIN32
            int flags = fcntl(socket_, F_GETFL, 0);
            fcntl(socket_, F_SETFL, flags | O_NONBLOCK);
#else
            u_long mode = 1; ioctlsocket(socket_, FIONBIO, &mode);
#endif
            int c = ::connect(socket_, rp->ai_addr, rp->ai_addrlen);
#ifndef _WIN32
            if (c != 0 && errno != EINPROGRESS) { ::close(socket_); socket_ = INVALID_SOCKET; continue; }
#else
            if (c != 0 && WSAGetLastError() != WSAEWOULDBLOCK) { closesocket(socket_); socket_ = INVALID_SOCKET; continue; }
#endif
            fd_set wfds; FD_ZERO(&wfds); FD_SET(socket_, &wfds);
            fd_set efds; FD_ZERO(&efds); FD_SET(socket_, &efds);
            timeval tv; tv.tv_sec = timeout_ms / 1000; tv.tv_usec = (timeout_ms % 1000) * 1000;
            int sel = select(socket_ + 1, nullptr, &wfds, &efds, &tv);
            if (sel <= 0 || FD_ISSET(socket_, &efds)) {
#ifndef _WIN32
                ::close(socket_);
#else
                closesocket(socket_);
#endif
                socket_ = INVALID_SOCKET; continue;
            }
            // restore blocking
#ifndef _WIN32
            fcntl(socket_, F_SETFL, flags);
#else
            mode = 0; ioctlsocket(socket_, FIONBIO, &mode);
#endif
            last_error = {ResultCode::SUCCESS, ""};
            break;
        }
        freeaddrinfo(res);
        if (!last_error) return last_error;

        if (use_ssl) {
            auto r = setupSSL();
            if (!r) { close(); return r; }
        }
        return {ResultCode::SUCCESS, ""};
    }

    WebSocketResult sendAll(const void *data, size_t len) {
        const uint8_t *p = static_cast<const uint8_t *>(data);
        size_t sent = 0;
        while (sent < len) {
            int n = 0;
            if (ssl_) {
                n = SSL_write(ssl_, p + sent, static_cast<int>(len - sent));
                if (n <= 0) {
                    int err = SSL_get_error(ssl_, n);
                    if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) continue;
                    return {ResultCode::CONNECTION_ERROR, "SSL_write failed"};
                }
            } else {
#ifndef _WIN32
                n = ::send(socket_, p + sent, len - sent, 0);
#else
                n = ::send(socket_, reinterpret_cast<const char *>(p + sent), static_cast<int>(len - sent), 0);
#endif
                if (n <= 0) return {ResultCode::CONNECTION_ERROR, "send failed"};
            }
            sent += static_cast<size_t>(n);
        }
        return {ResultCode::SUCCESS, ""};
    }

    // recv some bytes with timeout; returns number of bytes read (0 on timeout), negative on error
    int recvSome(void *buf, size_t len, int timeout_ms) {
        fd_set rfds; FD_ZERO(&rfds);
        int fd = (ssl_ ? SSL_get_fd(ssl_) : socket_);
        if (fd < 0) return -1;
        FD_SET(fd, &rfds);
        timeval tv; tv.tv_sec = timeout_ms / 1000; tv.tv_usec = (timeout_ms % 1000) * 1000;
        int sel = select(fd + 1, &rfds, nullptr, nullptr, (timeout_ms >= 0 ? &tv : nullptr));
        if (sel == 0) return 0;         // timeout
        if (sel < 0) return -1;         // error
        int n = 0;
        if (ssl_) {
            n = SSL_read(ssl_, buf, static_cast<int>(len));
            if (n <= 0) {
                int err = SSL_get_error(ssl_, n);
                if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) return 0;
                return -1;
            }
        } else {
#ifndef _WIN32
            n = ::recv(socket_, buf, len, 0);
#else
            n = ::recv(socket_, reinterpret_cast<char *>(buf), static_cast<int>(len), 0);
#endif
            if (n == 0) return -1; // peer closed
            if (n < 0) return -1;
        }
        return n;
    }

    void close() {
        if (ssl_) {
            SSL_shutdown(ssl_);
            SSL_free(ssl_); ssl_ = nullptr;
        }
        if (ssl_ctx_) { SSL_CTX_free(ssl_ctx_); ssl_ctx_ = nullptr; }
        if (socket_ != INVALID_SOCKET) {
#ifndef _WIN32
            ::close(socket_);
#else
            closesocket(socket_);
#endif
            socket_ = INVALID_SOCKET;
        }
    }

    bool isOpen() const { return socket_ != INVALID_SOCKET; }

private:
    WebSocketResult setupSSL() {
        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();
        ssl_ctx_ = SSL_CTX_new(TLS_client_method());
        if (!ssl_ctx_) return {ResultCode::SSL_ERROR, "SSL_CTX_new failed"};
        ssl_ = SSL_new(ssl_ctx_);
        if (!ssl_) return {ResultCode::SSL_ERROR, "SSL_new failed"};
        SSL_set_tlsext_host_name(ssl_, host_.c_str());
        SSL_set_fd(ssl_, socket_);
        int r = SSL_connect(ssl_);
        if (r != 1) return {ResultCode::SSL_ERROR, "SSL_connect failed"};
        return {ResultCode::SUCCESS, ""};
    }

    int socket_;
    std::string host_;
    SSL_CTX *ssl_ctx_;
    SSL *ssl_;
};

#ifndef _WIN32
#undef INVALID_SOCKET
#undef SOCKET_ERROR
#endif

// ------------------------ Frames ------------------------

enum class FrameType : uint8_t { CONTINUATION = 0x0, TEXT = 0x1, BINARY = 0x2, CLOSE = 0x8, PING = 0x9, PONG = 0xA };

class WebSocketFrame {
public:
    bool fin = true;
    uint8_t opcode = 0;
    bool masked = false;
    std::string mask_key; // 4 bytes when masked
    std::string payload;

    std::string serialize() const {
        std::string out;
        uint8_t b0 = (fin ? 0x80 : 0x00) | (opcode & 0x0F);
        out.push_back(static_cast<char>(b0));
        uint64_t len = payload.size();
        uint8_t b1 = masked ? 0x80 : 0x00;
        if (len < 126) {
            out.push_back(static_cast<char>(b1 | static_cast<uint8_t>(len)));
        } else if (len <= 0xFFFF) {
            out.push_back(static_cast<char>(b1 | 126));
            out.push_back(static_cast<char>((len >> 8) & 0xFF));
            out.push_back(static_cast<char>(len & 0xFF));
        } else {
            out.push_back(static_cast<char>(b1 | 127));
            for (int i = 7; i >= 0; --i) out.push_back(static_cast<char>((len >> (i * 8)) & 0xFF));
        }
        if (masked) out.append(mask_key);
        if (masked) {
            std::string mp = payload;
            for (size_t i = 0; i < mp.size(); ++i) mp[i] = mp[i] ^ mask_key[i % 4];
            out.append(mp);
        } else {
            out.append(payload);
        }
        return out;
    }

    static WebSocketResult parse(const std::string &in, size_t &consumed, WebSocketFrame &frame) {
        consumed = 0;
        if (in.size() < 2) return {ResultCode::FRAME_ERROR, "incomplete header"};
        size_t i = 0;
        uint8_t b0 = static_cast<uint8_t>(in[i++]);
        uint8_t b1 = static_cast<uint8_t>(in[i++]);
        frame.fin = (b0 & 0x80) != 0;
        frame.opcode = (b0 & 0x0F);
        frame.masked = (b1 & 0x80) != 0;
        uint64_t len = (b1 & 0x7F);
        if (len == 126) {
            if (in.size() < i + 2) return {ResultCode::FRAME_ERROR, "incomplete 16-bit length"};
            len = (static_cast<uint8_t>(in[i]) << 8) | static_cast<uint8_t>(in[i + 1]);
            i += 2;
        } else if (len == 127) {
            if (in.size() < i + 8) return {ResultCode::FRAME_ERROR, "incomplete 64-bit length"};
            len = 0; for (int k = 0; k < 8; ++k) len = (len << 8) | static_cast<uint8_t>(in[i++]);
        }
        if (frame.masked) {
            if (in.size() < i + 4) return {ResultCode::FRAME_ERROR, "incomplete mask"};
            frame.mask_key = in.substr(i, 4);
            i += 4;
        }
        if (in.size() < i + len) return {ResultCode::FRAME_ERROR, "incomplete payload"};
        frame.payload = in.substr(i, static_cast<size_t>(len));
        if (frame.masked) {
            for (size_t k = 0; k < frame.payload.size(); ++k) frame.payload[k] ^= frame.mask_key[k % 4];
        }
        i += static_cast<size_t>(len);
        consumed = i;
        return {ResultCode::SUCCESS, ""};
    }
};

// ------------------------ Handshake ------------------------

class WebSocketHandshake {
public:
    static std::string buildRequest(const URL &url, const WebSocketConfig &cfg, std::string &client_key_b64, std::string &expected_accept_b64) {
        // Generate 16 random bytes and base64 encode for Sec-WebSocket-Key
        std::string client_key_raw = Utils::randomBytes(16);
        client_key_b64 = Utils::base64Encode(client_key_raw);

        // Expected accept = base64(SHA1(key + GUID)) where key is base64 form per RFC
        static const std::string GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        std::string accept_src = client_key_b64 + GUID;
        unsigned char sha[SHA_DIGEST_LENGTH];
        SHA1(reinterpret_cast<const unsigned char *>(accept_src.data()), static_cast<unsigned long>(accept_src.size()), sha);
        expected_accept_b64 = Utils::base64Encode(std::string(reinterpret_cast<char *>(sha), SHA_DIGEST_LENGTH));

        std::ostringstream req;
        req << "GET " << url.path() << " HTTP/1.1\r\n";
        req << "Host: " << url.host();
        if (!((url.port() == 80 && url.scheme() == "ws") || (url.port() == 443 && url.scheme() == "wss"))) {
            req << ":" << url.port();
        }
        req << "\r\n";
        req << "Upgrade: websocket\r\n";
        req << "Connection: Upgrade\r\n";
        req << "Sec-WebSocket-Key: " << client_key_b64 << "\r\n";
        req << "Sec-WebSocket-Version: 13\r\n";
        // custom headers
        for (const auto &kv : cfg.getHeaders()) req << kv.first << ": " << kv.second << "\r\n";
        // extensions header
        if (!cfg.getExtensions().empty()) {
            std::string ext;
            for (const auto &kv : cfg.getExtensions()) {
                if (!ext.empty()) ext += ", ";
                ext += kv.first;
                if (!kv.second.empty()) ext += "; " + kv.second;
            }
            req << "Sec-WebSocket-Extensions: " << ext << "\r\n";
        }
        req << "\r\n";
        return req.str();
    }

    static WebSocketResult validateResponse(const std::string &resp, const std::string &expected_accept_b64) {
        auto lines = Utils::split(resp, '\n');
        if (lines.empty()) return {ResultCode::HANDSHAKE_ERROR, "empty response"};
        std::string status = Utils::trim(lines[0]);
        if (status.find("HTTP/1.1 101") == std::string::npos) return {ResultCode::HANDSHAKE_ERROR, "bad status: " + status};
        bool has_upgrade = false, has_connection = false, has_accept = false;
        for (size_t i = 1; i < lines.size(); ++i) {
            std::string ln = Utils::trim(lines[i]);
            if (ln.empty()) break;
            size_t c = ln.find(':');
            if (c == std::string::npos) continue;
            std::string k = Utils::toLower(Utils::trim(ln.substr(0, c)));
            std::string v = Utils::trim(ln.substr(c + 1));
            if (k == "upgrade" && Utils::toLower(v).find("websocket") != std::string::npos) has_upgrade = true;
            else if (k == "connection" && Utils::toLower(v).find("upgrade") != std::string::npos) has_connection = true;
            else if (k == "sec-websocket-accept") { if (v == expected_accept_b64) has_accept = true; }
        }
        if (!has_upgrade) return {ResultCode::HANDSHAKE_ERROR, "missing Upgrade"};
        if (!has_connection) return {ResultCode::HANDSHAKE_ERROR, "missing Connection"};
        if (!has_accept) return {ResultCode::HANDSHAKE_ERROR, "bad Sec-WebSocket-Accept"};
        return {ResultCode::SUCCESS, ""};
    }
};

// ------------------------ WebSocketClient ------------------------

class WebSocketClient {
public:
    WebSocketClient() : state_(WebSocketState::CLOSED), stop_(false) {}
    explicit WebSocketClient(const WebSocketConfig &cfg) : config_(cfg), state_(WebSocketState::CLOSED), stop_(false) {}
    ~WebSocketClient() { disconnect(); }

    // Callbacks
    void setOnText(std::function<void(const std::string &)> cb) { on_text_ = std::move(cb); }
    void setOnBinary(std::function<void(const std::vector<uint8_t> &)> cb) { on_binary_ = std::move(cb); }
    void setOnOpen(std::function<void()> cb) { on_open_ = std::move(cb); }
    void setOnClose(std::function<void()> cb) { on_close_ = std::move(cb); }
    void setOnError(std::function<void(const WebSocketResult &)> cb) { on_error_ = std::move(cb); }

    WebSocketState getState() const noexcept { return state_.load(); }
    const WebSocketConfig &getConfig() const noexcept { return config_; }

    // Synchronous connect; starts one worker thread for IO after handshake
    WebSocketResult connect(const std::string &url) {
        if (getState() != WebSocketState::CLOSED) return {ResultCode::INVALID_STATE, "already open or connecting"};
        URL u; auto pr = u.parse(url); if (!pr) return pr;
        parsed_url_ = u;

        state_.store(WebSocketState::CONNECTING);
        bool use_ssl = (u.scheme() == "wss");
        auto rc = conn_.connect(u.host(), u.port(), use_ssl, config_.getTimeout());
        if (!rc) { state_.store(WebSocketState::CLOSED); emitError(rc); return rc; }

        std::string client_key_b64, expected_accept_b64;
        std::string req = WebSocketHandshake::buildRequest(u, config_, client_key_b64, expected_accept_b64);
        rc = conn_.sendAll(req.data(), req.size());
        if (!rc) { state_.store(WebSocketState::CLOSED); emitError(rc); conn_.close(); return rc; }

        std::string resp;
        // read until header end \r\n\r\n or timeout
        auto start = std::chrono::steady_clock::now();
        char buf[2048];
        while (resp.find("\r\n\r\n") == std::string::npos) {
            int left_ms = config_.getTimeout() - static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
            if (left_ms <= 0) { conn_.close(); state_.store(WebSocketState::CLOSED); return {ResultCode::TIMEOUT, "handshake timeout"}; }
            int n = conn_.recvSome(buf, sizeof(buf), std::min(left_ms, 500));
            if (n < 0) { conn_.close(); state_.store(WebSocketState::CLOSED); return {ResultCode::CONNECTION_ERROR, "read failed"}; }
            if (n == 0) continue; // timeout slice
            resp.append(buf, buf + n);
            if (resp.size() > 32 * 1024) { conn_.close(); state_.store(WebSocketState::CLOSED); return {ResultCode::HANDSHAKE_ERROR, "response too large"}; }
        }

        // Validate headers only
        size_t header_end = resp.find("\r\n\r\n");
        std::string headers = resp.substr(0, header_end);
        rc = WebSocketHandshake::validateResponse(headers, expected_accept_b64);
        if (!rc) { conn_.close(); state_.store(WebSocketState::CLOSED); emitError(rc); return rc; }

        // Ready
        state_.store(WebSocketState::OPEN);
        if (on_open_) on_open_();

        stop_ = false;
        worker_ = std::thread([this] { this->runLoop(); });
        return {ResultCode::SUCCESS, ""};
    }

    void disconnect() {
        WebSocketState st = getState();
        if (st == WebSocketState::CLOSED) return;
        if (st == WebSocketState::OPEN) {
            // try to send close frame (best-effort)
            sendFrame(FrameType::CLOSE, std::string());
        }
        stop_ = true;
        if (worker_.joinable()) worker_.join();
        conn_.close();
        state_.store(WebSocketState::CLOSED);
        if (on_close_) on_close_();
    }

    WebSocketResult send(const std::string &text) { return sendFrame(FrameType::TEXT, text); }
    WebSocketResult sendText(const std::string &text) { return sendFrame(FrameType::TEXT, text); }
    WebSocketResult sendBinary(const std::string &data) { return sendFrame(FrameType::BINARY, data); }
    WebSocketResult ping(const std::string &data = std::string()) { return sendFrame(FrameType::PING, data); }

private:
    void runLoop() {
        std::string recv_buf;
        auto last_ping = std::chrono::steady_clock::now();
        while (!stop_) {
            // periodic ping
            if (config_.getPingInterval() > 0) {
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_ping).count() >= config_.getPingInterval()) {
                    sendFrame(FrameType::PING, ""); // ignore result
                    last_ping = now;
                }
            }

            char tmp[4096];
            int n = conn_.recvSome(tmp, sizeof(tmp), 200);
            if (n < 0) {
                // connection error/closed by peer
                break;
            }
            if (n == 0) continue; // timeout slice
            recv_buf.append(tmp, tmp + n);

            // parse frames as far as possible
            while (true) {
                WebSocketFrame f; size_t used = 0;
                WebSocketResult pr = WebSocketFrame::parse(recv_buf, used, f);
                if (!pr) break; // need more
                handleFrame(f);
                recv_buf.erase(0, used);
                if (recv_buf.empty()) break;
            }
        }
    }

    void handleFrame(const WebSocketFrame &f) {
        switch (static_cast<FrameType>(f.opcode)) {
            case FrameType::TEXT:
                if (on_text_) on_text_(f.payload);
                break;
            case FrameType::BINARY: {
                if (on_binary_) on_binary_(std::vector<uint8_t>(f.payload.begin(), f.payload.end()));
                break;
            }
            case FrameType::PING: {
                // reply pong with same payload
                sendFrame(FrameType::PONG, f.payload);
                break;
            }
            case FrameType::PONG:
                // ignore
                break;
            case FrameType::CLOSE: {
                // echo close and stop
                sendFrame(FrameType::CLOSE, std::string());
                stop_ = true;
                break;
            }
            default:
                break;
        }
    }

    WebSocketResult sendFrame(FrameType type, const std::string &payload) {
        if (getState() != WebSocketState::OPEN) return {ResultCode::INVALID_STATE, "not open"};
        WebSocketFrame f;
        f.fin = true;
        f.opcode = static_cast<uint8_t>(type);
        f.masked = true; // client must mask
        f.mask_key = Utils::randomBytes(4);
        f.payload = payload;
        std::string data = f.serialize();
        std::lock_guard<std::mutex> lk(send_mtx_);
        return conn_.sendAll(data.data(), data.size());
    }

    void emitError(const WebSocketResult &r) { if (on_error_) on_error_(r); }

    WebSocketConfig config_;
    std::atomic<WebSocketState> state_ { WebSocketState::CLOSED };
    NetworkConnection conn_;
    URL parsed_url_;

    std::thread worker_;          // single worker thread per client
    std::atomic<bool> stop_;
    std::mutex send_mtx_;

    std::function<void(const std::string &)> on_text_;
    std::function<void(const std::vector<uint8_t> &)> on_binary_;
    std::function<void()> on_open_;
    std::function<void()> on_close_;
    std::function<void(const WebSocketResult &)> on_error_;
};

} // namespace websocket

#endif // WEBSOCKET_CLIENT_HPP