//
//  socket.h
//  http-json
//
//  Created by Corey Ferguson on 1/28/26.
//

#ifndef socket_h
#define socket_h

#include "util.h"
#include <arpa/inet.h>  // inet_ptons
#include <csignal>      // signal
#include <mutex>
#include <netinet/in.h> // sockaddr_in
#include <sys/socket.h> // socket
#include <thread>
#include <unistd.h>     // close, read

namespace mysocket {
    // Typedef
    
    struct error: public std::exception {
        // Constructors

        error(const int errnum);

        // Member Fields

        int         errnum() const;

        const char* what() const throw();
    private:
        // Member Fields

        int         _errnum = 0;
        std::string _what;
    };

    struct tcp_client {
        // Constructors

        tcp_client(const std::string host, const int port);

        // Member Functions

        void        close();

        std::string recv() const;

        int         send(const std::string message) const;
    private:
        // Constructors

        ~tcp_client();

        // Member Fields

        struct sockaddr_in* _address;
        int                 _file_descriptor;
    };

    struct tcp_server {
        // Typedef

        class connection {
            // Constructors

            connection(tcp_server* parent, const int file_descriptor);

            ~connection();

            // Member Fields

            int         _file_descriptor;
            tcp_server* _parent = NULL;

            // Member Functions

            void _close();
        public:
            // Typdef

            friend tcp_server;

            // Member Functions

            void        close();

            std::string recv() const;

            int         send(const std::string message) const;
        };

        // Constructors

        tcp_server(const int port, const int backlog = 1024);

        tcp_server(const int port, const std::function<void(connection*)> handler, const int backlog = 1024);

        // Member Functions

        void                     close();

        void                     close(class connection* connection);

        std::vector<connection*> connections();
    private:
        // Constructors

        ~tcp_server();

        // Member Fields

        struct sockaddr_in               _address;
        int                              _address_length;
        std::vector<connection*>         _connections;
        int                              _file_descriptor;
        std::function<void(connection*)> _handler = [](const class connection* connection){ };
        std::thread                      _listener;
        std::mutex                       _mutex;
        std::atomic<bool>                _shut_down = false;

        // Member Functions

        int _find_connection(const class connection* connection);

        int _find_connection(const class connection* connection, const size_t start, const size_t end);
    };

    struct udp_socket {
        // Member Functions

        void        close();

        std::string recvfrom() const;

        int         sendto(const std::string message) const;
    protected:
        // Member Fields

        struct sockaddr_in* _address = NULL;
        int                 _file_descriptor;
    };

    class udp_client: public udp_socket {
        // Constructors

        ~udp_client();
    public:
        // Constructors

        udp_client(const std::string host, const int port);
    };

    class udp_server: public udp_socket {
        // Constructors

        ~udp_server();
    public:
        // Constructors

        udp_server(const int port);
    };
}

#endif /* socket_h */
