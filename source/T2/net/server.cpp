#include <functional> // Exclusively for logging purposes.
#include <iostream>
#include <mutex>
#include <thread>

#include <boost/lexical_cast.hpp> // Exclusively for logging purposes.

#include "./net.hpp"

T2::net::server::server(const uint16_t port) : port(port) { }

void T2::net::server::call_handlers(const std::vector<std::function<void(T2::net::client* const)>>& handlers,
    T2::net::client* const accepted_client) {

    for (const std::function<void(T2::net::client* const)>& iterative_handler : connection_handlers) {
        try {
            iterative_handler(accepted_client);
        } catch (std::runtime_error& exception_object) {
#if defined(_DEBUG)
        std::clog << "T2::net::server::listen_loop() @ " + std::to_string(__LINE__) + ": "
            "Iterative handler threw an exception when processing connection - " +
            std::string(exception_object.what()) + ".\r\n";
#endif
            if (!catch_listeners) {
                delete accepted_client; // Disconnects and frees resources.
                std::__throw_runtime_error("Server listener threw an exception.");
            }
        }
    }
    // In this capacity, delete will call the destructor for T2::net::client which will,
    // if appropriate, disconnect safely and then free resources.
    delete accepted_client;
}

void T2::net::server::listen_loop(
        const std::vector<std::function<void(T2::net::client* const)>>& connection_handlers,
        const bool catch_listeners) {
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

        // T2::net::server::call_handlers will delete the T2::net::client object when finished.
        T2::net::client* const accepted_client = new T2::net::client(active_socket);
        // TODO: Bind the call_handlers function to connection_handlers at the start.
        std::async(std::launch::async, T2::net::server::call_handlers, connection_handlers, accepted_client);
    }
    this->cleaned_up = true;
}

void T2::net::server::start_listening(const std::function<void(T2::net::client* const)>& connection_handler,
    const bool catch_listeners) {
    this->start_listening(
        std::vector<std::function<void(T2::net::client* const)>>{ connection_handler },
        catch_listeners
    );
}

void T2::net::server::start_listening(
        const std::vector<std::function<void(T2::net::client* const)>>& connection_handlers,
        const bool catch_listeners) {

    if (this->actively_listening) {
        std::__throw_runtime_error("T2::net::server::start_listening() was called when the "
            "server was already listening.");
    }

    this->actively_listening = true;
    std::thread(&T2::net::server::listen_loop, this, connection_handlers, catch_listeners).detach();
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