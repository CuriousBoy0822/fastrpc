#pragma once

#include "rpc_common/util.hh"
#include "rpc_parser.hh"
#include <type_traits>

namespace rpc {

struct gcrequest_base {
    virtual void process_reply(parser& p) = 0;
    virtual void process_connection_error() = 0;
    virtual uint32_t proc() const = 0;
    virtual uint64_t start_at() const = 0;
    virtual ~gcrequest_base() {
    }
    uint32_t seq_;
    static uint32_t last_server_latency_;
};

template <typename T>
struct has_eno {
    template <typename C>
    static uint8_t test(typename C::set_eno*);
    template <typename>
    static uint32_t test(...);
    static const bool value = (sizeof(test<T>(0)) == 1);
};

template <typename T>
typename std::enable_if<has_eno<T>::value, void>::type set_default_eno(T* r) {
    r->set_eno(app_param::ErrorCode::RPCERR);
}

template <typename T>
typename std::enable_if<!has_eno<T>::value, void>::type set_default_eno(T* r) {
}

template <uint32_t PROC, typename F>
struct gcrequest_iface : public gcrequest_base {
    typedef typename analyze_grequest<PROC, false>::request_type request_type;
    typedef typename analyze_grequest<PROC, false>::reply_type reply_type;

    gcrequest_iface(F callback): cb_(callback), tstart_(rpc::common::tstamp()) {
    }
    void process_reply(parser& p) {
	p.parse_message(reply_);
	cb_.operator()(req(), reply_);
        delete this;
    }
    void process_connection_error() {
        //reply_.set_eno(app_param::ErrorCode::RPCERR);
        set_default_eno(&reply_);
	cb_.operator()(req(), reply_);
        delete this;
    }
    uint32_t proc() const {
	return PROC;
    }
    uint64_t start_at() const {
	return tstart_;
    }
    virtual request_type& req() = 0;
    reply_type reply_;
  private:
    F cb_;
    uint64_t tstart_;
};

// client request with inline storage for request
template <uint32_t PROC, typename F>
struct gcrequest : public gcrequest_iface<PROC, F> {
    typedef typename gcrequest_iface<PROC, F>::request_type request_type;

    gcrequest(F callback): gcrequest_iface<PROC, F>(callback)  {
    }
    request_type& req() {
	return req_;
    }
    request_type req_;
};

// client request without storage for request
template <uint32_t PROC, typename F>
struct gcrequest_external : public gcrequest_iface<PROC, F> {
    typedef typename gcrequest_iface<PROC, F>::request_type request_type;

    gcrequest_external(F callback, request_type* req) 
	: gcrequest_iface<PROC, F>(callback), req_(req)  {
    }
    request_type& req() {
	assert(req_);
	return *req_;
    }
    request_type* req_;
};

};

