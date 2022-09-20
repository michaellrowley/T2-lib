# T2-Lib
This project is an object-oriented wrapper around [boost](https://www.boost.org/)'s networking functionality (mostly stemming from the ``asio`` namespace). It requires that you have access to boost's ``asio``, ``endian``, and ``lexical_cast`` packages/libraries for compilation.

## Documentation
There are two primary classes that are used to communicate with other devices - a ``T2::net::client`` which initiates a connection to a remote endpoint that is already listening for a new connection, and a ``T2::net::server`` which listens on a given port and calls specific connection handlers when a new connection is established on that port.

### Client
*most* of the time you'll want to create an object of class/server type when interacting with this library however in some limited cases, the use of exposed static functions may be more suitable or even required:

- ``T2::net::client::initialization()``: Responsible for ensuring that future client objects can run smoothly by using the same ``io_context`` for all clients. **You almost certainly don't need to call this _*private*_ function** because the ``T2::net::client::client()`` constructor does it by default.
- ``T2::net::client::retire()``: Responsible for cleaning up the ``io_context`` thread. Again, this is private to the ``T2::net::client`` class and will be called automatically when the count of active ``T2::net::client`` objects is zero.

### Server

### Compilation
The ``_DEBUG`` macro can be specified in order to enable debug outputs via ``std::clog`` and ``std::cerr``.

Example usage of this library is available in the ``example/`` directory, compilation instructions for ``clang++`` can be found at the top of those files, it should be fairly trivial to convert them to MSVC instructions as there are no OS-specific flags/options used.

For linux users, ``build-library.sh`` is available to compile the library into a ``.a`` archive file which can then be linked against.

The T2-lib headers can be used in your project as long as you link their respective C++ files and add a path to boost in your include search-list.

## Security
Whilst the overall security and integrity of this library isn't guaranteed, it *is* guaranteed that the testing tool (``tests/tests.cpp``) is exploitable if allowed to run under arbitrary command line arguments that weren't chosen by you.