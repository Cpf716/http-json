//
//  socket.cpp
//  http-json
//
//  Created by Corey Ferguson on 1/28/26.
//

#include "socket.h"

namespace mysocket {
    // Non-Member Functions

    std::string _recv(const int file_descriptor) {
        char buff[1024] = {0};
            
        ssize_t len = recv(file_descriptor, buff, 1024, 0);

        if (len == -1)
            throw mysocket::error(errno);

        buff[len] = '\0';
        
        return trim_end(std::string(buff));
    }

    int _send(const int file_descriptor, const std::string message) {
        ssize_t len = ::send(file_descriptor, message.c_str(), message.length(), MSG_NOSIGNAL);
            
        if (len == -1)
            throw mysocket::error(errno);
        
        return (int)len;
    }

    // Constructors

    tcp_server::connection::connection(tcp_server* parent, const int file_descriptor) {
        this->_parent = parent;
        this->_file_descriptor = file_descriptor;
    }

    error::error(const int errnum) {
        this->_errnum = errnum;
        this->_what = std::strerror(this->_errnum);
    }

    tcp_client::tcp_client(const std::string host, const int port) {
        this->_file_descriptor = ::socket(AF_INET, SOCK_STREAM, 0);
            
        if (this->_file_descriptor == -1)
            throw mysocket::error(errno);
        
        struct sockaddr_in addr;

        // IPv4
        addr.sin_family = AF_INET;

        // Convert port to network byte order
        addr.sin_port = htons(port);
        
        if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) == -1)
            throw mysocket::error(errno);
        
        // Returns 0 for success, -1 otherwise
        if (connect(this->_file_descriptor, (struct sockaddr *)&addr, sizeof(addr))) {
            ::close(this->_file_descriptor);
            
            throw mysocket::error(errno);
        }
    }

    tcp_server::tcp_server(const int port, const int backlog) {
        this->_file_descriptor = ::socket(AF_INET, SOCK_STREAM, 0);
            
        if (this->_file_descriptor == -1)
            throw mysocket::error(errno);
                
        int opt = 1;
        
        if (setsockopt(this->_file_descriptor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
            ::close(this->_file_descriptor);
            
            throw mysocket::error(errno);
        }
            
        // Listen for any IP address
        this->_address.sin_addr.s_addr = INADDR_ANY;

        // IPv4
        this->_address.sin_family = AF_INET;

        // Convert port to network byte order
        this->_address.sin_port = htons(port);
        this->_address_length = sizeof(this->_address);
        
        if (bind(this->_file_descriptor, (struct sockaddr *)&this->_address, this->_address_length)) {
            ::close(this->_file_descriptor);
            
            throw mysocket::error(errno);
        }
        
        if (listen(this->_file_descriptor, backlog)) {
            ::close(this->_file_descriptor);
            
            throw mysocket::error(errno);
        }
        
        this->_listener = std::thread([&]{
            while (true) {
                if (this->_shut_down.load())
                    return;
                
                // Returns nonnegative file descriptor or -1 for error
                int file_descriptor = accept(
                    this->_file_descriptor,
                    (struct sockaddr *)&this->_address, (socklen_t *)&this->_address_length);
                
                if (file_descriptor == -1)
                    continue;

                class connection* connection = new class connection(this, file_descriptor);
                
                this->_mutex.lock();
                this->_connections.push_back(connection);
                
                // Sort into place
                for (size_t i = this->_connections.size() - 1; i > 0 && this->_connections[i] < this->_connections[i - 1]; i--)
                    std::swap(this->_connections[i], this->_connections[i - 1]);
                
                this->_mutex.unlock();
                this->_handler(connection);
            }
        });
    }

    tcp_server::tcp_server(const int port, const std::function<void(connection*)> handler, const int backlog): tcp_server(port, backlog) {
        this->_handler = handler;
    }

    udp_client::udp_client(const std::string host, const int port) {
        this->_file_descriptor = ::socket(AF_INET, SOCK_DGRAM, 0);
            
        if (this->_file_descriptor == -1)
            throw mysocket::error(errno);
        
        this->_address = new struct sockaddr_in();
        
        memset(this->_address, 0, sizeof(* this->_address));
        
        // IPv4
        this->_address->sin_family = AF_INET;

        // Convert port to network byte order
        this->_address->sin_port = htons(port);
        
        if (inet_pton(AF_INET, host.c_str(), &this->_address->sin_addr) == -1) {
            ::close(this->_file_descriptor);
            
            throw mysocket::error(errno);
        }
    }

    udp_server::udp_server(const int port) {
        this->_file_descriptor = ::socket(AF_INET, SOCK_DGRAM, 0);
            
        if (this->_file_descriptor == -1)
            throw mysocket::error(errno);
        
        // Server
        struct sockaddr_in addr;
        
        memset(&addr, 0, sizeof(addr));
        
        // Listen for any IP address
        addr.sin_addr.s_addr = INADDR_ANY;

        // IPv4
        addr.sin_family = AF_INET;

        // Convert port to network byte order
        addr.sin_port = htons(port);
        
        if (bind(this->_file_descriptor, (const struct sockaddr *)&addr, sizeof(addr))) {
            ::close(this->_file_descriptor);
            
            throw mysocket::error(errno);
        }
        
        // Client
        this->_address = new struct sockaddr_in();
        
        memset(this->_address, 0, sizeof(* this->_address));
    }

    tcp_server::connection::~connection() { }

    tcp_client::~tcp_client() { }

    tcp_server::~tcp_server() { }

    udp_client::~udp_client() { }

    udp_server::~udp_server() { }

    // Member Functions

    void tcp_server::connection::_close() {
        if (::close(this->_file_descriptor))
            throw mysocket::error(errno);

        delete this;
    }

    int tcp_server::_find_connection(const class connection* connection) {
        return this->_find_connection(connection, 0, this->_connections.size());
    }

    int tcp_server::_find_connection(const class connection* connection, const size_t start, const size_t end) {
        if (start == end)
            return -1;
        
        size_t len = floor((end - start) / 2);
        
        if (this->_connections[start + len] == connection)
            return (int)(start + len);
        
        if (this->_connections[start + len] > connection)
            return this->_find_connection(connection, start, start + len);
        
        return this->_find_connection(connection, start + len + 1, end);
    }

    void tcp_server::connection::close() {
        this->_parent->close(this);
    }

    void tcp_client::close() {
        if (::close(this->_file_descriptor)) 
            throw mysocket::error(errno);

        delete this;
    }

    void tcp_server::close() {
        this->_shut_down.store(true);
        this->_mutex.lock();
        
        if (::close(this->_file_descriptor)) {
            this->_mutex.unlock();

            throw mysocket::error(errno);
        }
        
        this->_listener.join();

        for (size_t i = 0; i < this->_connections.size(); i++) {
            if (::close(this->_connections[i]->_file_descriptor)) {
                this->_mutex.unlock();

                throw mysocket::error(errno);
            }
                
            delete this->_connections[i];
        }

        this->_mutex.unlock();
        
        delete this;
    }

    void udp_socket::close() {
        if (::close(this->_file_descriptor))
            throw mysocket::error(errno);
        
        delete this->_address;
        delete this;
    }

    void tcp_server::close(class connection* connection) {
        this->_mutex.lock();
        
        int index = this->_find_connection(connection);

        if (index == -1)
            return this->_mutex.unlock();

        try {
            connection->_close();
        } catch (mysocket::error& e) {
            this->_mutex.unlock();

            throw e;
        }

        this->_connections.erase(this->_connections.begin() + index);
        this->_mutex.unlock();
    }

    std::vector<tcp_server::connection*> tcp_server::connections() {
        this->_mutex.lock();

        std::vector<connection*> temp = this->_connections;

        this->_mutex.unlock();

        return temp;
    }

    int error::errnum() const {
        return this->_errnum;
    }

    std::string tcp_server::connection::recv() const {
        return _recv(this->_file_descriptor);
    }

    std::string tcp_client::recv() const {
        return _recv(this->_file_descriptor);
    }

    std::string udp_socket::recvfrom() const {
        char      buff[1024];
        socklen_t addrlen = sizeof(* this->_address);
        ssize_t   len = ::recvfrom(this->_file_descriptor, (char *)buff, 1024, MSG_WAITALL, (struct sockaddr *)this->_address, &addrlen);
        
        if (len == -1)
            throw mysocket::error(errno);
        
        buff[len] = '\0';
        
        return std::string(buff);
    }

    int tcp_server::connection::send(const std::string message) const {
        return _send(this->_file_descriptor, message);
    }

    int tcp_client::send(const std::string message) const {
        return _send(this->_file_descriptor, message);
    }

    int udp_socket::sendto(const std::string message) const {
        ssize_t len = ::sendto(this->_file_descriptor, (const char *)message.c_str(), message.length(), 0, (const struct sockaddr *)this->_address, sizeof(* this->_address));
        
        if (len == -1)
            throw mysocket::error(errno);
        
        return (int)len;
    }

    const char* error::what() const throw() {
        return this->_what.c_str();
    }
}
