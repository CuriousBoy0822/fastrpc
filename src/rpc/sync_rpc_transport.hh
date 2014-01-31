#pragma once
#include <string>
#include <mutex>
#include <sys/socket.h>
#include <atomic>
#include <memory>
#include "rpc_util/kvio.h"
#include "rpc_common/sock_helper.hh"
#include "rpc_util/buffered_rpc_stream.hh"
#include "rpc_parser.hh"
#include "sync_rpc.hh"
#include "tcp_provider.hh"

namespace rpc {

struct spinlock {
    spinlock() : lock_(ATOMIC_FLAG_INIT) {
    }
    void lock() {
        while (lock_.test_and_set(std::memory_order_acquire))
            ;
    }
    void unlock() {
        lock_.clear(std::memory_order_release);
    }
  private:
    std::atomic_flag lock_;
};

template <typename T>
struct buffered_sync_transport {
    buffered_sync_transport(int fd) 
	: tp_(new T(fd)), in_(tp_, 65536), out_(tp_, 65536) {
	assert(tp_ != NULL);
	assert(fd >= 0);
    }
    ~buffered_sync_transport() {
	delete tp_;
    }
    bool flush() {
	return out_.flush();
    }
    void shutdown() {
	flush();
	tp_->shutdown();
    }
    rpc_istream_base* in() {
	return &in_;
    }
    rpc_ostream_base* out() {
	return &out_;
    }

  private:
    T* tp_;
    buffered_rpc_istream<T> in_;
    buffered_rpc_ostream<T> out_;
};

template <typename T>
struct sync_rpc_transport : public spinlock {
    sync_rpc_transport() : p_(NULL), conn_(NULL), cid_(0) {
    }
    sync_rpc_transport(const std::string& h, int port) : conn_(NULL), cid_(0) {
	p_ = NULL;
	set_address(h, port, "0.0.0.0");
    }
    sync_rpc_transport(int fd) : conn_(NULL), cid_(0) {
	p_ = new onetime_tcpp(fd);
	assert(connect());
    }
    ~sync_rpc_transport() {
	if (p_)
	    delete p_;
        disconnect();
    }
    explicit sync_rpc_transport(const sync_rpc_transport&) = delete;
    void operator=(const sync_rpc_transport&) = delete;
    void set_address(const std::string& h, int port, const std::string& local) {
	if (p_)
	    delete p_;
	p_ = new multi_tcpp(h.c_str(), local.c_str(), port);
    }
    bool connect() {
        if (conn_ == NULL) {
	    int fd = p_->connect();
            if (fd < 0)
                return false;
            rpc::common::sock_helper::make_nodelay(fd);
	    conn_ = new T(fd);
        }
        return true;
    }
    bool connected() const {
        return conn_;
    }
    void disconnect() {
        if (conn_ != NULL) {
	    delete conn_;
	    conn_ = NULL;
	}
    }
    void shutdown() {
	if (conn_ != NULL)
	    conn_->shutdown();
    }
    bool hard_read(void* buffer, size_t len) {
	assert(connected());
	return conn_->in()->read((char*)buffer, len);
    }
    bool safe_flush() {
	this->lock();
	bool ok = flush();
	this->unlock();
	return ok;
    }

    template <typename REPLY>
    void safe_send_reply(const REPLY& r, const rpc::rpc_header& h, bool doflush) {
	this->lock();
	assert(connected());
	bool ok = rpc::send_reply(conn_->out(), h.mproc(), h.seq_, h.cid_, r);
	if (ok && doflush)
	    flush();
	this->unlock();
    }
    template <typename M>
    bool read_reply(rpc::rpc_header& h, M& m) {
	assert(connected());
	return rpc::read_reply(conn_->in(), m, h);
    }
    template <typename PROC, typename M>
    bool send_request(PROC proc, uint32_t seq, const M& m, bool doflush) {
	assert(connected());
	bool ok = rpc::send_request(conn_->out(), proc, seq, cid_, m);
	if (ok && doflush)
	    conn_->flush();
	return ok;
    }
    template <typename PROC, typename REPLY>
    bool write_reply(PROC mproc, int seq, const REPLY& r, uint64_t /*latency*/) {
	assert(connected());
	bool ok = rpc::send_reply(conn_->out(), mproc, seq, cid_, r);
	if (ok && /*doflush*/false)
	    flush();
	return ok;
    }
    template <typename PROC, typename M, typename REPLY>
    bool sync_call(uint32_t seq, PROC proc, const M& req, REPLY& r) {
	assert(connected());
	return rpc::sync_call(conn_->out(), conn_->in(), cid_, seq, proc, req, r);
    }
    bool flush() {
	assert(connected());
	return conn_->flush();
    }
    void set_rpc_clientid(int cid) {
	cid_ = cid;
    }
  protected:
    tcp_provider* p_;
    T* conn_;
    int cid_;
};
}
