#ifndef T2_PROTOCOLS
#define T2_PROTOCOLS

#include <vector>

#include <boost/endian/arithmetic.hpp>

#include "./net/net.hpp"

namespace T2 {
    namespace protocols {
        // Everything here is essentially a static class because we're only using one instance of
        // it in the protocol array.
        class protocol_base {
        public:
            protocol_base() {}
            enum identification_status {
                invalid,
                valid,
                need_more_data
            };

            virtual const size_t min_data() {
                std::__throw_runtime_error("Virtual base class member called.");
            } // OVERRIDE THIS
            virtual const size_t max_data() {
                std::__throw_runtime_error("Virtual base class member called.");
            } // OVERRIDE THIS

            // Ideally, these would be static functions (or at least a virtual one)
            // however this makes the use of a standardised parent-based vector
            // impossible (AFAIK, at least)
            virtual identification_status identification_fn(const boost::asio::const_buffer&,
                const boost::asio::ip::tcp::endpoint& local,
                const boost::asio::ip::tcp::endpoint& remote){

                std::__throw_runtime_error("Virtual base class member called.");
            } // OVERRIDE THIS
            virtual void ending_identification() {
                // Some classes just won't need this function, there's no need to throw exception
                // if it isn't defined in the child class.
                // throw "Virtual base class member called.";
            }; // (MAYBE) OVERRIDE THIS
            virtual void handle_connection(boost::asio::ip::tcp::socket& socket){
                std::__throw_runtime_error("Virtual base class member called.");
            } // OVERRIDE THIS
        };
    };
};

#endif