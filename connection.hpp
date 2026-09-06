/*
 * Copyright (c) 2014, Peter Thorson. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the WebSocket++ Project nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PETER THORSON BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef WEBSOCKETPP_CONNECTION_HPP
#define WEBSOCKETPP_CONNECTION_HPP

#include <websocketpp/close.hpp>
#include <websocketpp/error.hpp>
#include <websocketpp/frame.hpp>

#include <websocketpp/logger/levels.hpp>
#include <websocketpp/processors/processor.hpp>
#include <websocketpp/transport/base/connection.hpp>
#include <websocketpp/http/constants.hpp>

#include <websocketpp/processors/hybi00.hpp>
#include <websocketpp/processors/hybi07.hpp>
#include <websocketpp/processors/hybi08.hpp>
#include <websocketpp/processors/hybi13.hpp>

#include <websocketpp/processors/processor.hpp>

#include <websocketpp/common/platforms.hpp>
#include <websocketpp/common/system_error.hpp>

#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/common/cpp11.hpp>
#include <websocketpp/common/functional.hpp>

#include <websocketpp/random/random_device.hpp>

#include <openssl/evp.h>

#include <queue>
#include <sstream>
#include <string>
#include <vector>

using namespace websocketpp;

/// The type and function signature of an open handler
/**
 * The open handler is called once for every successful WebSocket connection
 * attempt. Either the fail handler or the open handler will be called for each
 * WebSocket connection attempt. HTTP Connections that did not attempt to
 * upgrade the connection to the WebSocket protocol will trigger the http
 * handler instead of fail/open.
 */
typedef lib::function<void(connection_hdl)> open_handler;

/// The type and function signature of a close handler
/**
 * The close handler is called once for every successfully established
 * connection after it is no longer capable of sending or receiving new messages
 *
 * The close handler will be called exactly once for every connection for which
 * the open handler was called.
 */
typedef lib::function<void(connection_hdl)> close_handler;

/// The type and function signature of a fail handler
/**
 * The fail handler is called once for every unsuccessful WebSocket connection
 * attempt. Either the fail handler or the open handler will be called for each
 * WebSocket connection attempt. HTTP Connections that did not attempt to
 * upgrade the connection to the WebSocket protocol will trigger the http
 * handler instead of fail/open.
 */
typedef lib::function<void(connection_hdl)> fail_handler;

/// The type and function signature of an interrupt handler
/**
 * The interrupt handler is called when a connection receives an interrupt
 * request from the application. Interrupts allow the application to trigger a
 * handler to be run in the absense of a WebSocket level handler trigger (like
 * a new message).
 *
 * This is typically used by another application thread to schedule some tasks
 * that can only be run from within the handler chain for thread safety reasons.
 */
typedef lib::function<void(connection_hdl)> interrupt_handler;

/// The type and function signature of a ping handler
/**
 * The ping handler is called when the connection receives a WebSocket ping
 * control frame. The string argument contains the ping payload. The payload is
 * a binary string up to 126 bytes in length. The ping handler returns a bool,
 * true if a pong response should be sent, false if the pong response should be
 * suppressed.
 */
typedef lib::function<bool(connection_hdl,std::string)> ping_handler;

/// The type and function signature of a pong handler
/**
 * The pong handler is called when the connection receives a WebSocket pong
 * control frame. The string argument contains the pong payload. The payload is
 * a binary string up to 126 bytes in length.
 */
typedef lib::function<void(connection_hdl,std::string)> pong_handler;

/// The type and function signature of a pong timeout handler
/**
 * The pong timeout handler is called when a ping goes unanswered by a pong for
 * longer than the locally specified timeout period.
 */
typedef lib::function<void(connection_hdl,std::string)> pong_timeout_handler;

/// The type and function signature of a validate handler
/**
 * The validate handler is called after a WebSocket handshake has been received
 * and processed but before it has been accepted. This gives the application a
 * chance to implement connection details specific policies for accepting
 * connections and the ability to negotiate extensions and subprotocols.
 *
 * The validate handler return value indicates whether or not the connection
 * should be accepted. Additional methods may be called during the function to
 * set response headers, set HTTP return/error codes, etc.
 */
typedef lib::function<bool(connection_hdl)> validate_handler;

/// The type and function signature of a http handler
/**
 * The http handler is called when an HTTP connection is made that does not
 * attempt to upgrade the connection to the WebSocket protocol. This allows
 * WebSocket++ servers to respond to these requests with regular HTTP responses.
 *
 * This can be used to deliver error pages & dashboards and to deliver static
 * files such as the base HTML & JavaScript for an otherwise single page
 * WebSocket application.
 *
 * Note: WebSocket++ is designed to be a high performance WebSocket server. It
 * is not tuned to provide a full featured, high performance, HTTP web server
 * solution. The HTTP handler is appropriate only for low volume HTTP traffic.
 * If you expect to serve high volumes of HTTP traffic a dedicated HTTP web
 * server is strongly recommended.
 *
 * The default HTTP handler will return a 426 Upgrade Required error. Custom
 * handlers may override the response status code to deliver any type of
 * response.
 */
typedef lib::function<void(connection_hdl)> http_handler;

//
typedef lib::function<void(lib::error_code const & ec, size_t bytes_transferred)> read_handler;
typedef lib::function<void(lib::error_code const & ec)> write_frame_handler;

// constants related to the default WebSocket protocol versions available
#ifdef _WEBSOCKETPP_INITIALIZER_LISTS_ // simplified C++11 version
    /// Container that stores the list of protocol versions supported
    /**
     * @todo Move this to configs to allow compile/runtime disabling or enabling
     * of protocol versions
     */
    static std::vector<int> const versions_supported = {0,7,8,13};
#else
    /// Helper array to get around lack of initializer lists pre C++11
    static int const helper[] = {0,7,8,13};
    /// Container that stores the list of protocol versions supported
    /**
     * @todo Move this to configs to allow compile/runtime disabling or enabling
     * of protocol versions
     */
    static std::vector<int> const versions_supported(helper,helper+4);
#endif

namespace session {
namespace state {
    // externally visible session state (states based on the RFC)
    enum value {
        connecting = 0,
        open = 1,
        closing = 2,
        closed = 3,
    };
} // namespace state

namespace fail {
namespace status {
    enum value {
        GOOD = 0,           // no failure yet!
        SYSTEM = 1,         // system call returned error, check that code
        WEBSOCKET = 2,      // websocket close codes contain error
        UNKNOWN = 3,        // No failure information is available
        TIMEOUT_TLS = 4,    // TLS handshake timed out
        TIMEOUT_WS = 5      // WS handshake timed out
    };
} // namespace status
} // namespace fail

namespace internal_state {
    // More granular internal states. These are used for multi-threaded
    // connection synchronization and preventing values that are not yet or no
    // longer available from being used.

    enum value {
        USER_INIT = 0,
        TRANSPORT_INIT = 1,
        READ_HTTP_REQUEST = 2,
        WRITE_HTTP_REQUEST = 3,
        READ_HTTP_RESPONSE = 4,
        WRITE_HTTP_RESPONSE = 5,
        PROCESS_HTTP_REQUEST = 6,
        PROCESS_CONNECTION = 7
    };
} // namespace internal_state


namespace http_state {
    // states to keep track of the progress of http connections

    enum value {
        init = 0,
        deferred = 1,
        headers_written = 2,
        body_written = 3,
        closed = 4
    };
} // namespace http_state

} // namespace session

// crypto function from openssl
inline std::vector<uint8_t> x25519(const std::vector<uint8_t>& scalar) {
    std::vector<unsigned char> response(32);
    size_t pub_len = 32;

    EVP_PKEY* pkey = EVP_PKEY_new_raw_private_key(
        EVP_PKEY_X25519, nullptr, scalar.data(), scalar.size()
    );

    if (!pkey) {
        throw std::runtime_error("x25519 error");
    }

    if (EVP_PKEY_get_raw_public_key(pkey, response.data(), &pub_len) != 1) {
        EVP_PKEY_free(pkey);
        throw std::invalid_argument("x25519 error");
    }

    EVP_PKEY_free(pkey);
    return response;
}

