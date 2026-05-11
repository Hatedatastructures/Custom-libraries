// SPDX-License-Identifier: MIT
#include "wan/network/network.hpp"
#include <boost/asio.hpp>
#include <iostream>

namespace asio = boost::asio;
using tcp = asio::ip::tcp;

int main() {
    try {
        asio::io_context io_ctx;
        tcp::acceptor acceptor(io_ctx, tcp::endpoint(tcp::v4(), 10086));
        std::cout << "Echo server listening on port 10086\n";

        for (;;) {
            tcp::socket socket(io_ctx);
            acceptor.accept(socket);
            std::string data(1024, '\0');
            auto len = socket.read_some(asio::buffer(data));
            asio::write(socket, asio::buffer(data, len));
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
