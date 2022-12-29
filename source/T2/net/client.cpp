#include <functional>
#include <iostream> // Exclusively for logging purposes.
#include <thread>
#include <mutex>

#include <boost/lexical_cast.hpp> // Exclusively for logging purposes.
#include <boost/asio/basic_stream_socket.hpp> // native_handle_type

#include "./net.hpp"

void T2::net::client::retire() {
    T2::net::client::retired_flag = T2::net::client::retire_flags::retire_signal;
    // T2::utility::blocking_timer -> maybe in the future, this isn't really a concern.
    // Also, consider using io_context.stop()?
    while (T2::net::client::retired_flag != T2::net::client::retire_flags::finished_retiring) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

void T2::net::client::initialization() {
    static std::thread asio_loop_thread;
    std::unique_lock<std::mutex> instance_count_lock(T2::net::client::active_instance_count.mutex);
    if (T2::net::client::active_instance_count.object++ == 0) {
        asio_loop_thread = std::thread(T2::net::client::asio_loop);
        asio_loop_thread.detach();
    }
    instance_count_lock.unlock();
}

T2::net::client::client(const boost::asio::ip::tcp::endpoint& dest) :
    connection_state(T2::net::client::disconnected),
    connection_socket(boost::asio::ip::tcp::socket(T2::net::client::asio_context)), destination(dest) {
    this->connection_state = T2::net::client::connection_states::disconnected;
    T2::net::client::initialization();
}

T2::net::client::client(boost::asio::ip::tcp::socket& base) : connection_state(T2::net::client::connected),
    connection_socket(boost::asio::ip::tcp::socket(T2::net::client::asio_context)),
    destination(base.remote_endpoint()) {

    const boost::asio::ip::tcp::socket::native_handle_type base_socket_descriptor = base.release();
    connection_socket.assign(this->destination.protocol(), base_socket_descriptor);

    boost::asio::socket_base::keep_alive linger_option(true);
    this->connection_socket.set_option(linger_option);
    this->connection_state = T2::net::client::connection_states::connected;

    T2::net::client::initialization();
}

void T2::net::client::connect(const std::chrono::milliseconds& connect_timeout) {
    if (this->connection_state != T2::net::client::connection_states::disconnected)
        std::__throw_runtime_error("Connect attempted whilst already connected.");

    T2::net::client::asio_request* const request_obj =
        new T2::net::client::asio_request{
        .request_type = T2::net::client::asio_request::asio_request_types::connect,
        .socket = this->connection_socket,
        .request.connection_details = {
            .endpoint = this->destination
        }
    };
    std::unique_lock pending_connections_lock(T2::net::client::pending_asio_requests.mutex);
    T2::net::client::pending_asio_requests.object.push_back(request_obj);
    pending_connections_lock.unlock();

    // Storing the result of this ...::blocking_timer() call isnt' required; the
    // request status of the asio request object will reflect a timeout if applicable.
    T2::utility::blocking_timer(connect_timeout, &request_obj->work_finished);

    // Make sure that the function has used the object (or at least stored its parameters)
    // by this point, after setting disposal_flag we cannot be sure of any members' validity.
    const T2::net::client::asio_request::request_statuses request_status = request_obj->request_status;
    pending_connections_lock.lock();
    request_obj->disposal_flag = true; // [this]->[asio_loop]: "You can free/delete the object now"
    pending_connections_lock.unlock();

    if (request_status == T2::net::client::asio_request::request_statuses::processing) {
        if (this->connection_state == T2::net::client::connection_states::connected)
            this->disconnect();
#if defined(_DEBUG)
        std::clog << "T2::net::client::connect() @ " + std::to_string(__LINE__) +
            ": Timed out after " + std::to_string(connect_timeout.count())
            + "ms. Destination: '" + boost::lexical_cast<std::string>(this->destination) + "'.\r\n";
#endif
       std::__throw_runtime_error("Client-based async_connect() failed to receive callback/handler call.");
    }
    else if (request_status != T2::net::client::asio_request::request_statuses::success/* &&
             request_status != T2::net::client::asio_request::request_statuses::processing*/) {
        if (this->connection_state == T2::net::client::connection_states::connected)
            this->disconnect();
        // An error has been sent to cerr explaining this in more detail.
        std::__throw_runtime_error("Client-based async_connect() received an unexpected "
            "error during connection.");
    }
    this->source = this->connection_socket.local_endpoint();

    boost::asio::socket_base::keep_alive linger_option(true);
    this->connection_socket.set_option(linger_option);
    this->connection_state = T2::net::client::connection_states::connected;
}

void T2::net::client::disconnect() {
    if (this->connection_state != T2::net::client::connection_states::connected)
        std::__throw_runtime_error("Disconnect attempted whilst already disconnected.");
    this->connection_socket.cancel();
    this->connection_socket.close();
    this->connection_state = T2::net::client::connection_states::disconnected;
}

void T2::net::client::send_data(const boost::asio::const_buffer& data) {
    T2::net::client::send_data_base(this->connection_socket, data);
}

size_t T2::net::client::receive_data(const boost::asio::mutable_buffer& data_buffer,
                const std::chrono::milliseconds& timeout) {
    return T2::net::client::receive_data_base(this->connection_socket, data_buffer, timeout);
}

void T2::net::client::send_data_base(boost::asio::ip::tcp::socket& socket,
    const boost::asio::const_buffer& data) {

    T2::net::client::initialization();

    // Opted for the linear, synchronous approach because sending data over
    // a socket shouldn't take long enough to warrant the asynchronous call.

    boost::system::error_code error_code;

    if (error_code) {
        std::__throw_runtime_error("Client-based send() failed to retrieve the destination "
            "string (this is usually indicative of a closed socket).");
    }

    size_t bytes_sent = 0;
    // Only accepting an error code here to silence any exceptions, treating bytes_sent == 0 as
    // an error anyway so this isn't dangerous.
    if ((bytes_sent = socket.send(data, 0, error_code)) != data.size() || error_code) {

#if defined(_DEBUG)
        std::cerr << "T2::net::client::send_data_base() @ " << std::to_string(__LINE__) +
        ": " "Sent an insufficient amount of data (requested '" +
        std::to_string(data.size()) + "', sent '" + std::to_string(bytes_sent) + "') to '" +
        boost::lexical_cast<std::string>(socket.remote_endpoint(error_code)) + "'.\r\n";
#endif
        std::__throw_runtime_error("Client-based send() failed to send the full amount of data.");
    }
#if defined(_DEBUG)
    std::clog << "T2::net::client::send_data_base() @ " + std::to_string(__LINE__) + ": "
        "Sent " + std::to_string(bytes_sent) + "/" + std::to_string(data.size()) +
        " bytes to '" + boost::lexical_cast<std::string>(socket.remote_endpoint(error_code)) + "'.\r\n";
#endif
#if defined(_EXTRA_DEBUG)
    for (size_t i = 0; i < bytes_sent; i++)
        printf("%02X %s", ((uint8_t*)data.data())[i], (((i % 16) == 15) ? "\n" : ""));
    printf("\n\n");
#endif
    return;
}

size_t T2::net::client::receive_data_base(boost::asio::ip::tcp::socket& socket,
    const boost::asio::mutable_buffer& data_buffer, const std::chrono::milliseconds& receive_timeout) {

    T2::net::client::initialization();

    // In contrast to T2::net::client::send_data_base(), it may be better
    // use boost's async_receive counterpart here because receiving data can
    // block for extended periods of time (especially if none has been sent).
    T2::net::client::asio_request* const base_request =
        new T2::net::client::asio_request{
            .request_type = T2::net::client::asio_request::asio_request_types::receive_data,
            .socket = socket,
            .request.receive_details = {
                .buffer = data_buffer,
                .bytes_received = 0
            }
    };

    std::unique_lock pending_lock(T2::net::client::pending_asio_requests.mutex);
    T2::net::client::pending_asio_requests.object.push_back(base_request);
    pending_lock.unlock();

    const bool timed_out = T2::utility::blocking_timer(receive_timeout,
        &base_request->work_finished);

    const size_t bytes_received = base_request->request.receive_details.bytes_received;
    const T2::net::client::asio_request::request_statuses request_status = base_request->request_status;
    pending_lock.lock();
    base_request->disposal_flag = true;
    pending_lock.unlock();

    const std::string dest_string = "'" + boost::lexical_cast<std::string>(socket.remote_endpoint()) + "'";

    if (timed_out) {
#if defined(_DEBUG)
        std::clog << "T2::net::client::receive_data_base() @ " + std::to_string(__LINE__)
        + ": " "Timed out after " + std::to_string(receive_timeout.count()) +
        "ms in attempt to read from " + dest_string + ".\r\n";
#endif
        // throw "Client-based async_receive() timed out.";
        return 0; // This seems like a better way to handle a timeout.
    }
    else if (request_status != T2::net::client::asio_request::request_statuses::success) {
        // An error has been sent to cerr explaining this in more detail.
#if defined(_DEBUG)
        std::cerr << "T2::net::client::receive_data_base() @ " << std::to_string(__LINE__) +
        ": " "Suffered an unexpected error (see above output) whilst receiving from " + dest_string +
        ".\r\n";
#endif
        std::__throw_runtime_error("Client-based async_receive() suffered an unexpected error.");
    }
#if defined(_EXTRA_DEBUG)
    for (size_t i = 0; i < bytes_received; i++)
        printf("%02X %s", ((uint8_t*)data_buffer.data())[i], (((i % 16) == 15) ? "\n" : ""));
    printf("\n\n");
#endif

    return bytes_received;
}

T2::net::client::~client() {
    if (this->connection_state == connected) {
        this->disconnect(); // No need to wrap this in a try/catch, we've just checked connection_state.
    }
    std::unique_lock<std::mutex> count_lock(T2::net::client::active_instance_count.mutex);
    if (--T2::net::client::active_instance_count.object == 0) {
        T2::net::client::retire(); // Could take a while.
    }
    // Not really necessary due to count_lock's going out of scope right now. This
    // is still a good precaution in case I add more code to this destructor later.
    count_lock.unlock();
}

void T2::net::client::asio_loop() {
    // There are two options for optimizing performance here:
    // (1) call ...::io_context.run() for *each request* or
    // (2) call it (^) after processing *all* requests
    // I went for (2) but if a bottleneck occurs as a result, consider changing to (1).
    T2::utility::mutex_wrapped<std::vector<asio_request*>>& requests_wrapper =
        T2::net::client::pending_asio_requests;
    while (T2::net::client::retired_flag == T2::net::client::retire_flags::normal_operation) {
        std::unique_lock pending_asio_lock(requests_wrapper.mutex);
        size_t asio_request_count = requests_wrapper.object.size();
        if (asio_request_count == 0) {
            // TOTWEAK: The sleep duration could effect performance,
            //          fine-tuning it should be done at some point.
            pending_asio_lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            continue;
        }

        for (size_t index = 0; index < asio_request_count; index++) {
            T2::net::client::asio_request* const iterative_request =
                requests_wrapper.object[index];

            if (iterative_request->disposal_flag) {
                requests_wrapper.object.erase(requests_wrapper.object.begin() + index);
                --index;
                --asio_request_count;
                delete iterative_request;
                continue;
            }
            else if (iterative_request->request_status == T2::net::client::asio_request::request_statuses::success) {
                continue;
            }

            if (iterative_request->request_status == T2::net::client::asio_request::request_statuses::unprocessed) {
                iterative_request->request_status = T2::net::client::asio_request::request_statuses::processing;
                switch (iterative_request->request_type) {
                    case T2::net::client::asio_request::asio_request_types::connect: {
                        const T2::net::client::asio_request::request_specific::connection_request& details =
                            iterative_request->request.connection_details;
                        iterative_request->socket.async_connect(details.endpoint,
                        [&iterative_request, &details](const boost::system::error_code& error) {
                            const std::string dest_string = "'" + boost::lexical_cast<std::string>(details.endpoint) + "'";
                            if (error) {
                                iterative_request->request_status =
                                    T2::net::client::asio_request::request_statuses::failed;
#if defined(_DEBUG)
                                std::cerr << "T2::net::client::asio_loop() @ " +
                                    std::to_string(__LINE__) + ": "
                                    "'async_connect()' handler was called with an error code of '" +
                                    std::to_string(error.value()) + "'.\r\n";
#endif
                                iterative_request->work_finished = true;
                                return;
                            }
#if defined(_DEBUG)
                            std::clog << "T2::net::client::asio_loop() @ " +
                                std::to_string(__LINE__) + ": " "'async_connect()' handler was called "
                                "successfully for destination " + dest_string + ".\r\n";
#endif
                            iterative_request->request_status =
                                T2::net::client::asio_request::request_statuses::success;
                            iterative_request->work_finished = true;
                            return; // Just for clarity.
                        });
                    }
                    break;
                    case T2::net::client::asio_request::asio_request_types::receive_data: {
                        iterative_request->socket.async_receive(
                            iterative_request->request.receive_details.buffer,
                            [&iterative_request](const boost::system::error_code& error,
                                size_t bytes_transferred) {
                            
                                if (error) {
                                    iterative_request->request_status =
                                        T2::net::client::asio_request::request_statuses::failed;
    #if defined(_DEBUG)
                                    std::cerr << "T2::net::client::asio_loop() @ " +
                                        std::to_string(__LINE__) + ": " "'async_receive()' handler was "
                                        "called with an error code of '" + error.message() + "'.\r\n";
    #endif
                                    iterative_request->work_finished = true;
                                    return;
                                }
    #if defined(_DEBUG)
                                std::clog << "T2::net::client::asio_loop() @ " + std::to_string(__LINE__) +
                                    ": " "'async_receive()' handler was called successfully, " +
                                    std::to_string(bytes_transferred) + " bytes were received from '" +
                                    boost::lexical_cast<std::string>(iterative_request->socket.remote_endpoint())
                                    + "'.\r\n";
    #endif
                                iterative_request->request.receive_details.bytes_received = bytes_transferred;
                                iterative_request->request_status =
                                    T2::net::client::asio_request::request_statuses::success;
                                iterative_request->work_finished = true;
                            }
                        );
                    }
                    break;
                    default:
                        break;
                }
            }
        }

        pending_asio_lock.unlock();

        // Boost's io_context.run_until() could be used but it would enforce a *universal*
        // timeout. By having the 'push-er' handle timeouts, we can ensure that (A) the
        // call that triggered the pushing won't return too soon and that (B) timeouts
        // can be fine-tuned for each operation (connection, read/write, even individual
        // connections in the future).
        T2::net::client::asio_context.run();
        T2::net::client::asio_context.restart();
    }
    T2::net::client::retired_flag = T2::net::client::retire_flags::finished_retiring;
}