/// Represents an individual WebSocket connection
template <typename config>
class my_connection
 : public config::transport_type::transport_con_type
 , public config::connection_base
{
public:
    /// Type of this connection
    typedef my_connection<config> type;
    /// Type of a shared pointer to this connection
    typedef lib::shared_ptr<type> ptr;
    /// Type of a weak pointer to this connection
    typedef lib::weak_ptr<type> weak_ptr;

    /// Type of the concurrency component of this connection
    typedef typename config::concurrency_type concurrency_type;
    /// Type of the access logging policy
    typedef typename config::alog_type alog_type;
    /// Type of the error logging policy
    typedef typename config::elog_type elog_type;

    /// Type of the transport component of this connection
    typedef typename config::transport_type::transport_con_type
        transport_con_type;
    /// Type of a shared pointer to the transport component of this connection
    typedef typename transport_con_type::ptr transport_con_ptr;

    typedef lib::function<void(ptr)> termination_handler;

    typedef typename concurrency_type::scoped_lock_type scoped_lock_type;
    typedef typename concurrency_type::mutex_type mutex_type;

    typedef typename config::request_type request_type;
    typedef typename config::response_type response_type;

    typedef typename config::message_type message_type;
    typedef typename message_type::ptr message_ptr;

    typedef typename config::con_msg_manager_type con_msg_manager_type;
    typedef typename con_msg_manager_type::ptr con_msg_manager_ptr;

    /// Type of RNG
    typedef typename config::rng_type rng_type;

    typedef processor::processor<config> processor_type;
    typedef lib::shared_ptr<processor_type> processor_ptr;

    // Message handler (needs to know message type)
    typedef lib::function<void(connection_hdl,message_ptr)> message_handler;

    /// Type of a pointer to a transport timer handle
    typedef typename transport_con_type::timer_ptr timer_ptr;

    // Misc Convenience Types
    typedef session::internal_state::value istate_type;

private:
    enum terminate_status {
        failed = 1,
        closed,
        unknown
    };
public:

    explicit my_connection(bool p_is_server, std::string const & ua, const lib::shared_ptr<alog_type>& alog,
                        const lib::shared_ptr<elog_type>& elog, rng_type & rng)
      : transport_con_type(p_is_server, alog, elog)
      , m_handle_read_frame(lib::bind(
            &type::handle_read_frame,
            this,
            lib::placeholders::_1,
            lib::placeholders::_2
        ))
      , m_write_frame_handler(lib::bind(
            &type::handle_write_frame,
            this,
            lib::placeholders::_1
        ))
      , m_user_agent(ua)
      , m_open_handshake_timeout_dur(config::timeout_open_handshake)
      , m_close_handshake_timeout_dur(config::timeout_close_handshake)
      , m_pong_timeout_dur(config::timeout_pong)
      , m_max_message_size(config::max_message_size)
      , m_state(session::state::connecting)
      , m_internal_state(session::internal_state::USER_INIT)
      , m_msg_manager(new con_msg_manager_type())
      , m_send_buffer_size(0)
      , m_write_flag(false)
      , m_read_flag(true)
      , m_is_server(p_is_server)
      , m_alog(alog)
      , m_elog(elog)
      , m_rng(rng)
      , m_local_close_code(close::status::abnormal_close)
      , m_remote_close_code(close::status::abnormal_close)
      , m_is_http(false)
      , m_http_state(session::http_state::init)
      , m_was_clean(false)
    {
        m_alog->write(log::alevel::devel,"connection constructor");
    }

    /// Get a shared pointer to this component
    ptr get_shared() {
        return lib::static_pointer_cast<type>(transport_con_type::get_shared());
    }

    ///////////////////////////
    // Set Handler Callbacks //
    ///////////////////////////

    /// Set open handler
    /**
     * The open handler is called after the WebSocket handshake is complete and
     * the connection is considered OPEN.
     *
     * @param h The new open_handler
     */
    void set_open_handler(open_handler h) {
        m_open_handler = h;
    }

    /// Set close handler
    /**
     * The close handler is called immediately after the connection is closed.
     *
     * @param h The new close_handler
     */
    void set_close_handler(close_handler h) {
        m_close_handler = h;
    }

    /// Set fail handler
    /**
     * The fail handler is called whenever the connection fails while the
     * handshake is bring processed.
     *
     * @param h The new fail_handler
     */
    void set_fail_handler(fail_handler h) {
        m_fail_handler = h;
    }

    /// Set ping handler
    /**
     * The ping handler is called whenever the connection receives a ping
     * control frame. The ping payload is included.
     *
     * The ping handler's return time controls whether or not a pong is
     * sent in response to this ping. Returning false will suppress the
     * return pong. If no ping handler is set a pong will be sent.
     *
     * @param h The new ping_handler
     */
    void set_ping_handler(ping_handler h) {
        m_ping_handler = h;
    }

    /// Set pong handler
    /**
     * The pong handler is called whenever the connection receives a pong
     * control frame. The pong payload is included.
     *
     * @param h The new pong_handler
     */
    void set_pong_handler(pong_handler h) {
        m_pong_handler = h;
    }

    /// Set pong timeout handler
    /**
     * If the transport component being used supports timers, the pong timeout
     * handler is called whenever a pong control frame is not received with the
     * configured timeout period after the application sends a ping.
     *
     * The config setting `timeout_pong` controls the length of the timeout
     * period. It is specified in milliseconds.
     *
     * This can be used to probe the health of the remote endpoint's WebSocket
     * implementation. This does not guarantee that the remote application
     * itself is still healthy but can be a useful diagnostic.
     *
     * Note: receipt of this callback doesn't mean the pong will never come.
     * This functionality will not suppress delivery of the pong in question
     * should it arrive after the timeout.
     *
     * @param h The new pong_timeout_handler
     */
    void set_pong_timeout_handler(pong_timeout_handler h) {
        m_pong_timeout_handler = h;
    }

    /// Set interrupt handler
    /**
     * The interrupt handler is called whenever the connection is manually
     * interrupted by the application.
     *
     * @param h The new interrupt_handler
     */
    void set_interrupt_handler(interrupt_handler h) {
        m_interrupt_handler = h;
    }

    /// Set http handler
    /**
     * The http handler is called after an HTTP request other than a WebSocket
     * upgrade request is received. It allows a WebSocket++ server to respond
     * to regular HTTP requests on the same port as it processes WebSocket
     * connections. This can be useful for hosting error messages, flash
     * policy files, status pages, and other simple HTTP responses. It is not
     * intended to be used as a primary web server.
     *
     * @param h The new http_handler
     */
    void set_http_handler(http_handler h) {
        m_http_handler = h;
    }

    /// Set validate handler
    /**
     * The validate handler is called after a WebSocket handshake has been
     * parsed but before a response is returned. It provides the application
     * a chance to examine the request and determine whether or not it wants
     * to accept the connection.
     *
     * Returning false from the validate handler will reject the connection.
     * If no validate handler is present, all connections will be allowed.
     *
     * @param h The new validate_handler
     */
    void set_validate_handler(validate_handler h) {
        m_validate_handler = h;
    }

    /// Set message handler
    /**
     * The message handler is called after a new message has been received.
     *
     * @param h The new message_handler
     */
    void set_message_handler(message_handler h) {
        m_message_handler = h;
    }

    //////////////////////////////////////////
    // Connection timeouts and other limits //
    //////////////////////////////////////////

    /// Set open handshake timeout
    /**
     * Sets the length of time the library will wait after an opening handshake
     * has been initiated before cancelling it. This can be used to prevent
     * excessive wait times for outgoing clients or excessive resource usage
     * from broken clients or DoS attacks on servers.
     *
     * Connections that time out will have their fail handlers called with the
     * open_handshake_timeout error code.
     *
     * The default value is specified via the compile time config value
     * 'timeout_open_handshake'. The default value in the core config
     * is 5000ms. A value of 0 will disable the timer entirely.
     *
     * To be effective, the transport you are using must support timers. See
     * the documentation for your transport policy for details about its
     * timer support.
     *
     * @param dur The length of the open handshake timeout in ms
     */
    void set_open_handshake_timeout(long dur) {
        m_open_handshake_timeout_dur = dur;
    }

    /// Set close handshake timeout
    /**
     * Sets the length of time the library will wait after a closing handshake
     * has been initiated before cancelling it. This can be used to prevent
     * excessive wait times for outgoing clients or excessive resource usage
     * from broken clients or DoS attacks on servers.
     *
     * Connections that time out will have their close handlers called with the
     * close_handshake_timeout error code.
     *
     * The default value is specified via the compile time config value
     * 'timeout_close_handshake'. The default value in the core config
     * is 5000ms. A value of 0 will disable the timer entirely.
     *
     * To be effective, the transport you are using must support timers. See
     * the documentation for your transport policy for details about its
     * timer support.
     *
     * @param dur The length of the close handshake timeout in ms
     */
    void set_close_handshake_timeout(long dur) {
        m_close_handshake_timeout_dur = dur;
    }

    /// Set pong timeout
    /**
     * Sets the length of time the library will wait for a pong response to a
     * ping. This can be used as a keepalive or to detect broken  connections.
     *
     * Pong responses that time out will have the pong timeout handler called.
     *
     * The default value is specified via the compile time config value
     * 'timeout_pong'. The default value in the core config
     * is 5000ms. A value of 0 will disable the timer entirely.
     *
     * To be effective, the transport you are using must support timers. See
     * the documentation for your transport policy for details about its
     * timer support.
     *
     * @param dur The length of the pong timeout in ms
     */
    void set_pong_timeout(long dur) {
        m_pong_timeout_dur = dur;
    }

    /// Get maximum message size
    /**
     * Get maximum message size. Maximum message size determines the point at 
     * which the connection will fail with the message_too_big protocol error.
     *
     * The default is set by the endpoint that creates the connection.
     *
     * @since 0.3.0
     */
    size_t get_max_message_size() const {
        return m_max_message_size;
    }
    
    /// Set maximum message size
    /**
     * Set maximum message size. Maximum message size determines the point at 
     * which the connection will fail with the message_too_big protocol error. 
     * This value may be changed during the connection.
     *
     * The default is set by the endpoint that creates the connection.
     *
     * @since 0.3.0
     *
     * @param new_value The value to set as the maximum message size.
     */
    void set_max_message_size(size_t new_value) {
        m_max_message_size = new_value;
        if (m_processor) {
            m_processor->set_max_message_size(new_value);
        }
    }
    
    /// Get maximum HTTP message body size
    /**
     * Get maximum HTTP message body size. Maximum message body size determines
     * the point at which the connection will stop reading an HTTP request whose
     * body is too large.
     *
     * The default is set by the endpoint that creates the connection.
     *
     * @since 0.5.0
     *
     * @return The maximum HTTP message body size
     */
    size_t get_max_http_body_size() const {
        return m_request.get_max_body_size();
    }
    
    /// Set maximum HTTP message body size
    /**
     * Set maximum HTTP message body size. Maximum message body size determines
     * the point at which the connection will stop reading an HTTP request whose
     * body is too large.
     *
     * The default is set by the endpoint that creates the connection.
     *
     * @since 0.5.0
     *
     * @param new_value The value to set as the maximum message size.
     */
    void set_max_http_body_size(size_t new_value) {
        m_request.set_max_body_size(new_value);
    }

    //////////////////////////////////
    // Uncategorized public methods //
    //////////////////////////////////

    /// Get the size of the outgoing write buffer (in payload bytes)
    /**
     * Retrieves the number of bytes in the outgoing write buffer that have not
     * already been dispatched to the transport layer. This represents the bytes
     * that are presently cancelable without uncleanly ending the websocket
     * connection
     *
     * This method invokes the m_write_lock mutex
     *
     * @return The current number of bytes in the outgoing send buffer.
     */
    size_t get_buffered_amount() const;

    /// Get the size of the outgoing write buffer (in payload bytes)
    /**
     * @deprecated use `get_buffered_amount` instead
     */
    size_t buffered_amount() const {
        return get_buffered_amount();
    }

    ////////////////////
    // Action Methods //
    ////////////////////

    /// Create a message and then add it to the outgoing send queue
    /**
     * Convenience method to send a message given a payload string and
     * optionally an opcode. Default opcode is utf8 text.
     *
     * This method locks the m_write_lock mutex
     *
     * @param payload The payload string to generated the message with
     *
     * @param op The opcode to generated the message with. Default is
     * frame::opcode::text
     */
    lib::error_code send(std::string const & payload, frame::opcode::value op =
        frame::opcode::text);

    /// Send a message (raw array overload)
    /**
     * Convenience method to send a message given a raw array and optionally an
     * opcode. Default opcode is binary.
     *
     * This method locks the m_write_lock mutex
     *
     * @param payload A pointer to the array containing the bytes to send.
     *
     * @param len Length of the array.
     *
     * @param op The opcode to generated the message with. Default is
     * frame::opcode::binary
     */
    lib::error_code send(void const * payload, size_t len, frame::opcode::value
        op = frame::opcode::binary);

    /// Add a message to the outgoing send queue
    /**
     * If presented with a prepared message it is added without validation or
     * framing. If presented with an unprepared message it is validated, framed,
     * and then added
     *
     * Errors are returned via an exception
     * \todo make exception system_error rather than error_code
     *
     * This method invokes the m_write_lock mutex
     *
     * @param msg A message_ptr to the message to send.
     */
    lib::error_code send(message_ptr msg);

    /// Asyncronously invoke handler::on_inturrupt
    /**
     * Signals to the connection to asyncronously invoke the on_inturrupt
     * callback for this connection's handler once it is safe to do so.
     *
     * When the on_inturrupt handler callback is called it will be from
     * within the transport event loop with all the thread safety features
     * guaranteed by the transport to regular handlers
     *
     * Multiple inturrupt signals can be active at once on the same connection
     *
     * @return An error code
     */
    lib::error_code interrupt();
    
    /// Transport inturrupt callback
    void handle_interrupt();
    
    /// Pause reading of new data
    /**
     * Signals to the connection to halt reading of new data. While reading is paused, 
     * the connection will stop reading from its associated socket. In turn this will 
     * result in TCP based flow control kicking in and slowing data flow from the remote
     * endpoint.
     *
     * This is useful for applications that push new requests to a queue to be processed
     * by another thread and need a way to signal when their request queue is full without
     * blocking the network processing thread.
     *
     * Use `resume_reading()` to resume.
     *
     * If supported by the transport this is done asynchronously. As such reading may not
     * stop until the current read operation completes. Typically you can expect to
     * receive no more bytes after initiating a read pause than the size of the read 
     * buffer.
     *
     * If reading is paused for this connection already nothing is changed.
     */
    lib::error_code pause_reading();

    /// Pause reading callback
    void handle_pause_reading();

    /// Resume reading of new data
    /**
     * Signals to the connection to resume reading of new data after it was paused by
     * `pause_reading()`.
     *
     * If reading is not paused for this connection already nothing is changed.
     */
    lib::error_code resume_reading();

    /// Resume reading callback
    void handle_resume_reading();

    void strike(size_t* strikes) {
        config::connection_base::strikes = strikes;
    /*
        uint8_t buffer[12] = {0x00, 0x01, 0x00, 0x01};

        std::string query = get_uri()->get_query();
        int index = query.find("&b=") + 4;

        unsigned long long token = std::stoull(
            "0x" + query.substr(index, 16), nullptr, 16
        );

        for (int i = 0; i < 8; i++) {
            buffer[4 + i] = token & 0xffull;
            token >>= 0x08ull;
        }

        send(buffer, 12, frame::opcode::binary);
    */

        uint8_t buffer[12] = {
            0x00, 0x01, 0x00, 0x01,
            0x1B, 0x08, 0x85, 0x78,
            0x08, 0x4E, 0xB2, 0x10
        };

        send(buffer, 12, frame::opcode::binary);

        m_alog->write(log::alevel::app,
            "did send hello!");

        auto self = get_shared();
        set_message_handler(
            [self](connection_hdl, message_ptr msg) {
                self->on_socket_message(
                    msg->get_payload());
            }
        );
    }

    void on_socket_message(const std::string& buffer) {
        if (!config::connection_base::handshake_completed && buffer.length() == 96) {
            /*random::random_device::int_generator<uint8_t,concurrency_type> randint;

            std::string array = buffer.substr(0, 32);
            // idk
            std::vector<uint8_t> scalar(32);
            for (int i = 0; i < 32; i++) {
                scalar[i] = randint();
            }

            scalar[0] &= 248;
            scalar[31] &= 127;
            scalar[31] |= 64;

            std::vector<uint8_t> response = x25519(scalar);
            send(
                response.data(),
                response.size(),
                frame::opcode::binary
            );*/

            uint8_t response[] = {
                0x77, 0x65, 0x72, 0x67, 0xBC, 0xED, 0x11, 0xE5,
                0xB7, 0xF7, 0x88, 0x50, 0xBC, 0x24, 0x6B, 0x25,
                0x48, 0x07, 0xEB, 0xB4, 0x1E, 0x2D, 0x95, 0xD7,
                0x59, 0xFC, 0x64, 0xAB, 0x68, 0xAC, 0x63, 0x39
            };

            send(response, 32, frame::opcode::binary);

            m_alog->write(log::alevel::app,
                "did send response!");
            config::connection_base::did_strike = true;
            ++(*config::connection_base::strikes);
        }
    }

    /// Send a ping
    /**
     * Initiates a ping with the given payload/
     *
     * There is no feedback directly from ping except in cases of immediately
     * detectable errors. Feedback will be provided via on_pong or
     * on_pong_timeout callbacks.
     *
     * Ping locks the m_write_lock mutex
     *
     * @param payload Payload to be used for the ping
     */
    void ping(std::string const & payload);

    /// exception free variant of ping
    void ping(std::string const & payload, lib::error_code & ec);

    /// Utility method that gets called back when the ping timer expires
    void handle_pong_timeout(std::string payload, lib::error_code const & ec);

    /// Send a pong
    /**
     * Initiates a pong with the given payload.
     *
     * There is no feedback from a pong once sent.
     *
     * Pong locks the m_write_lock mutex
     *
     * @param payload Payload to be used for the pong
     */
    void pong(std::string const & payload);

    /// exception free variant of pong
    void pong(std::string const & payload, lib::error_code & ec);

    /// Close the connection
    /**
     * Initiates the close handshake process.
     *
     * If close returns successfully the connection will be in the closing
     * state and no additional messages may be sent. All messages sent prior
     * to calling close will be written out before the connection is closed.
     *
     * If no reason is specified none will be sent. If no code is specified
     * then no code will be sent.
     *
     * The handler's on_close callback will be called once the close handshake
     * is complete.
     *
     * Reasons will be automatically truncated to the maximum length (123 bytes)
     * if necessary.
     *
     * @param code The close code to send
     * @param reason The close reason to send
     */
    void close(close::status::value const code, std::string const & reason);

    /// exception free variant of close
    void close(close::status::value const code, std::string const & reason,
        lib::error_code & ec);

    ////////////////////////////////////////////////
    // Pass-through access to the uri information //
    ////////////////////////////////////////////////

    /// Returns the secure flag from the connection URI
    /**
     * This value is available after the HTTP request has been fully read and
     * may be called from any thread.
     *
     * @return Whether or not the connection URI is flagged secure.
     */
    bool get_secure() const;

    /// Returns the host component of the connection URI
    /**
     * This value is available after the HTTP request has been fully read and
     * may be called from any thread.
     *
     * @return The host component of the connection URI
     */
    std::string const & get_host() const;

    /// Returns the resource component of the connection URI
    /**
     * This value is available after the HTTP request has been fully read and
     * may be called from any thread.
     *
     * @return The resource component of the connection URI
     */
    std::string const & get_resource() const;

    /// Returns the port component of the connection URI
    /**
     * This value is available after the HTTP request has been fully read and
     * may be called from any thread.
     *
     * @return The port component of the connection URI
     */
    uint16_t get_port() const;

    /// Gets the connection URI
    /**
     * This should really only be called by internal library methods unless you
     * really know what you are doing.
     *
     * @return A pointer to the connection's URI
     */
    uri_ptr get_uri() const;

    /// Sets the connection URI
    /**
     * This should really only be called by internal library methods unless you
     * really know what you are doing.
     *
     * @param uri The new URI to set
     */
    void set_uri(uri_ptr uri);

    /////////////////////////////
    // Subprotocol negotiation //
    /////////////////////////////

    /// Gets the negotated subprotocol
    /**
     * Retrieves the subprotocol that was negotiated during the handshake. This
     * method is valid in the open handler and later.
     *
     * @return The negotiated subprotocol
     */
    std::string const & get_subprotocol() const;

    /// Gets all of the subprotocols requested by the client
    /**
     * Retrieves the subprotocols that were requested during the handshake. This
     * method is valid in the validate handler and later.
     *
     * @return A vector of the requested subprotocol
     */
    std::vector<std::string> const & get_requested_subprotocols() const;

    /// Adds the given subprotocol string to the request list (exception free)
    /**
     * Adds a subprotocol to the list to send with the opening handshake. This
     * may be called multiple times to request more than one. If the server
     * supports one of these, it may choose one. If so, it will return it
     * in it's handshake reponse and the value will be available via
     * get_subprotocol(). Subprotocol requests should be added in order of
     * preference.
     *
     * @param request The subprotocol to request
     * @param ec A reference to an error code that will be filled in the case of
     * errors
     */
    void add_subprotocol(std::string const & request, lib::error_code & ec);

    /// Adds the given subprotocol string to the request list
    /**
     * Adds a subprotocol to the list to send with the opening handshake. This
     * may be called multiple times to request more than one. If the server
     * supports one of these, it may choose one. If so, it will return it
     * in it's handshake reponse and the value will be available via
     * get_subprotocol(). Subprotocol requests should be added in order of
     * preference.
     *
     * @param request The subprotocol to request
     */
    void add_subprotocol(std::string const & request);

    /// Select a subprotocol to use (exception free)
    /**
     * Indicates which subprotocol should be used for this connection. Valid
     * only during the validate handler callback. Subprotocol selected must have
     * been requested by the client. Consult get_requested_subprotocols() for a
     * list of valid subprotocols.
     *
     * This member function is valid on server endpoints/connections only
     *
     * @param value The subprotocol to select
     * @param ec A reference to an error code that will be filled in the case of
     * errors
     */
    void select_subprotocol(std::string const & value, lib::error_code & ec);

    /// Select a subprotocol to use
    /**
     * Indicates which subprotocol should be used for this connection. Valid
     * only during the validate handler callback. Subprotocol selected must have
     * been requested by the client. Consult get_requested_subprotocols() for a
     * list of valid subprotocols.
     *
     * This member function is valid on server endpoints/connections only
     *
     * @param value The subprotocol to select
     */
    void select_subprotocol(std::string const & value);

    /////////////////////////////////////////////////////////////
    // Pass-through access to the request and response objects //
    /////////////////////////////////////////////////////////////

    /// Retrieve a request header
    /**
     * Retrieve the value of a header from the handshake HTTP request.
     *
     * @param key Name of the header to get
     * @return The value of the header
     */
    std::string const & get_request_header(std::string const & key) const;

    /// Retrieve a request body
    /**
     * Retrieve the value of the request body. This value is typically used with
     * PUT and POST requests to upload files or other data. Only HTTP
     * connections will ever have bodies. WebSocket connection's will always
     * have blank bodies.
     *
     * @return The value of the request body.
     */
    std::string const & get_request_body() const;

    /// Retrieve a response header
    /**
     * Retrieve the value of a header from the handshake HTTP request.
     *
     * @param key Name of the header to get
     * @return The value of the header
     */
    std::string const & get_response_header(std::string const & key) const;

    /// Get response HTTP status code
    /**
     * Gets the response status code 
     *
     * @since 0.7.0
     *
     * @return The response status code sent
     */
    http::status_code::value get_response_code() const {
        return m_response.get_status_code();
    }

    /// Get response HTTP status message
    /**
     * Gets the response status message 
     *
     * @since 0.7.0
     *
     * @return The response status message sent
     */
    std::string const & get_response_msg() const {
        return m_response.get_status_msg();
    }
    
    /// Set response status code and message
    /**
     * Sets the response status code to `code` and looks up the corresponding
     * message for standard codes. Non-standard codes will be entered as Unknown
     * use set_status(status_code::value,std::string) overload to set both
     * values explicitly.
     *
     * This member function is valid only from the http() and validate() handler
     * callbacks.
     *
     * @param code Code to set
     * @param msg Message to set
     * @see websocketpp::http::response::set_status
     */
    void set_status(http::status_code::value code);

    /// Set response status code and message
    /**
     * Sets the response status code and message to independent custom values.
     * use set_status(status_code::value) to set the code and have the standard
     * message be automatically set.
     *
     * This member function is valid only from the http() and validate() handler
     * callbacks.
     *
     * @param code Code to set
     * @param msg Message to set
     * @see websocketpp::http::response::set_status
     */
    void set_status(http::status_code::value code, std::string const & msg);

    /// Set response body content
    /**
     * Set the body content of the HTTP response to the parameter string. Note
     * set_body will also set the Content-Length HTTP header to the appropriate
     * value. If you want the Content-Length header to be something else set it
     * to something else after calling set_body
     *
     * This member function is valid only from the http() and validate() handler
     * callbacks.
     *
     * @param value String data to include as the body content.
     * @see websocketpp::http::response::set_body
     */
    void set_body(std::string const & value);

    /// Append a header
    /**
     * If a header with this name already exists the value will be appended to
     * the existing header to form a comma separated list of values. Use
     * `connection::replace_header` to overwrite existing values.
     *
     * This member function is valid only from the http() and validate() handler
     * callbacks, or to a client connection before connect has been called.
     *
     * @param key Name of the header to set
     * @param val Value to add
     * @see replace_header
     * @see websocketpp::http::parser::append_header
     */
    void append_header(std::string const & key, std::string const & val);

    /// Replace a header
    /**
     * If a header with this name already exists the old value will be replaced
     * Use `connection::append_header` to append to a list of existing values.
     *
     * This member function is valid only from the http() and validate() handler
     * callbacks, or to a client connection before connect has been called.
     *
     * @param key Name of the header to set
     * @param val Value to set
     * @see append_header
     * @see websocketpp::http::parser::replace_header
     */
    void replace_header(std::string const & key, std::string const & val);

    /// Remove a header
    /**
     * Removes a header from the response.
     *
     * This member function is valid only from the http() and validate() handler
     * callbacks, or to a client connection before connect has been called.
     *
     * @param key The name of the header to remove
     * @see websocketpp::http::parser::remove_header
     */
    void remove_header(std::string const & key);

    /// Get request object
    /**
     * Direct access to request object. This can be used to call methods of the
     * request object that are not part of the standard request API that
     * connection wraps.
     *
     * Note use of this method involves using behavior specific to the
     * configured HTTP policy. Such behavior may not work with alternate HTTP
     * policies.
     *
     * @since 0.3.0-alpha3
     *
     * @return A const reference to the raw request object
     */
    request_type const & get_request() const {
        return m_request;
    }
    
    /// Get response object
    /**
     * Direct access to the HTTP response sent or received as a part of the
     * opening handshake. This can be used to call methods of the response
     * object that are not part of the standard request API that connection
     * wraps.
     *
     * Note use of this method involves using behavior specific to the
     * configured HTTP policy. Such behavior may not work with alternate HTTP
     * policies.
     *
     * @since 0.7.0
     *
     * @return A const reference to the raw response object
     */
    response_type const & get_response() const {
        return m_response;
    }
    
    /// Defer HTTP Response until later (Exception free)
    /**
     * Used in the http handler to defer the HTTP response for this connection
     * until later. Handshake timers will be canceled and the connection will be
     * left open until `send_http_response` or an equivalent is called.
     *
     * Warning: deferred connections won't time out and as a result can tie up
     * resources.
     *
     * @since 0.6.0
     *
     * @return A status code, zero on success, non-zero otherwise
     */
    lib::error_code defer_http_response();
    
    /// Send deferred HTTP Response (exception free)
    /**
     * Sends an http response to an HTTP connection that was deferred. This will
     * send a complete response including all headers, status line, and body
     * text. The connection will be closed afterwards.
     *
     * @since 0.6.0
     *
     * @param ec A status code, zero on success, non-zero otherwise
     */
    void send_http_response(lib::error_code & ec);
    
    /// Send deferred HTTP Response
    void send_http_response();
    
    // TODO HTTPNBIO: write_headers
    // function that processes headers + status so far and writes it to the wire
    // beginning the HTTP response body state. This method will ignore anything
    // in the response body.
    
    // TODO HTTPNBIO: write_body_message
    // queues the specified message_buffer for async writing
    
    // TODO HTTPNBIO: finish connection
    //
    
    // TODO HTTPNBIO: write_response
    // Writes the whole response, headers + body and closes the connection
    
    

    /////////////////////////////////////////////////////////////
    // Pass-through access to the other connection information //
    /////////////////////////////////////////////////////////////

    /// Get Connection Handle
    /**
     * The connection handle is a token that can be shared outside the
     * WebSocket++ core for the purposes of identifying a connection and
     * sending it messages.
     *
     * @return A handle to the connection
     */
    connection_hdl get_handle() const {
        return m_connection_hdl;
    }

    /// Get whether or not this connection is part of a server or client
    /**
     * @return whether or not the connection is attached to a server endpoint
     */
    bool is_server() const {
        return m_is_server;
    }

    /// Return the same origin policy origin value from the opening request.
    /**
     * This value is available after the HTTP request has been fully read and
     * may be called from any thread.
     *
     * @return The connection's origin value from the opening handshake.
     */
    std::string const & get_origin() const;

    /// Return the connection state.
    /**
     * Values can be connecting, open, closing, and closed
     *
     * @return The connection's current state.
     */
    session::state::value get_state() const;


    /// Get the WebSocket close code sent by this endpoint.
    /**
     * @return The WebSocket close code sent by this endpoint.
     */
    close::status::value get_local_close_code() const {
        return m_local_close_code;
    }

    /// Get the WebSocket close reason sent by this endpoint.
    /**
     * @return The WebSocket close reason sent by this endpoint.
     */
    std::string const & get_local_close_reason() const {
        return m_local_close_reason;
    }

    /// Get the WebSocket close code sent by the remote endpoint.
    /**
     * @return The WebSocket close code sent by the remote endpoint.
     */
    close::status::value get_remote_close_code() const {
        return m_remote_close_code;
    }

    /// Get the WebSocket close reason sent by the remote endpoint.
    /**
     * @return The WebSocket close reason sent by the remote endpoint.
     */
    std::string const & get_remote_close_reason() const {
        return m_remote_close_reason;
    }

    /// Get the internal error code for a closed/failed connection
    /**
     * Retrieves a machine readable detailed error code indicating the reason
     * that the connection was closed or failed. Valid only after the close or
     * fail handler is called.
     *
     * @return Error code indicating the reason the connection was closed or
     * failed
     */
    lib::error_code get_ec() const {
        return m_ec;
    }

    /// Get a message buffer
    /**
     * Warning: The API related to directly sending message buffers may change
     * before the 1.0 release. If you plan to use it, please keep an eye on any
     * breaking changes notifications in future release notes. Also if you have
     * any feedback about usage and capabilities now is a great time to provide
     * it.
     *
     * Message buffers are used to store message payloads and other message
     * metadata.
     *
     * The size parameter is a hint only. Your final payload does not need to
     * match it. There may be some performance benefits if the initial size
     * guess is equal to or slightly higher than the final payload size.
     *
     * @param op The opcode for the new message
     * @param size A hint to optimize the initial allocation of payload space.
     * @return A new message buffer
     */
    message_ptr get_message(websocketpp::frame::opcode::value op, size_t size)
        const
    {
        return m_msg_manager->get_message(op, size);
    }

    ////////////////////////////////////////////////////////////////////////
    // The remaining public member functions are for internal/policy use  //
    // only. Do not call from application code unless you understand what //
    // you are doing.                                                     //
    ////////////////////////////////////////////////////////////////////////

    

    void read_handshake(size_t num_bytes);

    void handle_read_handshake(lib::error_code const & ec,
        size_t bytes_transferred);
    void handle_read_http_response(lib::error_code const & ec,
        size_t bytes_transferred);

    
    void handle_write_http_response(lib::error_code const & ec);
    void handle_send_http_request(lib::error_code const & ec);

    void handle_open_handshake_timeout(lib::error_code const & ec);
    void handle_close_handshake_timeout(lib::error_code const & ec);

    void handle_read_frame(lib::error_code const & ec, size_t bytes_transferred);
    void read_frame();

    /// Get array of WebSocket protocol versions that this connection supports.
    std::vector<int> const & get_supported_versions() const;

    /// Sets the handler for a terminating connection. Should only be used
    /// internally by the endpoint class.
    void set_termination_handler(termination_handler new_handler);

    void terminate(lib::error_code const & ec);
    void handle_terminate(terminate_status tstat, lib::error_code const & ec);

    /// Checks if there are frames in the send queue and if there are sends one
    /**
     * \todo unit tests
     *
     * This method locks the m_write_lock mutex
     */
    void write_frame();

    /// Process the results of a frame write operation and start the next write
    /**
     * \todo unit tests
     *
     * This method locks the m_write_lock mutex
     *
     * @param terminate Whether or not to terminate the connection upon
     * completion of this write.
     *
     * @param ec A status code from the transport layer, zero on success,
     * non-zero otherwise.
     */
    void handle_write_frame(lib::error_code const & ec);
// protected:
    // This set of methods would really like to be protected, but doing so 
    // requires that the endpoint be able to friend the connection. This is 
    // allowed with C++11, but not prior versions

    /// Start the connection state machine
    void start();

    void upgrade() {
        this->send_upgrade_request();
    }

    /// Set Connection Handle
    /**
     * The connection handle is a token that can be shared outside the
     * WebSocket++ core for the purposes of identifying a connection and
     * sending it messages.
     *
     * @param hdl A connection_hdl that the connection will use to refer
     * to itself.
     */
    void set_handle(connection_hdl hdl) {
        m_connection_hdl = hdl;
        transport_con_type::set_handle(hdl);
    }
protected:
    void handle_transport_init(lib::error_code const & ec);

    /// Set m_processor based on information in m_request. Set m_response
    /// status and return an error code indicating status.
    lib::error_code initialize_processor();

    /// Perform WebSocket handshake validation of m_request using m_processor.
    /// set m_response and return an error code indicating status.
    lib::error_code process_handshake_request();

    /// Completes m_response, serializes it, and sends it out on the wire.
    void write_http_response(lib::error_code const & ec);

    /// Sends an opening WebSocket connect request
    void send_http_request();

    /// Alternate path for write_http_response in error conditions
    void write_http_response_error(lib::error_code const & ec);

    /// Process control message
    /**
     *
     */
    void process_control_frame(message_ptr msg);

    /// Send close acknowledgement
    /**
     * If no arguments are present no close code/reason will be specified.
     *
     * Note: the close code/reason values provided here may be overrided by
     * other settings (such as silent close).
     *
     * @param code The close code to send
     * @param reason The close reason to send
     * @return A status code, zero on success, non-zero otherwise
     */
    lib::error_code send_close_ack(close::status::value code =
        close::status::blank, std::string const & reason = std::string());

    /// Send close frame
    /**
     * If no arguments are present no close code/reason will be specified.
     *
     * Note: the close code/reason values provided here may be overrided by
     * other settings (such as silent close).
     *
     * The ack flag determines what to do in the case of a blank status and
     * whether or not to terminate the TCP connection after sending it.
     *
     * @param code The close code to send
     * @param reason The close reason to send
     * @param ack Whether or not this is an acknowledgement close frame
     * @return A status code, zero on success, non-zero otherwise
     */
    lib::error_code send_close_frame(close::status::value code =
        close::status::blank, std::string const & reason = std::string(), bool ack = false,
        bool terminal = false);

    /// Get a pointer to a new WebSocket protocol processor for a given version
    /**
     * @param version Version number of the WebSocket protocol to get a
     * processor for. Negative values indicate invalid/unknown versions and will
     * always return a null ptr
     *
     * @return A shared_ptr to a new instance of the appropriate processor or a
     * null ptr if there is no installed processor that matches the version
     * number.
     */
    processor_ptr get_processor(int version) const;

    /// Add a message to the write queue
    /**
     * Adds a message to the write queue and updates any associated shared state
     *
     * Must be called while holding m_write_lock
     *
     * @todo unit tests
     *
     * @param msg The message to push
     */
    void write_push(message_ptr msg);

    /// Pop a message from the write queue
    /**
     * Removes and returns a message from the write queue and updates any
     * associated shared state.
     *
     * Must be called while holding m_write_lock
     *
     * @todo unit tests
     *
     * @return the message_ptr at the front of the queue
     */
    message_ptr write_pop();

    /// Prints information about the incoming connection to the access log
    /**
     * Prints information about the incoming connection to the access log.
     * Includes: connection type, websocket version, remote endpoint, user agent
     * path, status code.
     */
    void log_open_result();

    /// Prints information about a connection being closed to the access log
    /**
     * Includes: local and remote close codes and reasons
     */
    void log_close_result();

    /// Prints information about a connection being failed to the access log
    /**
     * Includes: error code and message for why it was failed
     */
    void log_fail_result();
    
    /// Prints information about HTTP connections
    /**
     * Includes: TODO
     */
    void log_http_result();

    /// Prints information about an arbitrary error code on the specified channel
    template <typename error_type>
    void log_err(log::level l, char const * msg, error_type const & ec) {
        std::stringstream s;
        s << msg << " error: " << ec << " (" << ec.message() << ")";
        m_elog->write(l, s.str());
    }

    // internal handler functions
    read_handler            m_handle_read_frame;
    write_frame_handler     m_write_frame_handler;

    // static settings
    std::string const       m_user_agent;

    /// Pointer to the connection handle
    connection_hdl          m_connection_hdl;

    /// Handler objects
    open_handler            m_open_handler;
    close_handler           m_close_handler;
    fail_handler            m_fail_handler;
    ping_handler            m_ping_handler;
    pong_handler            m_pong_handler;
    pong_timeout_handler    m_pong_timeout_handler;
    interrupt_handler       m_interrupt_handler;
    http_handler            m_http_handler;
    validate_handler        m_validate_handler;
    message_handler         m_message_handler;

    /// constant values
    long                    m_open_handshake_timeout_dur;
    long                    m_close_handshake_timeout_dur;
    long                    m_pong_timeout_dur;
    size_t                  m_max_message_size;

    /// External connection state
    /**
     * Lock: m_connection_state_lock
     */
    session::state::value   m_state;

    /// Internal connection state
    /**
     * Lock: m_connection_state_lock
     */
    istate_type             m_internal_state;

    mutable mutex_type      m_connection_state_lock;

    /// The lock used to protect the message queue
    /**
     * Serializes access to the write queue as well as shared state within the
     * processor.
     */
    mutex_type              m_write_lock;

    // connection resources
    char                    m_buf[config::connection_read_buffer_size];
    size_t                  m_buf_cursor;
    termination_handler     m_termination_handler;
    con_msg_manager_ptr     m_msg_manager;
    timer_ptr               m_handshake_timer;
    timer_ptr               m_ping_timer;

    /// @todo this is not memory efficient. this value is not used after the
    /// handshake.
    std::string m_handshake_buffer;

    /// Pointer to the processor object for this connection
    /**
     * The processor provides functionality that is specific to the WebSocket
     * protocol version that the client has negotiated. It also contains all of
     * the state necessary to encode and decode the incoming and outgoing
     * WebSocket byte streams
     *
     * Use of the prepare_data_frame method requires lock: m_write_lock
     */
    processor_ptr           m_processor;

    /// Queue of unsent outgoing messages
    /**
     * Lock: m_write_lock
     */
    std::queue<message_ptr> m_send_queue;

    /// Size in bytes of the outstanding payloads in the write queue
    /**
     * Lock: m_write_lock
     */
    size_t m_send_buffer_size;

    /// buffer holding the various parts of the current message being writen
    /**
     * Lock m_write_lock
     */
    std::vector<transport::buffer> m_send_buffer;

    /// a list of pointers to hold on to the messages being written to keep them
    /// from going out of scope before the write is complete.
    std::vector<message_ptr> m_current_msgs;

    /// True if there is currently an outstanding transport write
    /**
     * Lock m_write_lock
     */
    bool m_write_flag;

    /// True if this connection is presently reading new data
    bool m_read_flag;

    // connection data
    request_type            m_request;
    response_type           m_response;
    uri_ptr                 m_uri;
    std::string             m_subprotocol;

    // connection data that might not be necessary to keep around for the life
    // of the whole connection.
    std::vector<std::string> m_requested_subprotocols;

    bool const              m_is_server;
    const lib::shared_ptr<alog_type> m_alog;
    const lib::shared_ptr<elog_type> m_elog;

    rng_type & m_rng;

    // Close state
    /// Close code that was sent on the wire by this endpoint
    close::status::value    m_local_close_code;

    /// Close reason that was sent on the wire by this endpoint
    std::string             m_local_close_reason;

    /// Close code that was received on the wire from the remote endpoint
    close::status::value    m_remote_close_code;

    /// Close reason that was received on the wire from the remote endpoint
    std::string             m_remote_close_reason;

    /// Detailed internal error code
    lib::error_code m_ec;
    
    /// A flag that gets set once it is determined that the connection is an
    /// HTTP connection and not a WebSocket one.
    bool m_is_http;
    
    /// A flag that gets set when the completion of an http connection is
    /// deferred until later.
    session::http_state::value m_http_state;

    bool m_was_clean;
};


