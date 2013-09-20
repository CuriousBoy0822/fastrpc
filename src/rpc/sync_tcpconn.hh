#pragma once
#include <string>
#include <mutex>
#include <sys/socket.h>
#include "rpc_util/kvio.h"

namespace rpc {

struct sync_tcpconn {
    sync_tcpconn() : h_(""), port_(0), fd_(-1), in_(NULL), out_(NULL) {
    }
    explicit sync_tcpconn(sync_tcpconn& ) {
        assert(0); // disallow copying of mu_
    }
    sync_tcpconn(const std::string& h, int port) 
        : h_(h), port_(port), fd_(-1), in_(NULL), out_(NULL) {
    }
    ~sync_tcpconn() {
        disconnect();
    }
    void init(const std::string& h, int port, const std::string& localhost, int localport) {
        h_ = h;
        port_ = port;
        localhost_ = localhost;
	localport_ = localport;
    }
    void lock() {
        mu_.lock();
    }
    void unlock() {
        mu_.unlock();
    }
    bool connect() {
        if (fd_ < 0) {
            int fd = rpc::common::sock_helper::connect(h_.c_str(), port_, localhost_.c_str(), localport_);
            if (fd < 0)
                return false;
            rpc::common::sock_helper::make_nodelay(fd);
            fd_ = fd;
            in_ = new kvin(fd_, 64*1024);
            out_ = new kvout(fd_, 1024*1024);
        }
        return true;
    }
    bool connected() {
        return fd_ >= 0;
    }
    void disconnect() {
        if (fd_ >= 0) {
            close(fd_);
            delete in_;
            delete out_;
            fd_ = -1;
            in_ = NULL;
            out_ = NULL;
        }
    }
    int fd() {
        return fd_;
    }
    void shutdown() {
        ::shutdown(SHUT_RDWR, fd_);
    }
    kvin* in() {
        return in_;
    }
    kvout* out() {
        return out_;
    }
  protected:
    std::string h_;
    int port_;
    std::string localhost_;
    int localport_;
    int fd_;
    kvin* in_;
    kvout* out_;
    std::mutex mu_;
};
}