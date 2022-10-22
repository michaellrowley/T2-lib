#include <functional> // Exclusively for logging purposes.
#include <iostream>
#include <mutex>
#include <thread>

#include <boost/lexical_cast.hpp> // Exclusively for logging purposes.

#include "./net.hpp"

T2::net::server::server(const uint16_t port) : port(port) { }

void T2::net::server::listen_loop(
        std::vector<std::function<void(T2::net::client* const)>> connection_handlers) {
    // Each server instance gets its own io_context, my thinking
    // is that this will any bottlenecks that would occur with
    // a shared instance. Accepted clients use the T2::client
    // io_context, however.
    boost::asio::io_context asio_context;
    boost::asio::ip::tcp::acceptor server_acceptor = boost::asio::ip::tcp::acceptor(asio_context,
        boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), this->port));
    server_acceptor.non_blocking(true);

    while (this->actively_listening) {
        boost::asio::ip::tcp::socket active_socket(T2::net::client::asio_context);
        boost::system::error_code accept_result;
        server_acceptor.accept(active_socket, accept_result);
        if (accept_result.value() == 35) { // The request would've blocked.
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            continue;
        }

#if defined(_DEBUG)
        std::clog << "T2::net::server::listen_loop() @ " + std::to_string(__LINE__) + ": "
            "Received connection on '" + boost::lexical_cast<std::string>(active_socket.local_endpoint()) +
            "' from '" + boost::lexical_cast<std::string>(active_socket.remote_endpoint()) + "'.\r\n";
#endif

        T2::net::client* const accepted_client = new T2::net::client(active_socket);

        for (std::function<void(T2::net::client* const)>&
                iterative_handler : connection_handlers) {

            // Expecting the iterative_handler to delete accepted_client before returning.
            iterative_handler(accepted_client);
        }
    }
    this->cleaned_up = true;
}

void T2::net::server::start_listening(std::function<void(T2::net::client* const)> connection_handler) {
    std::vector<std::function<void(T2::net::client* const)>> connection_handler_vector = {
        connection_handler
    };
    this->start_listening(connection_handler_vector);
}

void T2::net::server::start_listening(
        std::vector<std::function<void(T2::net::client* const)>>
            connection_handlers,
        const bool multiple_calls) {

    if (this->actively_listening) {
        std::__throw_runtime_error("T2::net::server::start_listening() was called when the "
            "server was already listening.");
    }
    this->actively_listening = true;
    std::thread(&T2::net::server::listen_loop, this, connection_handlers).detach();
    if (multiple_calls) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void T2::net::server::stop_listening() {
    if (!this->actively_listening) {
        std::__throw_runtime_error("T2::net::server::stop_listening() was called when the "
            "server was not actively listening.");
    }
    this->actively_listening = false;
}

T2::net::server::~server() {
    if (this->actively_listening) {
        this->stop_listening();
    }
    while (!this->cleaned_up) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}