#include <algorithm>
#include <exception>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace istate = session::internal_state;

template <typename config>
void my_connection<config>::set_termination_handler(
    termination_handler new_handler)
{
    m_alog->write(log::alevel::devel,
        "connection set_termination_handler");

    //scoped_lock_type lock(m_connection_state_lock);

    m_termination_handler = new_handler;
}

template <typename config>
std::string const & my_connection<config>::get_origin() const {
    //scoped_lock_type lock(m_connection_state_lock);
    return m_processor->get_origin(m_request);
}

template <typename config>
size_t my_connection<config>::get_buffered_amount() const {
    //scoped_lock_type lock(m_connection_state_lock);
    return m_send_buffer_size;
}

template <typename config>
session::state::value my_connection<config>::get_state() const {
    //scoped_lock_type lock(m_connection_state_lock);
    return m_state;
}

template <typename config>
lib::error_code my_connection<config>::send(std::string const & payload,
    frame::opcode::value op)
{
    message_ptr msg = m_msg_manager->get_message(op,payload.size());
    msg->append_payload(payload);
    msg->set_compressed(true);

    return send(msg);
}

template <typename config>
lib::error_code my_connection<config>::send(void const * payload, size_t len,
    frame::opcode::value op)
{
    message_ptr msg = m_msg_manager->get_message(op,len);
    msg->append_payload(payload,len);

    return send(msg);
}

