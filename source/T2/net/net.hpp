#ifndef T2_NET
#define T2_NET

#include <chrono>
#include <atomic>
#include <vector>

#include <boost/asio.hpp>

#include "../utility/utility.hpp"

namespace T2 {
    namespace net {
        // A TCP/IP client that is capable of connecting to a remote
        // endpoint and then transmitting data bidirectionally.
        class client {
            // TODO: Consider a safeguard to reset the entire instance
            // of io_context (and wipe the array) if pending_asio_requests
            // gets too full as this could be the symptom of a memory leak.
            // Maybe even dump some debug info (#if 'defined(_DEBUG'), too.
        private:
            // Shared globally
            struct asio_request {
                enum asio_request_types {
                    connect,
                    // send_data, - Non-async approach was utilized.
                    receive_data
                } request_type;

                enum request_statuses {
                    unprocessed,
                    processing,
                    // All of the below are assumedly 'processed'.
                    success,
                    // failed_timeout, - See the 'work_finished' member.
                    failed
                } request_status = unprocessed;

                // The below variable represents whether the io_context is
                // finished processing it, this is more useful than using
                // request_status because it adds compatibility with
                // T2::utility::blocking_timer due to its boolean nature.
                bool work_finished = false;  // 'push-er' can work
                bool disposal_flag = false; // 'push-er' has finished, please erase!

                boost::asio::ip::tcp::socket& socket;
                // Try to leave the union at the end of the struct, if someone
                // decides to disassemble the program it makes it easier
                // to parse by hand when variable-length data is at the end.
                union request_specific {
                    struct connection_request {
                        // T2::net::client* const origin; - Not required (yet).
                        const boost::asio::ip::tcp::endpoint endpoint;
                    } connection_details;
                    struct receive_request {
                        boost::asio::mutable_buffer& buffer;
                        size_t bytes_received;
                    } receive_details;
                } request;
            };
            static void initialization(); // Launches asio_loop thread
            static void retire(); // Triggers (and waits for) the conclusion of the asio_loop thread
            enum retire_flags {
                normal_operation,
                retire_signal,
                finished_retiring
            } static inline retired_flag = normal_operation;
            static inline T2::utility::mutex_wrapped<size_t> active_instance_count;
            static inline T2::utility::mutex_wrapped<std::vector<asio_request*>> pending_asio_requests;
            static void asio_loop();

            // Unique
            boost::asio::ip::tcp::endpoint source; // Not const because we need to init a socket to get this in one constructor.

            enum connection_states {
                disconnected,
                connected
            } connection_state;

        public:
            boost::asio::ip::tcp::socket connection_socket;
            const boost::asio::ip::tcp::endpoint destination;
            // This is pretty annoying but since source isn't 'const' I don't want to leave it public.
            const boost::asio::ip::tcp::endpoint& get_source() const { return this->source; }

            client(const boost::asio::ip::tcp::endpoint& dest);
            client(boost::asio::ip::tcp::socket& base);
            void connect();
            void disconnect();

            // For use when a socket has been created via ...::client constructor.
            void send_data(const boost::asio::const_buffer& data);
            [[nodiscard]] size_t receive_data(boost::asio::mutable_buffer& data_buffer);

            // For use when a function has been passed a boost.ASIO socket
            static void send_data_base(boost::asio::ip::tcp::socket& socket,
                const boost::asio::const_buffer& data);
            [[nodiscard]] static size_t receive_data_base(boost::asio::ip::tcp::socket& socket,
                boost::asio::mutable_buffer& data_buffer);
            
            // This needs to be public so that external functions can instantiate their
            // sockets with this context and then use the ..._base functions as an I/O wrapper.
            static inline boost::asio::io_context asio_context;
            ~client();
        };

        class server {
            // TODO: Finish implementing stop_listening functionality.
            // Maybe even consider having the connection_handlers being
            // passed as a pointer so that external callers can change
            // the vector at runtime.
        private:
            bool cleaned_up = false; // Stores whether the server's listen_loop has returned.
            void listen_loop(
                std::vector<std::function<void(T2::net::client* const)>> connection_handlers);
            bool actively_listening = false;
            uint16_t port;
        public:
            server(const uint16_t port);
            ~server();
            void start_listening(
                std::vector<std::function<void(T2::net::client* const)>> connection_handlers,
                const bool multiple_calls = true);
            // Wrapper around the start_listening function that takes a vector for a parameter.
            void start_listening(std::function<void(T2::net::client* const)> connection_handler);
            void stop_listening(bool silent = false);
        };
    };
};

#endif