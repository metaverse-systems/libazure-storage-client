#pragma once

// Loopback HTTP stub server used by the tests to script table-service replies.
// Linux only. It binds 127.0.0.1 on an ephemeral port, answers one scripted
// response per request (one request per connection) and counts the requests.

#ifdef __linux__

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct StubResponse {
    int status = 200;
    std::string body;
    std::map<std::string, std::string> headers;
};

class StubHttpServer {
public:
    explicit StubHttpServer(std::vector<StubResponse> script)
        : script(std::move(script)) {
        this->listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
        int one = 1;
        ::setsockopt(this->listenFd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;
        ::bind(this->listenFd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
        socklen_t len = sizeof(addr);
        ::getsockname(this->listenFd, reinterpret_cast<sockaddr *>(&addr), &len);
        this->portNumber = ntohs(addr.sin_port);
        ::listen(this->listenFd, 8);
        this->worker = std::thread([this]() { this->Serve(); });
    }

    ~StubHttpServer() {
        this->stopping = true;
        ::shutdown(this->listenFd, SHUT_RDWR);
        ::close(this->listenFd);
        if (this->worker.joinable()) this->worker.join();
    }

    StubHttpServer(const StubHttpServer &) = delete;
    StubHttpServer &operator=(const StubHttpServer &) = delete;

    std::string Endpoint() const {
        return "http://127.0.0.1:" + std::to_string(this->portNumber);
    }

    int RequestCount() const { return this->requests.load(); }

    std::vector<std::string> RequestLines() {
        std::lock_guard<std::mutex> lock(this->mutex);
        return this->lines;
    }

private:
    void Serve() {
        while (!this->stopping) {
            int fd = ::accept(this->listenFd, nullptr, nullptr);
            if (fd < 0) break;
            std::string req;
            char buf[4096];
            while (req.find("\r\n\r\n") == std::string::npos) {
                ssize_t n = ::recv(fd, buf, sizeof(buf), 0);
                if (n <= 0) break;
                req.append(buf, static_cast<size_t>(n));
            }
            if (req.empty()) { ::close(fd); continue; }
            size_t index = static_cast<size_t>(this->requests.fetch_add(1));
            {
                std::lock_guard<std::mutex> lock(this->mutex);
                this->lines.push_back(req.substr(0, req.find("\r\n")));
            }
            StubResponse r;
            if (!this->script.empty())
                r = this->script[index < this->script.size() ? index : this->script.size() - 1];
            std::string out = "HTTP/1.1 " + std::to_string(r.status) + " X\r\n";
            out += "Content-Length: " + std::to_string(r.body.size()) + "\r\n";
            out += "Content-Type: application/json\r\nConnection: close\r\n";
            for (const auto &h : r.headers) out += h.first + ": " + h.second + "\r\n";
            out += "\r\n" + r.body;
            size_t sent = 0;
            while (sent < out.size()) {
                ssize_t n = ::send(fd, out.data() + sent, out.size() - sent, MSG_NOSIGNAL);
                if (n <= 0) break;
                sent += static_cast<size_t>(n);
            }
            ::close(fd);
        }
    }

    std::vector<StubResponse> script;
    int listenFd = -1;
    unsigned short portNumber = 0;
    std::atomic<bool> stopping{false};
    std::atomic<int> requests{0};
    std::mutex mutex;
    std::vector<std::string> lines;
    std::thread worker;
};

#endif // __linux__