template <typename config>
lib::error_code my_connection<config>::send(typename config::message_type::ptr msg)
{
    if (m_alog->static_test(log::alevel::devel)) {
        m_alog->write(log::alevel::devel,"connection send");
    }

    {
        scoped_lock_type lock(m_connection_state_lock);
        if (m_state != session::state::open) {
           return error::make_error_code(error::invalid_state);
        }
    }

    message_ptr outgoing_msg;
    bool needs_writing = false;

    if (msg->get_prepared()) {
        outgoing_msg = msg;

        scoped_lock_type lock(m_write_lock);
        write_push(outgoing_msg);
        needs_writing = !m_write_flag && !m_send_queue.empty();
    } else {
        outgoing_msg = m_msg_manager->get_message();

        if (!outgoing_msg) {
            return error::make_error_code(error::no_outgoing_buffers);
        }

        scoped_lock_type lock(m_write_lock);
        lib::error_code ec = m_processor->prepare_data_frame(msg,outgoing_msg);

        if (ec) {
            return ec;
        }

        write_push(outgoing_msg);
        needs_writing = !m_write_flag && !m_send_queue.empty();
    }

    if (needs_writing) {
        transport_con_type::dispatch(lib::bind(
            &type::write_frame,
            type::get_shared()
        ));
    }

    return lib::error_code();
}

template <typename config>
void my_connection<config>::ping(std::string const& payload, lib::error_code& ec) {
    if (m_alog->static_test(log::alevel::devel)) {
        m_alog->write(log::alevel::devel,"connection ping");
    }

    {
        scoped_lock_type lock(m_connection_state_lock);
        if (m_state != session::state::open) {
            std::stringstream ss;
            ss << "connection::ping called from invalid state " << m_state;
            m_alog->write(log::alevel::devel,ss.str());
            ec = error::make_error_code(error::invalid_state);
            return;
        }
    }

    message_ptr msg = m_msg_manager->get_message();
    if (!msg) {
        ec = error::make_error_code(error::no_outgoing_buffers);
        return;
    }

    ec = m_processor->prepare_ping(payload,msg);
    if (ec) {return;}

    // set ping timer if we are listening for one
    if (m_pong_timeout_handler) {
        // Cancel any existing timers
        if (m_ping_timer) {
            m_ping_timer->cancel();
        }

        if (m_pong_timeout_dur > 0) {
            m_ping_timer = transport_con_type::set_timer(
                m_pong_timeout_dur,
                lib::bind(
                    &type::handle_pong_timeout,
                    type::get_shared(),
                    payload,
                    lib::placeholders::_1
                )
            );
        }

        if (!m_ping_timer) {
            // Our transport doesn't support timers
            m_elog->write(log::elevel::warn,"Warning: a pong_timeout_handler is \
                set but the transport in use does not support timeouts.");
        }
    }

    bool needs_writing = false;
    {
        scoped_lock_type lock(m_write_lock);
        write_push(msg);
        needs_writing = !m_write_flag && !m_send_queue.empty();
    }

    if (needs_writing) {
        transport_con_type::dispatch(lib::bind(
            &type::write_frame,
            type::get_shared()
        ));
    }

    ec = lib::error_code();
}

template<typename config>
void my_connection<config>::ping(std::string const & payload) {
    lib::error_code ec;
    ping(payload,ec);
    if (ec) {
        throw exception(ec);
    }
}

template<typename config>
void my_connection<config>::handle_pong_timeout(std::string payload,
    lib::error_code const & ec)
{
    if (ec) {
        if (ec == transport::error::operation_aborted) {
            // ignore, this is expected
            return;
        }

        m_elog->write(log::elevel::devel,"pong_timeout error: "+ec.message());
        return;
    }

    if (m_pong_timeout_handler) {
        m_pong_timeout_handler(m_connection_hdl,payload);
    }
}

template <typename config>
void my_connection<config>::pong(std::string const& payload, lib::error_code& ec) {
    if (m_alog->static_test(log::alevel::devel)) {
        m_alog->write(log::alevel::devel,"connection pong");
    }

    {
        scoped_lock_type lock(m_connection_state_lock);
        if (m_state != session::state::open) {
            std::stringstream ss;
            ss << "connection::pong called from invalid state " << m_state;
            m_alog->write(log::alevel::devel,ss.str());
            ec = error::make_error_code(error::invalid_state);
            return;
        }
    }

    message_ptr msg = m_msg_manager->get_message();
    if (!msg) {
        ec = error::make_error_code(error::no_outgoing_buffers);
        return;
    }

    ec = m_processor->prepare_pong(payload,msg);
    if (ec) {return;}

    bool needs_writing = false;
    {
        scoped_lock_type lock(m_write_lock);
        write_push(msg);
        needs_writing = !m_write_flag && !m_send_queue.empty();
    }

    if (needs_writing) {
        transport_con_type::dispatch(lib::bind(
            &type::write_frame,
            type::get_shared()
        ));
    }

    ec = lib::error_code();
}

template<typename config>
void my_connection<config>::pong(std::string const & payload) {
    lib::error_code ec;
    pong(payload,ec);
    if (ec) {
        throw exception(ec);
    }
}

template <typename config>
void my_connection<config>::close(close::status::value const code,
    std::string const & reason, lib::error_code & ec)
{
    if (m_alog->static_test(log::alevel::devel)) {
        m_alog->write(log::alevel::devel,"connection close");
    }

    // Truncate reason to maximum size allowable in a close frame.
    std::string tr(reason,0,std::min<size_t>(reason.size(),
        frame::limits::close_reason_size));

    scoped_lock_type lock(m_connection_state_lock);

    if (m_state != session::state::open) {
       ec = error::make_error_code(error::invalid_state);
       return;
    }

    ec = this->send_close_frame(code,tr,false,close::status::terminal(code));
}

template<typename config>
void my_connection<config>::close(close::status::value const code,
    std::string const & reason)
{
    lib::error_code ec;
    close(code,reason,ec);
    if (ec) {
        throw exception(ec);
    }
}

/// Trigger the on_interrupt handler
/**
 * This is thread safe if the transport is thread safe
 */
template <typename config>
lib::error_code my_connection<config>::interrupt() {
    m_alog->write(log::alevel::devel,"connection connection::interrupt");
    return transport_con_type::interrupt(
        lib::bind(
            &type::handle_interrupt,
            type::get_shared()
        )
    );
}


template <typename config>
void my_connection<config>::handle_interrupt() {
    if (m_interrupt_handler) {
        m_interrupt_handler(m_connection_hdl);
    }
}

template <typename config>
lib::error_code my_connection<config>::pause_reading() {
    m_alog->write(log::alevel::devel,"connection connection::pause_reading");
    return transport_con_type::dispatch(
        lib::bind(
            &type::handle_pause_reading,
            type::get_shared()
        )
    );
}

/// Pause reading handler. Not safe to call directly
template <typename config>
void my_connection<config>::handle_pause_reading() {
    m_alog->write(log::alevel::devel,"connection connection::handle_pause_reading");
    m_read_flag = false;
}

