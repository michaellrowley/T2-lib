#include <functional>
#include <future>
#include <random>
#include <string>
#include <iostream>
#include <T2/T2.hpp> // Because we'd be including each header seperately, otherwise.

#define TEST_PORT 9009 // Pretty obvious palindrome that should make debugging easier

bool case_wrapper(std::function<bool()> case_ref, const std::string case_identifier) {
    try {
        if (!case_ref()) {
            std::cerr << "CASE FAILED: " + case_identifier + ".\r\n";
            return false;
        }
    }
    catch (...) {
            std::cerr << "CASE FAILED (VIA EXCEPTION): " + case_identifier + ".\r\n";
        return false;
    }
            std::clog << "CASE PASSED: " + case_identifier + ".\r\n";
    return true;
}

int main(int argc, char** argv) {
    // Note #1: It is not guaranteed that all of these cases will be run in the order that they
    // were declared in (i.e, [0], [1], [2], ..., [n]) as we use async to speed up the
    // calling process.
    // Note #2: We're not actually using the return value of each case right now but it could be
    // useful in the future and there's little performance overhead in a few instructions interacting
    // with EAX.
    static std::pair<std::function<bool()>, std::string> cases[] = {
        {
            []() {
                T2::net::server test_server(TEST_PORT);
                test_server.start_listening([](T2::net::client* const){});
                test_server.stop_listening();
                return true;
            },
            "Listen and stop listening"
        },
        {
            []() {
                T2::net::server test_server(TEST_PORT);
                uint8_t last_byte = 0;
                test_server.start_listening([&last_byte](T2::net::client* const cur_client){
                    boost::asio::mutable_buffer buffer(reinterpret_cast<void*>(&last_byte), 1);
                    if (cur_client->receive_data(buffer) == 0) {
                        throw std::runtime_error("Received no data (probably a timeout)");
                    }
                });
                T2::net::client test_client(boost::asio::ip::tcp::endpoint(
                    boost::asio::ip::make_address("::1"), TEST_PORT));
                test_client.connect();
                uint8_t temp_byte = 0xFF;
                test_client.send_data(boost::asio::const_buffer(&temp_byte, 1));
                test_client.disconnect();
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                if (last_byte != temp_byte) {
                    return false;
                }
                test_client.connect();
                test_client.disconnect();
                test_server.stop_listening();
                return true;
            },
            "Connect client to server"
        },
        {
            []() {
                T2::net::server test_server(TEST_PORT);
                test_server.start_listening([](T2::net::client* const){});
                test_server.stop_listening();
                T2::net::client test_client(boost::asio::ip::tcp::endpoint(
                    boost::asio::ip::make_address("::1"), TEST_PORT));
                try {
                    test_client.connect();
                } catch (...) { return true; }
                test_client.disconnect();
                return false;
            },
            "Connect client to server after stopped listening"
        }
    };

    if (argc != 1) {
        if (argc != 2) {
            std::cout << "Bad argument(s)." << std::endl;
            return -1;
        }
        // Select a specific case to rerun:
        // NOT SAFE: making some huge assumptions during casting process here
        //           and we're even assuming that sizeof(cases) / sizeof(cases[0]) >
        //           case_index which opens this program up to an out-of-bounds memory
        //           execution exploit.
        const unsigned int case_index = std::stoi(argv[1]);
        if (!case_wrapper(cases[case_index].first, std::to_string(case_index) + ": '" +
            cases[case_index].second + "'")) {
            return -1;
        }
    }
    else {
        for (size_t i = 0; i < (sizeof(cases) / sizeof(cases[0])); i++)
            std::async(std::launch::async, case_wrapper, cases[i].first, std::to_string(i) +
                ": '" + cases[i].second + "'");
    }
    while (true) { /* TODO: Maybe just wait until all cases have been run?! */ }
    return 1;
}