//
//  main.cpp
//  http-json
//
//  Created by Corey Ferguson on 1/28/26.
//

#include "http.h"
#include "json.h"
#include "socket.h"
#include "url.h"

using namespace http;
using namespace json;
using namespace mysocket;
using namespace std;

// Typedef

struct service {
    // Member Functions

    string greeting(header::map headers, class request request) {
        try {
            object* options = parse(request.body());
            object* first_name = options->get("firstName");
            object* last_name = options->get("lastName");
            
            if (first_name == NULL || first_name->value().empty())
                throw std::runtime_error("missing required parameter 'firstName'");
            
            string body = "Hello, " + decode(first_name->value());
            
            if (last_name != NULL && last_name->value().length())
                body += " " + decode(last_name->value());
            
            body += "!";

            headers["Content-Type"] = string("text/plain; charset=utf-8");
            
            return response(body, headers);
        } catch (std::exception e) {
            headers["Content-Type"] = string("application/json");
            
            return response(stringify(new object((vector<object*>){
                new object("message", encode(e.what())),
                new object("status", to_string(400))
            })), headers);
        }
    }

    string ping(header::map headers) {
        headers["Content-Type"] = string("text/plain; charset=utf-8");

        return response("Hello, world!", headers);
    }
};

// Non-Member Fields

const size_t PORT = 8080;

atomic<bool> _alive = true;
set<string>  _allow = { "GET", "HEAD", "PUT", "PATCH", "POST", "DELETE" };
header::map  _headers = {
    { "Access-Control-Allow-Origin", "*" },
    { "Connection", "keep-alive" },
    { "Keep-Alive", "" }
};
mutex        _mutex;
tcp_server*  _server = NULL;
service      _service;

// Non-Member Functions

// HTTP/1.1 default
size_t keep_alive_timeout() {
    return 5;
}

// Optional; assign a value < 0 to disable
int keep_alive_max() {
    return 200;
}

set<string> allow() {
    set<string> temp;

    _mutex.lock();

    temp = _allow;

    _mutex.unlock();

    return temp;
}

header::map headers() {
    header::map temp;

    _mutex.lock();

    temp = _headers;

    _mutex.unlock();

    return temp;
}

void log_request(class request request) {
    cout << "url: " << request.url() << ", body: " << (request.body().empty() ? "null" : request.body()) << endl;
}

string handle_request(header::map headers, class request request) {
    auto options = [](header::map headers) {
        headers["Access-Control-Allow-Methods"] = allow();

        return response(204, "No Content", "", headers);
    };
    
    auto not_found = [request]() {
        throw http::error(404, "Cannot " + toupperstr(request.method()) + " " + request.url());
        return "";
    };
    
    string url = request.url(),
           prefix = "/api";
    
    if (starts_with(url, prefix)) {
        url = url.substr(prefix.length());
        
        if (url == "/ping") {
            if (request.method() == "options")
                return options(headers);

            if (request.method() == "get") {
                log_request(request);
                
                return _service.ping(headers);
            }

            return not_found();
        }
        
        if (url == "/greeting") {
            if (request.method() == "options")
                return options(headers);

            if (request.method() == "post") {
                log_request(request);

                return _service.greeting(headers, request);
            }

            return not_found();
        }
        
        return not_found();
    }

    return not_found();
}

void initialize() {
    // Preserve comma-separated header values' order
    vector<string> ka = { join({ "timeout", to_string(keep_alive_timeout()) }, "=") };

    if (keep_alive_max() > 0)
        ka.push_back(join({ "max", to_string(keep_alive_max()) }, "="));

    _headers["Keep-Alive"] = join(ka, ",");
}

// Perform garbage collection
void onsignal(int signum) {
    thread([]() {
        cout << endl;
        cout << "Stop? (y/N) ";

        string str;

        getline(cin, str);

        if (tolowerstr(str) == "y") {
            _server->close();
            _alive.store(false);
        }
    }).detach();
}

int main(int argc, const char* argv[]) {
    int port;

    if (argc == 1)
        port = PORT;
    else {
        port = parse_int(argv[1]);

        if (port < 3000)
            port = PORT;
    }   

    initialize();

    while (true) {
        try {
            _server = new tcp_server(port, [](tcp_server::connection* connection) {
                // Handle request in its own thread
                thread([connection]() {
                    // Number of requests received
                    atomic<size_t> requestc = 0;

                    // Set connection timeout
                    thread([&requestc, connection]() {
                        for (size_t i = 0; i < http::timeout() && !requestc.load(); i++)
                            this_thread::sleep_for(chrono::milliseconds(1000));

                        if (requestc.load())
                            return;

                        requestc.store(1);
                        connection->close();
                    }).detach();

                    // Wait for non-empty request
                    while (true) {
                        try {
                            string request = connection->recv();

                            if (request.empty())
                                continue;

                            requestc.fetch_add(1);

                            auto handle_response = [connection](const string response) {
#if LOGGING == LEVEL_DEBUG
                                cout << response << endl;
#endif

                                connection->send(response);
                            };

                            try {
                                class request request_obj = parse_request(request);

                                if (request_obj.headers()["host"].str().length()) {
                                    auto next = [&]() {
                                        try {
                                            string response = handle_request(headers(), request_obj);

                                            handle_response(response);

                                            size_t request_id = requestc.load();

                                            if (request_id >= keep_alive_max()) {
                                                connection->close();
                                                
                                                return true;
                                            }

                                            // Keep alive
                                            thread([request_id, &requestc, connection]() {
                                                for (int i = 0; i < keep_alive_timeout() && request_id == requestc.load(); i++)
                                                    this_thread::sleep_for(chrono::milliseconds(1000));

                                                if (request_id == requestc.load())
                                                    connection->close();
                                            }).detach();
                                        } catch (http::error& e) {
                                            handle_response(response(e.status(), e.status_text(), e.text(), headers()));
                                        }
                                        
                                        return false;
                                    };

                                    std::string method = toupperstr(request_obj.method());
                                    
                                    if (method == "OPTIONS") {
                                        if (next())
                                            return;
                                    } else {
                                        set<string> allow = ::allow();
                                        
                                        if (allow.find(method) == allow.end())
                                            throw http::error(400);
                                            
                                        if (next())
                                            return;
                                    }
                                } else {
                                    handle_response(response(400, "Bad Request", to_string(0), {
                                        { "Connection", "close" },
                                        { "Transfer-Encoding", "chunked "}
                                    }));

                                    return connection->close();
                                }
                            } catch (http::error& e) {
                                handle_response(response(400, "Bad Request", e.text(), {
                                    { "Connection", "close" }
                                }, false));
                        
                                return connection->close();
                            }
                        } catch (mysocket::error& e) {
                            // Connection timed out; suppress error
                            if (requestc.load())
                                return;

                            throw e;
                        }
                    }
                }).detach();
            });
            
            signal(SIGINT, onsignal);
            signal(SIGTERM, onsignal);

            cout << "Server listening on port " << port << "...\n";

            while (_alive.load())
                continue;

            break;
        } catch (mysocket::error& e) {
            // EADDRINUSE
            if (e.errnum() == 48)
                port++;
            else
                throw e;
        }
    }
}