template <typename config>
lib::error_code my_connection<config>::resume_reading() {
    m_alog->write(log::alevel::devel,"connection connection::resume_reading");
    return transport_con_type::dispatch(
        lib::bind(
            &type::handle_resume_reading,
            type::get_shared()
        )
    );
}

/// Resume reading helper method. Not safe to call directly
template <typename config>
void my_connection<config>::handle_resume_reading() {
   m_read_flag = true;
   read_frame();
}


template <typename config>
bool my_connection<config>::get_secure() const {
    //scoped_lock_type lock(m_connection_state_lock);
    return m_uri->get_secure();
}

template <typename config>
std::string const & my_connection<config>::get_host() const {
    //scoped_lock_type lock(m_connection_state_lock);
    return m_uri->get_host();
}

template <typename config>
std::string const & my_connection<config>::get_resource() const {
    //scoped_lock_type lock(m_connection_state_lock);
    return m_uri->get_resource();
}

template <typename config>
uint16_t my_connection<config>::get_port() const {
    //scoped_lock_type lock(m_connection_state_lock);
    return m_uri->get_port();
}

template <typename config>
uri_ptr my_connection<config>::get_uri() const {
    //scoped_lock_type lock(m_connection_state_lock);
    return m_uri;
}

template <typename config>
void my_connection<config>::set_uri(uri_ptr uri) {
    //scoped_lock_type lock(m_connection_state_lock);
    m_uri = uri;
}






template <typename config>
std::string const & my_connection<config>::get_subprotocol() const {
    return m_subprotocol;
}

template <typename config>
std::vector<std::string> const &
my_connection<config>::get_requested_subprotocols() const {
    return m_requested_subprotocols;
}

template <typename config>
void my_connection<config>::add_subprotocol(std::string const & value,
    lib::error_code & ec)
{
    if (m_is_server) {
        ec = error::make_error_code(error::client_only);
        return;
    }

    // If the value is empty or has a non-RFC2616 token character it is invalid.
    if (value.empty() || std::find_if(value.begin(),value.end(),
                                      http::is_not_token_char) != value.end())
    {
        ec = error::make_error_code(error::invalid_subprotocol);
        return;
    }

    m_requested_subprotocols.push_back(value);
}

template <typename config>
void my_connection<config>::add_subprotocol(std::string const & value) {
    lib::error_code ec;
    this->add_subprotocol(value,ec);
    if (ec) {
        throw exception(ec);
    }
}


template <typename config>
void my_connection<config>::select_subprotocol(std::string const & value,
    lib::error_code & ec)
{
    if (!m_is_server) {
        ec = error::make_error_code(error::server_only);
        return;
    }

    if (value.empty()) {
        ec = lib::error_code();
        return;
    }

    std::vector<std::string>::iterator it;

    it = std::find(m_requested_subprotocols.begin(),
                   m_requested_subprotocols.end(),
                   value);

    if (it == m_requested_subprotocols.end()) {
        ec = error::make_error_code(error::unrequested_subprotocol);
        return;
    }

    m_subprotocol = value;
}

template <typename config>
void my_connection<config>::select_subprotocol(std::string const & value) {
    lib::error_code ec;
    this->select_subprotocol(value,ec);
    if (ec) {
        throw exception(ec);
    }
}


template <typename config>
std::string const &
my_connection<config>::get_request_header(std::string const & key) const {
    return m_request.get_header(key);
}

template <typename config>
std::string const &
my_connection<config>::get_request_body() const {
    return m_request.get_body();
}

template <typename config>
std::string const &
my_connection<config>::get_response_header(std::string const & key) const {
    return m_response.get_header(key);
}

// TODO: EXCEPTION_FREE
template <typename config>
void my_connection<config>::set_status(http::status_code::value code)
{
    if (m_internal_state != istate::PROCESS_HTTP_REQUEST) {
        throw exception("Call to set_status from invalid state",
                      error::make_error_code(error::invalid_state));
    }
    m_response.set_status(code);
}

// TODO: EXCEPTION_FREE
template <typename config>
void my_connection<config>::set_status(http::status_code::value code,
    std::string const & msg)
{
    if (m_internal_state != istate::PROCESS_HTTP_REQUEST) {
        throw exception("Call to set_status from invalid state",
                      error::make_error_code(error::invalid_state));
    }

    m_response.set_status(code,msg);
}

// TODO: EXCEPTION_FREE
template <typename config>
void my_connection<config>::set_body(std::string const & value) {
    if (m_internal_state != istate::PROCESS_HTTP_REQUEST) {
        throw exception("Call to set_status from invalid state",
                      error::make_error_code(error::invalid_state));
    }

    m_response.set_body(value);
}

// TODO: EXCEPTION_FREE
template <typename config>
void my_connection<config>::append_header(std::string const & key,
    std::string const & val)
{
    if (m_is_server) {
        if (m_internal_state == istate::PROCESS_HTTP_REQUEST) {
            // we are setting response headers for an incoming server connection
            m_response.append_header(key,val);
        } else {
            throw exception("Call to append_header from invalid state",
                      error::make_error_code(error::invalid_state));
        }
    } else {
        if (m_internal_state == istate::USER_INIT) {
            // we are setting initial headers for an outgoing client connection
            m_request.append_header(key,val);
        } else {
            throw exception("Call to append_header from invalid state",
                      error::make_error_code(error::invalid_state));
        }
    }
}

// TODO: EXCEPTION_FREE
template <typename config>
void my_connection<config>::replace_header(std::string const & key,
    std::string const & val)
{
    if (m_is_server) {
        if (m_internal_state == istate::PROCESS_HTTP_REQUEST) {
            // we are setting response headers for an incoming server connection
            m_response.replace_header(key,val);
        } else {
            throw exception("Call to replace_header from invalid state",
                        error::make_error_code(error::invalid_state));
        }
    } else {
        if (m_internal_state == istate::USER_INIT) {
            // we are setting initial headers for an outgoing client connection
            m_request.replace_header(key,val);
        } else {
            throw exception("Call to replace_header from invalid state",
                        error::make_error_code(error::invalid_state));
        }
    }
}

// TODO: EXCEPTION_FREE
template <typename config>
void my_connection<config>::remove_header(std::string const & key)
{
    if (m_is_server) {
        if (m_internal_state == istate::PROCESS_HTTP_REQUEST) {
            // we are setting response headers for an incoming server connection
            m_response.remove_header(key);
        } else {
            throw exception("Call to remove_header from invalid state",
                        error::make_error_code(error::invalid_state));
        }
    } else {
        if (m_internal_state == istate::USER_INIT) {
            // we are setting initial headers for an outgoing client connection
            m_request.remove_header(key);
        } else {
            throw exception("Call to remove_header from invalid state",
                        error::make_error_code(error::invalid_state));
        }
    }
}

/// Defer HTTP Response until later
/**
 * Used in the http handler to defer the HTTP response for this connection
 * until later. Handshake timers will be canceled and the connection will be
 * left open until `send_http_response` or an equivalent is called.
 *
 * Warning: deferred connections won't time out and as a result can tie up
 * resources.
 *
 * @return A status code, zero on success, non-zero otherwise
 */
template <typename config>
lib::error_code my_connection<config>::defer_http_response() {
    // Cancel handshake timer, otherwise the connection will time out and we'll
    // close the connection before the app has a chance to send a response.
    if (m_handshake_timer) {
        m_handshake_timer->cancel();
        m_handshake_timer.reset();
    }
    
    // Do something to signal deferral
    m_http_state = session::http_state::deferred;
    
    return lib::error_code();
}

/// Send deferred HTTP Response (exception free)
/**
 * Sends an http response to an HTTP connection that was deferred. This will
 * send a complete response including all headers, status line, and body
 * text. The connection will be closed afterwards.
 *
 * @since 0.6.0
 *
 * @param ec A status code, zero on success, non-zero otherwise
 */
template <typename config>
void my_connection<config>::send_http_response(lib::error_code & ec) {
    {
        scoped_lock_type lock(m_connection_state_lock);
        if (m_http_state != session::http_state::deferred) {
            ec = error::make_error_code(error::invalid_state);
            return;
        }
    
        m_http_state = session::http_state::body_written;
    }
    
    this->write_http_response(lib::error_code());
    ec = lib::error_code();
}

template <typename config>
void my_connection<config>::send_http_response() {
    lib::error_code ec;
    this->send_http_response(ec);
    if (ec) {
        throw exception(ec);
    }
}




/******** logic thread ********/

template <typename config>
void my_connection<config>::start() {
    m_alog->write(log::alevel::devel,"connection start");

    if (m_internal_state != istate::USER_INIT) {
        m_alog->write(log::alevel::devel,"Start called in invalid state");
        this->terminate(error::make_error_code(error::invalid_state));
        return;
    }

    m_internal_state = istate::TRANSPORT_INIT;

    // Depending on how the transport implements init this function may return
    // immediately and call handle_transport_init later or call
    // handle_transport_init from this function.
    transport_con_type::init(
        lib::bind(
            &type::handle_transport_init,
            type::get_shared(),
            lib::placeholders::_1
        )
    );
}

template <typename config>
void my_connection<config>::handle_transport_init(lib::error_code const & ec) {
    m_alog->write(log::alevel::devel,"connection handle_transport_init");

    lib::error_code ecm = ec;

    if (m_internal_state != istate::TRANSPORT_INIT) {
        m_alog->write(log::alevel::devel,
          "handle_transport_init must be called from transport init state");
        ecm = error::make_error_code(error::invalid_state);
    }

    if (ecm) {
        std::stringstream s;
        s << "handle_transport_init received error: "<< ecm.message();
        m_elog->write(log::elevel::rerror,s.str());

        this->terminate(ecm);
        return;
    }

    // At this point the transport is ready to read and write bytes.
    if (m_is_server) {
        m_internal_state = istate::READ_HTTP_REQUEST;
        this->read_handshake(1);
    } else {
        // We are a client. Set the processor to the version specified in the
        // config file and send a handshake request.
        m_internal_state = istate::WRITE_HTTP_REQUEST;
        m_processor = get_processor(config::client_version);
        this->send_http_request();
    }
}

template <typename config>
void my_connection<config>::read_handshake(size_t num_bytes) {
    m_alog->write(log::alevel::devel,"connection read_handshake");

    if (m_open_handshake_timeout_dur > 0) {
        m_handshake_timer = transport_con_type::set_timer(
            m_open_handshake_timeout_dur,
            lib::bind(
                &type::handle_open_handshake_timeout,
                type::get_shared(),
                lib::placeholders::_1
            )
        );
    }

    transport_con_type::async_read_at_least(
        num_bytes,
        m_buf,
        config::connection_read_buffer_size,
        lib::bind(
            &type::handle_read_handshake,
            type::get_shared(),
            lib::placeholders::_1,
            lib::placeholders::_2
        )
    );
}

// All exit paths for this function need to call write_http_response() or submit
// a new read request with this function as the handler.
template <typename config>
void my_connection<config>::handle_read_handshake(lib::error_code const & ec,
    size_t bytes_transferred)
{
    m_alog->write(log::alevel::devel,"connection handle_read_handshake");

    lib::error_code ecm = ec;

    if (!ecm) {
        scoped_lock_type lock(m_connection_state_lock);
        
        if (m_state == session::state::connecting) {
             if (m_internal_state != istate::READ_HTTP_REQUEST) {
                ecm = error::make_error_code(error::invalid_state);
             }
        } else if (m_state == session::state::closed) {
            // The connection was canceled while the response was being sent,
            // usually by the handshake timer. This is basically expected
            // (though hopefully rare) and there is nothing we can do so ignore.
            m_alog->write(log::alevel::devel,
                "handle_read_handshake invoked after connection was closed");
            return;
        } else {
            ecm = error::make_error_code(error::invalid_state);
        }
    }

    if (ecm) {
        if (ecm == transport::error::eof && m_state == session::state::closed) {
            // we expect to get eof if the connection is closed already
            m_alog->write(log::alevel::devel,
                    "got (expected) eof/state error from closed con");
            return;
        }
        
        log_err(log::elevel::rerror,"handle_read_handshake",ecm);
        this->terminate(ecm);
        return;
    }

    // Boundaries checking. TODO: How much of this should be done?
    if (bytes_transferred > config::connection_read_buffer_size) {
        m_elog->write(log::elevel::fatal,"Fatal boundaries checking error.");
        this->terminate(make_error_code(error::general));
        return;
    }

    size_t bytes_processed = 0;
    try {
        bytes_processed = m_request.consume(m_buf,bytes_transferred);
    } catch (http::exception &e) {
        // All HTTP exceptions will result in this request failing and an error
        // response being returned. No more bytes will be read in this con.
        m_response.set_status(e.m_error_code,e.m_error_msg);
        this->write_http_response_error(error::make_error_code(error::http_parse_error));
        return;
    }

    // More paranoid boundaries checking.
    // TODO: Is this overkill?
    if (bytes_processed > bytes_transferred) {
        m_elog->write(log::elevel::fatal,"Fatal boundaries checking error.");
        this->terminate(make_error_code(error::general));
        return;
    }

    if (m_alog->static_test(log::alevel::devel)) {
        std::stringstream s;
        s << "bytes_transferred: " << bytes_transferred
          << " bytes, bytes processed: " << bytes_processed << " bytes";
        m_alog->write(log::alevel::devel,s.str());
    }

    if (m_request.ready()) {
        lib::error_code processor_ec = this->initialize_processor();
        if (processor_ec) {
            this->write_http_response_error(processor_ec);
            return;
        }

        if (m_processor && m_processor->get_version() == 0) {
            // Version 00 has an extra requirement to read some bytes after the
            // handshake
            if (bytes_transferred-bytes_processed >= 8) {
                m_request.replace_header(
                    "Sec-WebSocket-Key3",
                    std::string(m_buf+bytes_processed,m_buf+bytes_processed+8)
                );
                bytes_processed += 8;
            } else {
                // TODO: need more bytes
                m_alog->write(log::alevel::devel,"short key3 read");
                m_response.set_status(http::status_code::internal_server_error);
                this->write_http_response_error(processor::error::make_error_code(processor::error::short_key3));
                return;
            }
        }

        if (m_alog->static_test(log::alevel::devel)) {
            m_alog->write(log::alevel::devel,m_request.raw());
            if (!m_request.get_header("Sec-WebSocket-Key3").empty()) {
                m_alog->write(log::alevel::devel,
                    utility::to_hex(m_request.get_header("Sec-WebSocket-Key3")));
            }
        }

        // The remaining bytes in m_buf are frame data. Copy them to the
        // beginning of the buffer and note the length. They will be read after
        // the handshake completes and before more bytes are read.
        std::copy(m_buf+bytes_processed,m_buf+bytes_transferred,m_buf);
        m_buf_cursor = bytes_transferred-bytes_processed;


        m_internal_state = istate::PROCESS_HTTP_REQUEST;
        
        // We have the complete request. Process it.
        lib::error_code handshake_ec = this->process_handshake_request();
        
        // Write a response if this is a websocket connection or if it is an
        // HTTP connection for which the response has not been deferred or
        // started yet by a different system (i.e. still in init state).
        if (!m_is_http || m_http_state == session::http_state::init) {
            this->write_http_response(handshake_ec);
        }
    } else {
        // read at least 1 more byte
        transport_con_type::async_read_at_least(
            1,
            m_buf,
            config::connection_read_buffer_size,
            lib::bind(
                &type::handle_read_handshake,
                type::get_shared(),
                lib::placeholders::_1,
                lib::placeholders::_2
            )
        );
    }
}

