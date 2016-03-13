#ifndef CONNECTION_ENGINE_HPP
#define CONNECTION_ENGINE_HPP

/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "proton/connection.hpp"
#include "proton/connection_options.hpp"
#include "proton/error.hpp"
#include "proton/export.hpp"
#include "proton/pn_unique_ptr.hpp"
#include "proton/types.hpp"

#include <cstddef>
#include <utility>
#include <string>

namespace proton {

class connection_engine_context;
class handler;
class connection;

// TODO aconway 2016-01-23: doc contrast with container.

/// An interface for connection-oriented IO integration.  A
/// connection_engine manages a single AMQP connection.  It is useful
/// for integrating AMQP into an existing IO framework.
///
/// The engine provides a simple "bytes-in/bytes-out" interface. Incoming AMQP
/// bytes from any kind of data connection are fed into the engine and processed
/// to dispatch events to a proton::handler.  The resulting AMQP output data is
/// available from the engine and can sent back over the connection.
///
/// The engine does no IO of its own. It assumes a two-way flow of bytes over
/// some externally-managed "connection". The "connection" could be a socket
/// managed by select, poll, epoll or some other mechanism, or it could be
/// something else such as an RDMA connection, a shared-memory buffer or a Unix
/// pipe.
///
/// The application is coded the same way for engine or container: you implement
/// proton::handler. Handlers attached to an engine will receive transport,
/// connection, session, link and message events. They will not receive reactor,
/// selectable or timer events, the engine assumes those are managed externally.
///
/// THREAD SAFETY: A single engine instance cannot be called concurrently, but
/// different engine instances can be processed concurrently in separate threads.
class
PN_CPP_CLASS_EXTERN connection_engine {
  public:
    class container {
      public:
        /// Create a container with id.  Default to random UUID if id
        /// == "".
        PN_CPP_EXTERN container(const std::string &id = "");
        PN_CPP_EXTERN ~container();

        /// Return the container-id
        PN_CPP_EXTERN std::string id() const;

        /// Make options to configure a new engine, using the default options.
        ///
        /// Call this once for each new engine as the options include a generated unique link_prefix.
        /// You can modify the configuration before creating the engine but you should not
        /// modify the container_id or link_prefix.
        PN_CPP_EXTERN connection_options make_options();

        /// Set the default options to be used for connection engines.
        /// The container will set the container_id and link_prefix when make_options is called.
        PN_CPP_EXTERN void options(const connection_options&);

      private:
        class impl;
        internal::pn_unique_ptr<impl> impl_;
    };
    /// Create a connection engine that dispatches to handler.
    PN_CPP_EXTERN connection_engine(handler&, const connection_options& = no_opts);

    PN_CPP_EXTERN virtual ~connection_engine();

    /// Return the number of bytes that the engine is currently ready to read.
    PN_CPP_EXTERN size_t can_read() const;

    /// Return the number of bytes that the engine is currently ready to write.
    PN_CPP_EXTERN size_t can_write() const;

    /// Combine these flags with | to indicate read, write, both or neither
    enum io_flag {
        READ = 1,
        WRITE = 2
    };

    /// Read, write and dispatch events.
    ///
    /// io_flags indicates whether to read, write, both or neither.
    /// Dispatches all events generated by reading or writing.
    /// Use closed() to check if the engine is closed after processing.
    ///
    /// @throw exceptions thrown by the engines handler or the IO adapter.
    PN_CPP_EXTERN void process(int io_flags=READ|WRITE);

    /// True if the engine is closed, meaning there are no further
    /// events to process and close_io has been called.  Call
    /// error_str() to get an error description.
    PN_CPP_EXTERN bool closed() const;

    /// Get the AMQP connection associated with this connection_engine.
    PN_CPP_EXTERN class connection connection() const;

    /// Thrown by io_read and io_write functions to indicate an error.
    struct PN_CPP_CLASS_EXTERN io_error : public error {
        PN_CPP_EXTERN explicit io_error(const std::string&);
    };

  protected:
    /// Do a non-blocking read on the IO stream.
    ///
    ///@return pair(size, true) if size bytes were read.
    /// size==0 means no data could be read without blocking, the stream is still open.
    /// Returns pair(0, false) if the stream closed.
    ///
    ///@throw proton::connection_engine::io_error if there is a read error.
    virtual std::pair<size_t, bool> io_read(char* buf, size_t max) = 0;

    /// Do a non-blocking write of up to max bytes from buf.
    ///
    /// Return the number of byes written , 0 if no data could be written
    /// without blocking.
    ///
    ///throw proton::connection_engine::io_error if there is a write error.
    virtual size_t io_write(const char*, size_t) = 0;

    /// Close the io, no more _io methods will be called after this is called.
    virtual void io_close() = 0;

    PN_CPP_EXTERN static const connection_options no_opts;

  private:
    connection_engine(const connection_engine&);
    connection_engine& operator=(const connection_engine&);

    void dispatch();
    void try_read();
    void try_write();

    class connection connection_;
    connection_engine_context* ctx_;
};

}

#endif // CONNECTION_ENGINE_HPP
