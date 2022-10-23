# T2-Lib
This project is an object-oriented wrapper around [boost](https://www.boost.org/)'s networking functionality (mostly stemming from the ``asio`` namespace). It requires that you have access to boost's [``asio``](https://github.com/boostorg/asio), [``endian``](https://github.com/boostorg/endian), and [``lexical_cast``](https://github.com/boostorg/lexical_cast) libraries for compilation (they are included via chevron brackets).

## Documentation
There are two primary classes that are used to communicate with other devices - a ``T2::net::client`` which initiates a connection to a remote endpoint that is already listening for a new connection, and a ``T2::net::server`` which listens on a given port and calls specific connection handlers when a new connection is established on that port.

*Most* of the time you'll want to create an object of class/server type when interacting with this library however in some limited cases, the use of exposed static functions may be more suitable or even required. Static functions are indicated with ⚡️. Functions that may throw a ``std::runtime_error`` are indicated with ⚠️.

### Client

- ⚡️ ``void T2::net::client::initialization()``: Responsible for ensuring that future client objects can run smoothly by using the same ``io_context`` for all clients. **You almost certainly don't need to call this _*private*_ function** because the ``T2::net::client::client()`` constructor does it by default.
- ⚡️ ``void T2::net::client::retire()``: Responsible for cleaning up the ``io_context`` thread. Again, this is private to the ``T2::net::client`` class and will be called automatically when the count of active ``T2::net::client`` objects is zero.
- ``void T2::net::client::client(const boost::asio::ip::tcp::endpoint&)``: A ``T2::net::client`` constructor that takes a ``boost::asio::ip::tcp::endpoint`` (representing the client's destination) as a parameter.
- ``void T2::net::client::client(boost::asio::ip::tcp::socket&)``: A ``T2::net::client`` constructor that takes a **connected** ``boost::asio::ip::tcp::socket`` as a parameter. This constructor migrates the ``io_context`` of the supplied socket to the global ``T2::net::client::asio_context``.
- ⚠️ ``void T2::net::client::connect()``: Connects to the endpoint that the client was constructed for. If this member function is called whilst connected to an endpoint, an exception will be thrown.
- ⚠️ ``void T2::net::client::disconnect()``: Disconnects from a connected endpoint. If this member function is called whilst already disconnected, an exception will be thrown.
- ⚠️ ``void T2::net::client::send_data(const boost::asio::const_buffer&)``: An internal wrapper for ``T2::net::client::send_data_base`` that passes the client's (private) socket.
- ⚠️ ⚡️ ``void T2::net::client::send_data_base(boost::asio::ip::tcp::socket&, const boost::asio::const_buffer&)``: Sends the given amount of data over the provided socket, throwing an exception if it is unable to send all of the data in one boost.asio ``send()`` call.
- ⚠️ ``size_t T2::net::client::receive_data(boost::asio::mutable_buffer&)``: An internal wrapper for ``T2::net::client::receive_data_base`` that passes the client's (private) socket.
- ⚠️ ⚡️ ``size_t T2::net::client::receive_data_base(boost::asio::ip::tcp::socket&, boost::asio::mutable_buffer&)``: Receives (at most, the size of the buffer passed) bytes from the specified socket. This function eturns zero (0) in the event of a timeout, the amount of bytes received if no error has occured, and throws an exception in the event of an error.
- ``void T2::net::client::~client()``: The ``T2::net::client`` destructor that disconnects if the socket is active and if appropriate, calling ``T2::net::client::retire()``.
- ⚡️ ``void T2::net::client::asio_loop()``: Unless you're extending this library you'll never need to interact with this function but it is worth knowing that it is the main handler/processor of 'io requests', the thread that this *blocking* function runs under is the one that recursively runs operations from a vector and passes back return values, disposing (via ``delete``) of objects when appropriate.

The ``T2::net::client`` class works to integrate timeouts in boost's API in addition to some bonus sanity checks, this is achieved by calling ``async_xxx`` functions and then using the current thread (say, the one that is executing ``T2::net::client::receive_data_base``) to run a timer (``T2::utils::blocking_timer``) and then cancelling the request if it has timed out. The thread that runs all of the client's I/O requests is created by the ``T2::net::client::initialization`` function and iterates over a list of requests (which are dynamically allocated and are submitted to ``T2::net::client::asio_loop`` which does all of this processing) and calls their respective async functions, once the ``async_xxx`` function has returned, the thread will set a flag that indicates that ``T2::utils::blocking_timer`` can return - once this has happened (or the timer returned due to a timeout), the thread that initially allocated the request will use the information from the request object, and then set a disposal flag that indicates that on its next iteration of all I/O requests, the ``T2::net::client::asio_loop`` thread can dispose of the object (``delete``).

### Server

- ``void T2::net::server::server(const uint16_t)``: Constructs a server object by plainly setting the ``const`` private member 'port' to the provided value.
- ``void T2::net::server::start_listening(std::function<void(T2::net::client* const)>)``: A wrapper to ``T2::net::server::start_listening``.
- ⚠️ ``void T2::net::server::start_listening(std::vector<std::function<void(T2::net::client* const)>>, const bool)``: Launches a ``T2::net::server::listen_loop`` on a separate thread. IIRC, the boolean parameter is for a user to indicate that they are going to be calling this function rapidly across ``T2::net::server`` instances, it simply embeds a fixed-duration thread sleep. I think that the use of this flag was to avoid a race condition that has since been resolved. An exception will be thrown if the server is already listening.
- ⚡️ ``void T2::net::server::listen_loop(std::vector<std::function<void(T2::net::client* const)>>)``: A private function that operates as the listener for the server. This function is currently launched as a separate thread, it takes a list (``std::vector``) of the anonymous functions that should be called when a new connection is established.
- ⚠️ ``void T2::net::server::stop_listening(bool)``: Cleanly stops the ``T2::net::server::listen_loop`` thread by setting a flag. This function throws an exception if the server isn't already listening.

***Note: Do not share one ``T2::net::client`` or ``T2::net::server`` instance across multiple threads if concurrent access is a possibility. These classes were not designed to surmount race conditions that would occur in those instances.***

### Compilation
The ``_DEBUG`` macro can be specified in order to enable debug outputs via ``std::clog`` and ``std::cerr``.

Example usage of this library is available in the ``example/`` directory, compilation instructions for ``clang++`` can be found at the top of those files, it should be fairly trivial to convert them to their MSVC counterparts as there are no OS-specific flags/options used.

For linux users, ``build-library.sh`` is available to compile the library into a ``.a`` archive file which can then be linked against.

The T2-lib headers can be used in your project as long as you link their respective C++ files and add a path to boost in your include search-list. A list of the current C++ files can be found below (starting from base directory ``source/``):
```
T2/utility/utility.cpp T2/net/client.cpp T2/net/server.cpp
```

## Security
Whilst the overall security and integrity of this library isn't guaranteed, it *is* guaranteed that the testing tool (``tests/tests.cpp``) is exploitable if allowed to run under arbitrary command line arguments that weren't chosen by you.
If you identify a possible vulnerability in this code please contact me via ``michaellrowley(@)protonmail.com``.