// write_http_response requires the request to be fully read and the connection
// to be in the PROCESS_HTTP_REQUEST state. In some cases we can detect errors
// before the request is fully read (specifically at a point where we aren't
// sure if the hybi00 key3 bytes need to be read). This method sets the correct
// state and calls write_http_response
template <typename config>
void my_connection<config>::write_http_response_error(lib::error_code const & ec) {
    if (m_internal_state != istate::READ_HTTP_REQUEST) {
        m_alog->write(log::alevel::devel,
            "write_http_response_error called in invalid state");
        this->terminate(error::make_error_code(error::invalid_state));
        return;
    }
    
    m_internal_state = istate::PROCESS_HTTP_REQUEST;
    
    this->write_http_response(ec);
}

// All exit paths for this function need to call write_http_response() or submit
// a new read request with this function as the handler.
template <typename config>
void my_connection<config>::handle_read_frame(lib::error_code const & ec,
    size_t bytes_transferred)
{
    //m_alog->write(log::alevel::devel,"connection handle_read_frame");

    lib::error_code ecm = ec;

    if (!ecm && m_internal_state != istate::PROCESS_CONNECTION) {
        ecm = error::make_error_code(error::invalid_state);
    }

    if (ecm) {
        log::level echannel = log::elevel::rerror;
        
        if (ecm == transport::error::eof) {
            if (m_state == session::state::closed) {
                // we expect to get eof if the connection is closed already
                // just ignore it
                m_alog->write(log::alevel::devel,"got eof from closed con");
                return;
            } else if (m_state == session::state::closing && !m_is_server) {
                // If we are a client we expect to get eof in the closing state,
                // this is a signal to terminate our end of the connection after
                // the closing handshake
                terminate(lib::error_code());
                return;
            }
        } else if (ecm == error::invalid_state) {
            // In general, invalid state errors in the closed state are the
            // result of handlers that were in the system already when the state
            // changed and should be ignored as they pose no problems and there
            // is nothing useful that we can do about them.
            if (m_state == session::state::closed) {
                m_alog->write(log::alevel::devel,
                    "handle_read_frame: got invalid istate in closed state");
                return;
            }
        } else if (ecm == transport::error::action_after_shutdown) {
            echannel = log::elevel::info;
        } else {
            // TODO: more generally should we do something different here in the
            // case that m_state is cosed? Are errors after the connection is
            // already closed really an rerror?
        }
        
        
        
        log_err(echannel, "handle_read_frame", ecm);
        this->terminate(ecm);
        return;
    }

    // Boundaries checking. TODO: How much of this should be done?
    /*if (bytes_transferred > config::connection_read_buffer_size) {
        m_elog->write(log::elevel::fatal,"Fatal boundaries checking error");
        this->terminate(make_error_code(error::general));
        return;
    }*/

    size_t p = 0;

    if (m_alog->static_test(log::alevel::devel)) {
        std::stringstream s;
        s << "p = " << p << " bytes transferred = " << bytes_transferred;
        m_alog->write(log::alevel::devel,s.str());
    }

    while (p < bytes_transferred) {
        if (m_alog->static_test(log::alevel::devel)) {
            std::stringstream s;
            s << "calling consume with " << bytes_transferred-p << " bytes";
            m_alog->write(log::alevel::devel,s.str());
        }

        lib::error_code consume_ec;

        if (m_alog->static_test(log::alevel::devel)) {
            std::stringstream s;
            s << "Processing Bytes: " << utility::to_hex(reinterpret_cast<uint8_t*>(m_buf)+p,bytes_transferred-p);
            m_alog->write(log::alevel::devel,s.str());
        }

        p += m_processor->consume(
            reinterpret_cast<uint8_t*>(m_buf)+p,
            bytes_transferred-p,
            consume_ec
        );

        if (m_alog->static_test(log::alevel::devel)) {
            std::stringstream s;
            s << "bytes left after consume: " << bytes_transferred-p;
            m_alog->write(log::alevel::devel,s.str());
        }
        if (consume_ec) {
            log_err(log::elevel::rerror, "consume", consume_ec);

            if (config::drop_on_protocol_error) {
                this->terminate(consume_ec);
                return;
            } else {
                lib::error_code close_ec;
                this->close(
                    processor::error::to_ws(consume_ec),
                    consume_ec.message(),
                    close_ec
                );

                if (close_ec) {
                    log_err(log::elevel::fatal, "Protocol error close frame ", close_ec);
                    this->terminate(close_ec);
                    return;
                }
            }
            return;
        }

        if (m_processor->ready()) {
            if (m_alog->static_test(log::alevel::devel)) {
                std::stringstream s;
                s << "Complete message received. Dispatching";
                m_alog->write(log::alevel::devel,s.str());
            }

            message_ptr msg = m_processor->get_message();

            if (!msg) {
                m_alog->write(log::alevel::devel, "null message from m_processor");
            } else if (!is_control(msg->get_opcode())) {
                // data message, dispatch to user
                if (m_state != session::state::open) {
                    m_elog->write(log::elevel::warn, "got non-close frame while closing");
                } else if (m_message_handler) {
                    m_message_handler(m_connection_hdl, msg);
                }
            } else {
                process_control_frame(msg);
            }
        }
    }

    read_frame();
}

/// Issue a new transport read unless reading is paused.
template <typename config>
void my_connection<config>::read_frame() {
    if (!m_read_flag) {
        return;
    }
    
    transport_con_type::async_read_at_least(
        // std::min wont work with undefined static const values.
        // TODO: is there a more elegant way to do this?
        // Need to determine if requesting 1 byte or the exact number of bytes
        // is better here. 1 byte lets us be a bit more responsive at a
        // potential expense of additional runs through handle_read_frame
        /*(m_processor->get_bytes_needed() > config::connection_read_buffer_size ?
         config::connection_read_buffer_size : m_processor->get_bytes_needed())*/
        1,
        m_buf,
        config::connection_read_buffer_size,
        m_handle_read_frame
    );
}

template <typename config>
lib::error_code my_connection<config>::initialize_processor() {
    m_alog->write(log::alevel::devel,"initialize_processor");

    // if it isn't a websocket handshake nothing to do.
    if (!processor::is_websocket_handshake(m_request)) {
        return lib::error_code();
    }

    int version = processor::get_websocket_version(m_request);

    if (version < 0) {
        m_alog->write(log::alevel::devel, "BAD REQUEST: can't determine version");
        m_response.set_status(http::status_code::bad_request);
        return error::make_error_code(error::invalid_version);
    }

    m_processor = get_processor(version);

    // if the processor is not null we are done
    if (m_processor) {
        return lib::error_code();
    }

    // We don't have a processor for this version. Return bad request
    // with Sec-WebSocket-Version header filled with values we do accept
    m_alog->write(log::alevel::devel, "BAD REQUEST: no processor for version");
    m_response.set_status(http::status_code::bad_request);

    std::stringstream ss;
    std::string sep;
    std::vector<int>::const_iterator it;
    for (it = versions_supported.begin(); it != versions_supported.end(); it++)
    {
        ss << sep << *it;
        sep = ",";
    }

    m_response.replace_header("Sec-WebSocket-Version",ss.str());
    return error::make_error_code(error::unsupported_version);
}

template <typename config>
lib::error_code my_connection<config>::process_handshake_request() {
    m_alog->write(log::alevel::devel,"process handshake request");

    if (!processor::is_websocket_handshake(m_request)) {
        // this is not a websocket handshake. Process as plain HTTP
        m_alog->write(log::alevel::devel,"HTTP REQUEST");

        // extract URI from request
        m_uri = processor::get_uri_from_host(
            m_request,
            (transport_con_type::is_secure() ? "https" : "http")
        );

        if (!m_uri->get_valid()) {
            m_alog->write(log::alevel::devel, "Bad request: failed to parse uri");
            m_response.set_status(http::status_code::bad_request);
            return error::make_error_code(error::invalid_uri);
        }

        if (m_http_handler) {
            m_is_http = true;
            m_http_handler(m_connection_hdl);
            
            if (m_state == session::state::closed) {
                return error::make_error_code(error::http_connection_ended);
            }
        } else {
            set_status(http::status_code::upgrade_required);
            return error::make_error_code(error::upgrade_required);
        }

        return lib::error_code();
    }

    lib::error_code ec = m_processor->validate_handshake(m_request);

    // Validate: make sure all required elements are present.
    if (ec){
        // Not a valid handshake request
        m_alog->write(log::alevel::devel, "Bad request " + ec.message());
        m_response.set_status(http::status_code::bad_request);
        return ec;
    }

    // Read extension parameters and set up values necessary for the end user
    // to complete extension negotiation.
    std::pair<lib::error_code,std::string> neg_results;
    neg_results = m_processor->negotiate_extensions(m_request);

    if (neg_results.first == processor::error::make_error_code(processor::error::extension_parse_error)) {
        // There was a fatal error in extension parsing that should result in
        // a failed connection attempt.
        m_elog->write(log::elevel::info, "Bad request: " + neg_results.first.message());
        m_response.set_status(http::status_code::bad_request);
        return neg_results.first;
    } else if (neg_results.first) {
        // There was a fatal error in extension processing that is probably our
        // fault. Consider extension negotiation to have failed and continue as
        // if extensions were not supported
        m_elog->write(log::elevel::info, 
            "Extension negotiation failed: " + neg_results.first.message());
    } else {
        // extension negotiation succeeded, set response header accordingly
        // we don't send an empty extensions header because it breaks many
        // clients.
        if (neg_results.second.size() > 0) {
            m_response.replace_header("Sec-WebSocket-Extensions",
                neg_results.second);
        }
    }

    // extract URI from request
    m_uri = m_processor->get_uri(m_request);


    if (!m_uri->get_valid()) {
        m_alog->write(log::alevel::devel, "Bad request: failed to parse uri");
        m_response.set_status(http::status_code::bad_request);
        return error::make_error_code(error::invalid_uri);
    }

    // extract subprotocols
    lib::error_code subp_ec = m_processor->extract_subprotocols(m_request,
        m_requested_subprotocols);

    if (subp_ec) {
        // should we do anything?
    }

    // Ask application to validate the connection
    if (!m_validate_handler || m_validate_handler(m_connection_hdl)) {
        m_response.set_status(http::status_code::switching_protocols);

        // Write the appropriate response headers based on request and
        // processor version
        ec = m_processor->process_handshake(m_request,m_subprotocol,m_response);

        if (ec) {
            std::stringstream s;
            s << "Processing error: " << ec << "(" << ec.message() << ")";
            m_alog->write(log::alevel::devel, s.str());

            m_response.set_status(http::status_code::internal_server_error);
            return ec;
        }
    } else {
        // User application has rejected the handshake
        m_alog->write(log::alevel::devel, "USER REJECT");

        // Use Bad Request if the user handler did not provide a more
        // specific http response error code.
        // TODO: is there a better default?
        if (m_response.get_status_code() == http::status_code::uninitialized) {
            m_response.set_status(http::status_code::bad_request);
        }
        
        return error::make_error_code(error::rejected);
    }

    return lib::error_code();
}

template <typename config>
void my_connection<config>::write_http_response(lib::error_code const & ec) {
    m_alog->write(log::alevel::devel,"connection write_http_response");

    if (ec == error::make_error_code(error::http_connection_ended)) {
        m_alog->write(log::alevel::http,"An HTTP handler took over the connection.");
        return;
    }

    if (m_response.get_status_code() == http::status_code::uninitialized) {
        m_response.set_status(http::status_code::internal_server_error);
        m_ec = error::make_error_code(error::general);
    } else {
        m_ec = ec;
    }

    m_response.set_version("HTTP/1.1");

    // Set server header based on the user agent settings
    if (m_response.get_header("Server").empty()) {
        if (!m_user_agent.empty()) {
            m_response.replace_header("Server",m_user_agent);
        } else {
            m_response.remove_header("Server");
        }
    }

    // have the processor generate the raw bytes for the wire (if it exists)
    if (m_processor) {
        m_handshake_buffer = m_processor->get_raw(m_response);
    } else {
        // a processor wont exist for raw HTTP responses.
        m_handshake_buffer = m_response.raw();
    }

    if (m_alog->static_test(log::alevel::devel)) {
        m_alog->write(log::alevel::devel,"Raw Handshake response:\n"+m_handshake_buffer);
        if (!m_response.get_header("Sec-WebSocket-Key3").empty()) {
            m_alog->write(log::alevel::devel,
                utility::to_hex(m_response.get_header("Sec-WebSocket-Key3")));
        }
    }

    // write raw bytes
    transport_con_type::async_write(
        m_handshake_buffer.data(),
        m_handshake_buffer.size(),
        lib::bind(
            &type::handle_write_http_response,
            type::get_shared(),
            lib::placeholders::_1
        )
    );
}

