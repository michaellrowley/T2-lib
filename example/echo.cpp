// clang++ example/echo.cpp -std=c++20 -Wall source/T2/net/client.cpp source/T2/net/server.cpp source/T2/utility/utility.cpp -I [YOUR_BOOST_PATH] -I source/

#include <T2/net/net.hpp>
#include <iostream>
#include <thread>

int main(int argc, char** argv) {
    if (argc == 1) {
        std::cout << "Provide an argument. [1]: Port to listen on.\r\n";
        return -1;
    }
    // Not a safe conversion or cast:
    T2::net::server listener(static_cast<uint16_t>(std::stoi(argv[1])));
    listener.start_listening([](T2::net::client* const connection) {
        boost::asio::mutable_buffer buf(reinterpret_cast<void*>(new uint8_t[256]), 256);
        const size_t received_amount = connection->receive_data(buf);
        // Not sure if this cast is safe either:
        std::cout << std::string(reinterpret_cast<const char*>(buf.data()), received_amount) << std::endl;
        delete reinterpret_cast<uint8_t*>(buf.data());
    });
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    return 1;
}