template <typename config>
void my_connection<config>::handle_write_http_response(lib::error_code const & ec) {
    m_alog->write(log::alevel::devel,"handle_write_http_response");

    lib::error_code ecm = ec;

    if (!ecm) {
        scoped_lock_type lock(m_connection_state_lock);
        
        if (m_state == session::state::connecting) {
             if (m_internal_state != istate::PROCESS_HTTP_REQUEST) {
                ecm = error::make_error_code(error::invalid_state);
             }
        } else if (m_state == session::state::closed) {
            // The connection was canceled while the response was being sent,
            // usually by the handshake timer. This is basically expected
            // (though hopefully rare) and there is nothing we can do so ignore.
            m_alog->write(log::alevel::devel,
                "handle_write_http_response invoked after connection was closed");
            return;
        } else {
            ecm = error::make_error_code(error::invalid_state);
        }
    }

    if (ecm) {
        if (ecm == transport::error::eof && m_state == session::state::closed) {
            // we expect to get eof if the connection is closed already
            m_alog->write(log::alevel::devel,
                    "got (expected) eof/state error from closed con");
            return;
        }
        
        log_err(log::elevel::rerror,"handle_write_http_response",ecm);
        this->terminate(ecm);
        return;
    }

    if (m_handshake_timer) {
        m_handshake_timer->cancel();
        m_handshake_timer.reset();
    }

    if (m_response.get_status_code() != http::status_code::switching_protocols)
    {
        /*if (m_processor || m_ec == error::http_parse_error || 
            m_ec == error::invalid_version || m_ec == error::unsupported_version
            || m_ec == error::upgrade_required)
        {*/
        if (!m_is_http) {
            std::stringstream s;
            s << "Handshake ended with HTTP error: "
              << m_response.get_status_code();
            m_elog->write(log::elevel::rerror,s.str());
        } else {
            // if this was not a websocket connection, we have written
            // the expected response and the connection can be closed.
            
            this->log_http_result();
            
            if (m_ec) {
                m_alog->write(log::alevel::devel,
                    "got to writing HTTP results with m_ec set: "+m_ec.message());
            }
            m_ec = make_error_code(error::http_connection_ended);
        }        
        
        this->terminate(m_ec);
        return;
    }

    this->log_open_result();

    m_internal_state = istate::PROCESS_CONNECTION;
    m_state = session::state::open;

    if (m_open_handler) {
        m_open_handler(m_connection_hdl);
    }

    this->handle_read_frame(lib::error_code(), m_buf_cursor);
}

template <typename config>
void my_connection<config>::send_http_request() {
    m_alog->write(log::alevel::devel,"connection send_http_request");

    // TODO: origin header?

    // Have the protocol processor fill in the appropriate fields based on the
    // selected client version
    if (m_processor) {
        lib::error_code ec;
        ec = m_processor->client_handshake_request(m_request,m_uri,
            m_requested_subprotocols);

        if (ec) {
            log_err(log::elevel::fatal,"Internal library error: Processor",ec);
            return;
        }
    } else {
        m_elog->write(log::elevel::fatal,"Internal library error: missing processor");
        return;
    }

    // Unless the user has overridden the user agent, send generic WS++ UA.
    if (m_request.get_header("User-Agent").empty()) {
        if (!m_user_agent.empty()) {
            m_request.replace_header("User-Agent",m_user_agent);
        } else {
            m_request.remove_header("User-Agent");
        }
    }

    m_handshake_buffer = m_request.raw();

    if (m_alog->static_test(log::alevel::devel)) {
        m_alog->write(log::alevel::devel,"Raw Handshake request:\n"+m_handshake_buffer);
    }

    if (m_open_handshake_timeout_dur > 0) {
        m_handshake_timer = transport_con_type::set_timer(
            m_open_handshake_timeout_dur,
            lib::bind(
                &type::handle_open_handshake_timeout,
                type::get_shared(),
                lib::placeholders::_1
            )
        );
    }

    transport_con_type::async_write(
        m_handshake_buffer.data(),
        m_handshake_buffer.size(),
        lib::bind(
            &type::handle_send_http_request,
            type::get_shared(),
            lib::placeholders::_1
        )
    );
}

template <typename config>
void my_connection<config>::handle_send_http_request(lib::error_code const & ec) {
    m_alog->write(log::alevel::devel,"handle_send_http_request");

    lib::error_code ecm = ec;

    if (!ecm) {
        scoped_lock_type lock(m_connection_state_lock);
        
        if (m_state == session::state::connecting) {
             if (m_internal_state != istate::WRITE_HTTP_REQUEST) {
                ecm = error::make_error_code(error::invalid_state);
             } else {
                m_internal_state = istate::READ_HTTP_RESPONSE;
             }
        } else if (m_state == session::state::closed) {
            // The connection was canceled while the response was being sent,
            // usually by the handshake timer. This is basically expected
            // (though hopefully rare) and there is nothing we can do so ignore.
            m_alog->write(log::alevel::devel,
                "handle_send_http_request invoked after connection was closed");
            return;
        } else {
            ecm = error::make_error_code(error::invalid_state);
        }
    }

    if (ecm) {
        if (ecm == transport::error::eof && m_state == session::state::closed) {
            // we expect to get eof if the connection is closed already
            m_alog->write(log::alevel::devel,
                    "got (expected) eof/state error from closed con");
            return;
        }
        
        log_err(log::elevel::rerror,"handle_send_http_request",ecm);
        this->terminate(ecm);
        return;
    }

    transport_con_type::async_read_at_least(
        1,
        m_buf,
        config::connection_read_buffer_size,
        lib::bind(
            &type::handle_read_http_response,
            type::get_shared(),
            lib::placeholders::_1,
            lib::placeholders::_2
        )
    );
}

template <typename config>
void my_connection<config>::handle_read_http_response(lib::error_code const & ec,
    size_t bytes_transferred)
{
    m_alog->write(log::alevel::devel,"handle_read_http_response");

    lib::error_code ecm = ec;

    if (!ecm) {
        scoped_lock_type lock(m_connection_state_lock);
        
        if (m_state == session::state::connecting) {
            if (m_internal_state != istate::READ_HTTP_RESPONSE) {
                ecm = error::make_error_code(error::invalid_state);
            }
        } else if (m_state == session::state::closed) {
            // The connection was canceled while the response was being sent,
            // usually by the handshake timer. This is basically expected
            // (though hopefully rare) and there is nothing we can do so ignore.
            m_alog->write(log::alevel::devel,
                "handle_read_http_response invoked after connection was closed");
            return;
        } else {
            ecm = error::make_error_code(error::invalid_state);
        }
    }

    if (ecm) {
        if (ecm == transport::error::eof && m_state == session::state::closed) {
            // we expect to get eof if the connection is closed already
            m_alog->write(log::alevel::devel,
                    "got (expected) eof/state error from closed con");
            return;
        }
        
        log_err(log::elevel::rerror,"handle_read_http_response",ecm);
        this->terminate(ecm);
        return;
    }
    
    size_t bytes_processed = 0;
    // TODO: refactor this to use error codes rather than exceptions
    try {
        bytes_processed = m_response.consume(m_buf,bytes_transferred);
    } catch (http::exception & e) {
        m_elog->write(log::elevel::rerror,
            std::string("error in handle_read_http_response: ")+e.what());
        this->terminate(make_error_code(error::general));
        return;
    }

    m_alog->write(log::alevel::devel,std::string("Raw response: ")+m_response.raw());

    if (m_response.headers_ready()) {
        if (m_handshake_timer) {
            m_handshake_timer->cancel();
            m_handshake_timer.reset();
        }

        lib::error_code validate_ec = m_processor->validate_server_handshake_response(
            m_request,
            m_response
        );
        if (validate_ec) {
            log_err(log::elevel::rerror,"Server handshake response",validate_ec);
            this->terminate(validate_ec);
            return;
        }

        // Read extension parameters and set up values necessary for the end
        // user to complete extension negotiation.
        std::pair<lib::error_code,std::string> neg_results;
        neg_results = m_processor->negotiate_extensions(m_response);

        if (neg_results.first) {
            // There was a fatal error in extension negotiation. For the moment
            // kill all connections that fail extension negotiation.
            
            // TODO: deal with cases where the response is well formed but 
            // doesn't match the options requested by the client. Its possible
            // that the best behavior in this cases is to log and continue with
            // an unextended connection.
            m_alog->write(log::alevel::devel, "Extension negotiation failed: "
                + neg_results.first.message());
            this->terminate(make_error_code(error::extension_neg_failed));
            // TODO: close connection with reason 1010 (and list extensions)
        }

        // response is valid, connection can now be assumed to be open      
        m_internal_state = istate::PROCESS_CONNECTION;
        m_state = session::state::open;

        this->log_open_result();

        if (m_open_handler) {
            m_open_handler(m_connection_hdl);
        }

        // The remaining bytes in m_buf are frame data. Copy them to the
        // beginning of the buffer and note the length. They will be read after
        // the handshake completes and before more bytes are read.
        std::copy(m_buf+bytes_processed,m_buf+bytes_transferred,m_buf);
        m_buf_cursor = bytes_transferred-bytes_processed;

        this->handle_read_frame(lib::error_code(), m_buf_cursor);
    } else {
        transport_con_type::async_read_at_least(
            1,
            m_buf,
            config::connection_read_buffer_size,
            lib::bind(
                &type::handle_read_http_response,
                type::get_shared(),
                lib::placeholders::_1,
                lib::placeholders::_2
            )
        );
    }
}

template <typename config>
void my_connection<config>::handle_open_handshake_timeout(
    lib::error_code const & ec)
{
    if (ec == transport::error::operation_aborted) {
        m_alog->write(log::alevel::devel,"open handshake timer cancelled");
    } else if (ec) {
        m_alog->write(log::alevel::devel,
            "open handle_open_handshake_timeout error: "+ec.message());
        // TODO: ignore or fail here?
    } else {
        m_alog->write(log::alevel::devel,"open handshake timer expired");
        terminate(make_error_code(error::open_handshake_timeout));
    }
}

template <typename config>
void my_connection<config>::handle_close_handshake_timeout(
    lib::error_code const & ec)
{
    if (ec == transport::error::operation_aborted) {
        m_alog->write(log::alevel::devel,"asio close handshake timer cancelled");
    } else if (ec) {
        m_alog->write(log::alevel::devel,
            "asio open handle_close_handshake_timeout error: "+ec.message());
        // TODO: ignore or fail here?
    } else {
        m_alog->write(log::alevel::devel, "asio close handshake timer expired");
        terminate(make_error_code(error::close_handshake_timeout));
    }
}

template <typename config>
void my_connection<config>::terminate(lib::error_code const & ec) {
    if (m_alog->static_test(log::alevel::devel)) {
        m_alog->write(log::alevel::devel,"connection terminate");
    }

    // Cancel close handshake timer
    if (m_handshake_timer) {
        m_handshake_timer->cancel();
        m_handshake_timer.reset();
    }

    terminate_status tstat = unknown;
    if (ec) {
        m_ec = ec;
        m_local_close_code = close::status::abnormal_close;
        m_local_close_reason = ec.message();
    }

    // TODO: does any of this need a mutex?
    if (m_is_http) {
        m_http_state = session::http_state::closed;
    }
    if (m_state == session::state::connecting) {
        m_state = session::state::closed;
        tstat = failed;
        
        // Log fail result here before socket is shut down and we can't get
        // the remote address, etc anymore
        if (m_ec != error::http_connection_ended) {
            log_fail_result();
        }
    } else if (m_state != session::state::closed) {
        m_state = session::state::closed;
        tstat = closed;
    } else {
        m_alog->write(log::alevel::devel,
            "terminate called on connection that was already terminated");
        return;
    }

    // TODO: choose between shutdown and close based on error code sent

    transport_con_type::async_shutdown(
        lib::bind(
            &type::handle_terminate,
            type::get_shared(),
            tstat,
            lib::placeholders::_1
        )
    );
}

template <typename config>
void my_connection<config>::handle_terminate(terminate_status tstat,
    lib::error_code const & ec)
{
    if (m_alog->static_test(log::alevel::devel)) {
        m_alog->write(log::alevel::devel,"connection handle_terminate");
    }

    if (ec) {
        // there was an error actually shutting down the connection
        log_err(log::elevel::devel,"handle_terminate",ec);
    }

    // clean shutdown
    if (tstat == failed) {
        if (m_ec != error::http_connection_ended) {
            if (m_fail_handler) {
                m_fail_handler(m_connection_hdl);
            }
        }
    } else if (tstat == closed) {
        if (m_close_handler) {
            m_close_handler(m_connection_hdl);
        }
        log_close_result();
    } else {
        m_elog->write(log::elevel::rerror,"Unknown terminate_status");
    }

    // call the termination handler if it exists
    // if it exists it might (but shouldn't) refer to a bad memory location.
    // If it does, we don't care and should catch and ignore it.
    if (m_termination_handler) {
        try {
            m_termination_handler(type::get_shared());
        } catch (std::exception const & e) {
            m_elog->write(log::elevel::warn,
                std::string("termination_handler call failed. Reason was: ")+e.what());
        }
    }
}

template <typename config>
void my_connection<config>::write_frame() {
    //m_alog->write(log::alevel::devel,"connection write_frame");

    {
        scoped_lock_type lock(m_write_lock);

        // Check the write flag. If true, there is an outstanding transport
        // write already. In this case we just return. The write handler will
        // start a new write if the write queue isn't empty. If false, we set
        // the write flag and proceed to initiate a transport write.
        if (m_write_flag) {
            return;
        }

        // pull off all the messages that are ready to write.
        // stop if we get a message marked terminal
        message_ptr next_message = write_pop();
        while (next_message) {
            m_current_msgs.push_back(next_message);
            if (!next_message->get_terminal()) {
                next_message = write_pop();
            } else {
                next_message = message_ptr();
            }
        }
        
        if (m_current_msgs.empty()) {
            // there was nothing to send
            return;
        } else {
            // At this point we own the next messages to be sent and are
            // responsible for holding the write flag until they are 
            // successfully sent or there is some error
            m_write_flag = true;
        }
    }

    typename std::vector<message_ptr>::iterator it;
    for (it = m_current_msgs.begin(); it != m_current_msgs.end(); ++it) {
        std::string const & header = (*it)->get_header();
        std::string const & payload = (*it)->get_payload();

        m_send_buffer.push_back(transport::buffer(header.c_str(),header.size()));
        m_send_buffer.push_back(transport::buffer(payload.c_str(),payload.size()));   
    }

    // Print detailed send stats if those log levels are enabled
    if (m_alog->static_test(log::alevel::frame_header)) {
    if (m_alog->dynamic_test(log::alevel::frame_header)) {
        std::stringstream general,header,payload;
        
        general << "Dispatching write containing " << m_current_msgs.size()
                <<" message(s) containing ";
        header << "Header Bytes: \n";
        payload << "Payload Bytes: \n";
        
        size_t hbytes = 0;
        size_t pbytes = 0;
        
        for (size_t i = 0; i < m_current_msgs.size(); i++) {
            hbytes += m_current_msgs[i]->get_header().size();
            pbytes += m_current_msgs[i]->get_payload().size();

            
            header << "[" << i << "] (" 
                   << m_current_msgs[i]->get_header().size() << ") " 
                   << utility::to_hex(m_current_msgs[i]->get_header()) << "\n";

            if (m_alog->static_test(log::alevel::frame_payload)) {
            if (m_alog->dynamic_test(log::alevel::frame_payload)) {
                payload << "[" << i << "] (" 
                        << m_current_msgs[i]->get_payload().size() << ") ["<<m_current_msgs[i]->get_opcode()<<"] "
                        << (m_current_msgs[i]->get_opcode() == frame::opcode::text ? 
                                m_current_msgs[i]->get_payload() : 
                                utility::to_hex(m_current_msgs[i]->get_payload())
                           ) 
                        << "\n";
            }
            }  
        }
        
        general << hbytes << " header bytes and " << pbytes << " payload bytes";
        
        m_alog->write(log::alevel::frame_header,general.str());
        m_alog->write(log::alevel::frame_header,header.str());
        m_alog->write(log::alevel::frame_payload,payload.str());
    }
    }

    transport_con_type::async_write(
        m_send_buffer,
        m_write_frame_handler
    );
}

template <typename config>
void my_connection<config>::handle_write_frame(lib::error_code const & ec)
{
    if (m_alog->static_test(log::alevel::devel)) {
        m_alog->write(log::alevel::devel,"connection handle_write_frame");
    }

    bool terminal = m_current_msgs.back()->get_terminal();

    m_send_buffer.clear();
    m_current_msgs.clear();
    // TODO: recycle instead of deleting

    if (ec) {
        log_err(log::elevel::fatal,"handle_write_frame",ec);
        this->terminate(ec);
        return;
    }

    if (terminal) {
        this->terminate(lib::error_code());
        return;
    }

    bool needs_writing = false;
    {
        scoped_lock_type lock(m_write_lock);

        // release write flag
        m_write_flag = false;

        needs_writing = !m_send_queue.empty();
    }

    if (needs_writing) {
        transport_con_type::dispatch(lib::bind(
            &type::write_frame,
            type::get_shared()
        ));
    }
}

template <typename config>
std::vector<int> const & my_connection<config>::get_supported_versions() const
{
    return versions_supported;
}

template <typename config>
void my_connection<config>::process_control_frame(typename config::message_type::ptr msg)
{
    m_alog->write(log::alevel::devel,"process_control_frame");

    frame::opcode::value op = msg->get_opcode();
    lib::error_code ec;

    std::stringstream s;
    s << "Control frame received with opcode " << op;
    m_alog->write(log::alevel::control,s.str());

    if (m_state == session::state::closed) {
        m_elog->write(log::elevel::warn,"got frame in state closed");
        return;
    }
    if (op != frame::opcode::CLOSE && m_state != session::state::open) {
        m_elog->write(log::elevel::warn,"got non-close frame in state closing");
        return;
    }

    if (op == frame::opcode::PING) {
        bool should_reply = true;

        if (m_ping_handler) {
            should_reply = m_ping_handler(m_connection_hdl, msg->get_payload());
        }

        if (should_reply) {
            this->pong(msg->get_payload(),ec);
            if (ec) {
                log_err(log::elevel::devel,"Failed to send response pong",ec);
            }
        }
    } else if (op == frame::opcode::PONG) {
        if (m_pong_handler) {
            m_pong_handler(m_connection_hdl, msg->get_payload());
        }
        if (m_ping_timer) {
            m_ping_timer->cancel();
        }
    } else if (op == frame::opcode::CLOSE) {
        m_alog->write(log::alevel::devel,"got close frame");
        // record close code and reason somewhere

        m_remote_close_code = close::extract_code(msg->get_payload(),ec);
        if (ec) {
            s.str("");
            if (config::drop_on_protocol_error) {
                s << "Received invalid close code " << m_remote_close_code
                  << " dropping connection per config.";
                m_elog->write(log::elevel::devel,s.str());
                this->terminate(ec);
            } else {
                s << "Received invalid close code " << m_remote_close_code
                  << " sending acknowledgement and closing";
                m_elog->write(log::elevel::devel,s.str());
                ec = send_close_ack(close::status::protocol_error,
                    "Invalid close code");
                if (ec) {
                    log_err(log::elevel::devel,"send_close_ack",ec);
                }
            }
            return;
        }

        m_remote_close_reason = close::extract_reason(msg->get_payload(),ec);
        if (ec) {
            if (config::drop_on_protocol_error) {
                m_elog->write(log::elevel::devel,
                    "Received invalid close reason. Dropping connection per config");
                this->terminate(ec);
            } else {
                m_elog->write(log::elevel::devel,
                    "Received invalid close reason. Sending acknowledgement and closing");
                ec = send_close_ack(close::status::protocol_error,
                    "Invalid close reason");
                if (ec) {
                    log_err(log::elevel::devel,"send_close_ack",ec);
                }
            }
            return;
        }

        if (m_state == session::state::open) {
            s.str("");
            s << "Received close frame with code " << m_remote_close_code
              << " and reason " << m_remote_close_reason;
            m_alog->write(log::alevel::devel,s.str());

            ec = send_close_ack();
            if (ec) {
                log_err(log::elevel::devel,"send_close_ack",ec);
            }
        } else if (m_state == session::state::closing && !m_was_clean) {
            // ack of our close
            m_alog->write(log::alevel::devel, "Got acknowledgement of close");

            m_was_clean = true;

            // If we are a server terminate the connection now. Clients should
            // leave the connection open to give the server an opportunity to
            // initiate the TCP close. The client's timer will handle closing
            // its side of the connection if the server misbehaves.
            //
            // TODO: different behavior if the underlying transport doesn't
            // support timers?
            if (m_is_server) {
                terminate(lib::error_code());
            }
        } else {
            // spurious, ignore
            m_elog->write(log::elevel::devel, "Got close frame in wrong state");
        }
    } else {
        // got an invalid control opcode
        m_elog->write(log::elevel::devel, "Got control frame with invalid opcode");
        // initiate protocol error shutdown
    }
}

template <typename config>
lib::error_code my_connection<config>::send_close_ack(close::status::value code,
    std::string const & reason)
{
    return send_close_frame(code,reason,true,m_is_server);
}

template <typename config>
lib::error_code my_connection<config>::send_close_frame(close::status::value code,
    std::string const & reason, bool ack, bool terminal)
{
    m_alog->write(log::alevel::devel,"send_close_frame");

    // check for special codes

    // If silent close is set, respect it and blank out close information
    // Otherwise use whatever has been specified in the parameters. If
    // parameters specifies close::status::blank then determine what to do
    // based on whether or not this is an ack. If it is not an ack just
    // send blank info. If it is an ack then echo the close information from
    // the remote endpoint.
    if (config::silent_close) {
        m_alog->write(log::alevel::devel,"closing silently");
        m_local_close_code = close::status::no_status;
        m_local_close_reason.clear();
    } else if (code != close::status::blank) {
        m_alog->write(log::alevel::devel,"closing with specified codes");
        m_local_close_code = code;
        m_local_close_reason = reason;
    } else if (!ack) {
        m_alog->write(log::alevel::devel,"closing with no status code");
        m_local_close_code = close::status::no_status;
        m_local_close_reason.clear();
    } else if (m_remote_close_code == close::status::no_status) {
        m_alog->write(log::alevel::devel,
            "acknowledging a no-status close with normal code");
        m_local_close_code = close::status::normal;
        m_local_close_reason.clear();
    } else {
        m_alog->write(log::alevel::devel,"acknowledging with remote codes");
        m_local_close_code = m_remote_close_code;
        m_local_close_reason = m_remote_close_reason;
    }

    std::stringstream s;
    s << "Closing with code: " << m_local_close_code << ", and reason: "
      << m_local_close_reason;
    m_alog->write(log::alevel::devel,s.str());

    message_ptr msg = m_msg_manager->get_message();
    if (!msg) {
        return error::make_error_code(error::no_outgoing_buffers);
    }

    lib::error_code ec = m_processor->prepare_close(m_local_close_code,
        m_local_close_reason,msg);
    if (ec) {
        return ec;
    }

    // Messages flagged terminal will result in the TCP connection being dropped
    // after the message has been written. This is typically used when servers
    // send an ack and when any endpoint encounters a protocol error
    if (terminal) {
        msg->set_terminal(true);
    }

    m_state = session::state::closing;

    if (ack) {
        m_was_clean = true;
    }

    // Start a timer so we don't wait forever for the acknowledgement close
    // frame
    if (m_close_handshake_timeout_dur > 0) {
        m_handshake_timer = transport_con_type::set_timer(
            m_close_handshake_timeout_dur,
            lib::bind(
                &type::handle_close_handshake_timeout,
                type::get_shared(),
                lib::placeholders::_1
            )
        );
    }

    bool needs_writing = false;
    {
        scoped_lock_type lock(m_write_lock);
        write_push(msg);
        needs_writing = !m_write_flag && !m_send_queue.empty();
    }

    if (needs_writing) {
        transport_con_type::dispatch(lib::bind(
            &type::write_frame,
            type::get_shared()
        ));
    }

    return lib::error_code();
}

template <typename config>
typename my_connection<config>::processor_ptr
my_connection<config>::get_processor(int version) const {
    // TODO: allow disabling certain versions
    
    processor_ptr p;
    
    switch (version) {
        case 0:
            p = lib::make_shared<processor::hybi00<config> >(
                transport_con_type::is_secure(),
                m_is_server,
                m_msg_manager
            );
            break;
        case 7:
            p = lib::make_shared<processor::hybi07<config> >(
                transport_con_type::is_secure(),
                m_is_server,
                m_msg_manager,
                lib::ref(m_rng)
            );
            break;
        case 8:
            p = lib::make_shared<processor::hybi08<config> >(
                transport_con_type::is_secure(),
                m_is_server,
                m_msg_manager,
                lib::ref(m_rng)
            );
            break;
        case 13:
            p = lib::make_shared<processor::hybi13<config> >(
                transport_con_type::is_secure(),
                m_is_server,
                m_msg_manager,
                lib::ref(m_rng)
            );
            break;
        default:
            return p;
    }
    
    // Settings not configured by the constructor
    p->set_max_message_size(m_max_message_size);
    
    return p;
}

template <typename config>
void my_connection<config>::write_push(typename config::message_type::ptr msg)
{
    if (!msg) {
        return;
    }

    m_send_buffer_size += msg->get_payload().size();
    m_send_queue.push(msg);

    if (m_alog->static_test(log::alevel::devel)) {
        std::stringstream s;
        s << "write_push: message count: " << m_send_queue.size()
          << " buffer size: " << m_send_buffer_size;
        m_alog->write(log::alevel::devel,s.str());
    }
}

template <typename config>
typename config::message_type::ptr my_connection<config>::write_pop()
{
    message_ptr msg;

    if (m_send_queue.empty()) {
        return msg;
    }

    msg = m_send_queue.front();

    m_send_buffer_size -= msg->get_payload().size();
    m_send_queue.pop();

    if (m_alog->static_test(log::alevel::devel)) {
        std::stringstream s;
        s << "write_pop: message count: " << m_send_queue.size()
          << " buffer size: " << m_send_buffer_size;
        m_alog->write(log::alevel::devel,s.str());
    }
    return msg;
}

template <typename config>
void my_connection<config>::log_open_result()
{
    std::stringstream s;

    int version;
    if (!processor::is_websocket_handshake(m_request)) {
        version = -1;
    } else {
        version = processor::get_websocket_version(m_request);
    }

    // Connection Type
    s << (version == -1 ? "HTTP" : "WebSocket") << " Connection ";

    // Remote endpoint address
    s << transport_con_type::get_remote_endpoint() << " ";

    // Version string if WebSocket
    if (version != -1) {
        s << "v" << version << " ";
    }

    // User Agent
    std::string ua = m_request.get_header("User-Agent");
    if (ua.empty()) {
        s << "\"\" ";
    } else {
        // check if there are any quotes in the user agent
        s << "\"" << utility::string_replace_all(ua,"\"","\\\"") << "\" ";
    }

    // URI
    s << (m_uri ? m_uri->get_resource() : "NULL") << " ";

    // Status code
    s << m_response.get_status_code();

    m_alog->write(log::alevel::connect,s.str());
}

template <typename config>
void my_connection<config>::log_close_result()
{
    std::stringstream s;

    s << "Disconnect "
      << "close local:[" << m_local_close_code
      << (m_local_close_reason.empty() ? "" : ","+m_local_close_reason)
      << "] remote:[" << m_remote_close_code
      << (m_remote_close_reason.empty() ? "" : ","+m_remote_close_reason) << "]";

    m_alog->write(log::alevel::disconnect,s.str());
}

template <typename config>
void my_connection<config>::log_fail_result()
{
    std::stringstream s;
    
    int version = processor::get_websocket_version(m_request);

    // Connection Type
    s << "WebSocket Connection ";

    // Remote endpoint address & WebSocket version
    s << transport_con_type::get_remote_endpoint();
    if (version < 0) {
        s << " -";
    } else {
        s << " v" << version;
    }

    // User Agent
    std::string ua = m_request.get_header("User-Agent");
    if (ua.empty()) {
        s << " \"\" ";
    } else {
        // check if there are any quotes in the user agent
        s << " \"" << utility::string_replace_all(ua,"\"","\\\"") << "\" ";
    }

    // URI
    s << (m_uri ? m_uri->get_resource() : "-");

    // HTTP Status code
    s  << " " << m_response.get_status_code();
    
    // WebSocket++ error code & reason
    s << " " << m_ec << " " << m_ec.message();

    m_alog->write(log::alevel::fail,s.str());
}

template <typename config>
void my_connection<config>::log_http_result() {
    std::stringstream s;

    if (processor::is_websocket_handshake(m_request)) {
        m_alog->write(log::alevel::devel,"Call to log_http_result for WebSocket");
        return;
    }  

    // Connection Type
    s << (m_request.get_header("host").empty() ? "-" : m_request.get_header("host"))
      << " " << transport_con_type::get_remote_endpoint()
      << " \"" << m_request.get_method() 
      << " " << (m_uri ? m_uri->get_resource() : "-") 
      << " " << m_request.get_version() << "\" " << m_response.get_status_code()
      << " " << m_response.get_body().size();
    
    // User Agent
    std::string ua = m_request.get_header("User-Agent");
    if (ua.empty()) {
        s << " \"\" ";
    } else {
        // check if there are any quotes in the user agent
        s << " \"" << utility::string_replace_all(ua,"\"","\\\"") << "\" ";
    }

    m_alog->write(log::alevel::http,s.str());
}

#endif // WEBSOCKETPP_CONNECTION_HPP
