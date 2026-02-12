//
//  main.cpp
//  http-json
//
//  Created by Corey Ferguson on 1/28/26.
//

#include "http.h"
#include "json.h"
#include "logger.h"
#include "service.h"
#include "socket.h"
#include "url.h"
#include <any>

using namespace http;
using namespace json;
using namespace mysocket;
using namespace std;

// Non-Member Fields

header::map  _headers = {
    { "Accept", "application/json" },
    { "Access-Control-Allow-Origin", "*" },
    { "Connection", "keep-alive" },
    { "Keep-Alive", 0 },
};

int          _port = 8080;

atomic<bool> _alive = true;
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

any sync(std::function<any(void)> cb) {
    _mutex.lock();
    
    any result = cb();
    
    _mutex.unlock();
    
    return result;
}

set<string> allow_methods() {
    return { "GET", "HEAD", "PUT", "PATCH", "POST", "DELETE" };
}

header::map headers() {
    return any_cast<header::map>(sync([]() {
        return _headers;
    }));
}

void log_request(class request request) {
    logger::info("url: " + request.url() + ", body: " + (request.body().empty() ? null() : request.body()));
}

string handle_request(header::map headers, class request request) {
    auto options = [](header::map headers) {
        headers["Access-Control-Allow-Methods"] = allow_methods();

        return response(NO_CONTENT, strstatus(NO_CONTENT), "", headers);
    };
    
    auto not_found = [request, &headers]() {
        headers["Content-Type"] = string("text/plain; charset=utf-8");
        
        return response(NOT_FOUND, strstatus(NOT_FOUND), "Cannot " + toupperstr(request.method()) + " " + request.url(), headers);
    };
    
    string url = request.url(),
            url_prefix = "/api";
    
    if (starts_with(url, url_prefix)) {
        url = url.substr(url_prefix.length());
        
        if (url == "/greeting") {
            if (request.method() == "options") {
                headers["Accept"] = string("application/json");

                return options(headers);
            }
            
            auto greeting = [request, headers]() {
#if LOGGING
                log_request(request);
#endif

                return _service.greeting(headers, request);
            };
            
            if (request.method() == "head") {
                greeting();
                
                return response(NO_CONTENT, strstatus(NO_CONTENT), "", headers);
            }

            if (request.method() == "post")
                return greeting();

            return not_found();
        }
        
        if (url == "/ping") {
            if (request.method() == "options")
                return options(headers);
            
            auto ping = [request, headers]() {
#if LOGGING
                log_request(request);
#endif
                
                return _service.ping(headers);
            };
            
            if (request.method() == "head") {
                ping();
                
                return response(NO_CONTENT, strstatus(NO_CONTENT), "", headers);
            }

            if (request.method() == "get")
                return ping();

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
    if (argc != 1) {
        _port = parse_int(argv[1]);

        if (_port < 3000)
            _port = 3000;
    }

    initialize();

    while (true) {
        try {
            _server = new tcp_server(_port, [](tcp_server::connection* connection) {
                // Number of requests received
                atomic<size_t> nrequests = 0;
                
                // Handle request in its own thread
                thread([&nrequests, connection]() {
                    // Set connection timeout
                    thread([&nrequests, connection]() {
                        for (size_t i = 0; i < http::timeout() && !nrequests.load(); i++)
                            this_thread::sleep_for(chrono::milliseconds(1000));

                        if (nrequests.load())
                            return;

                        nrequests.store(1);
                        connection->close();
                    }).detach();

                    // Wait for non-empty request
                    while (true) {
                        try {
                            string request = connection->recv();

                            if (request.empty())
                                continue;

                            nrequests.fetch_add(1);

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
                                        string response = handle_request(headers(), request_obj);

                                        handle_response(response);

                                        size_t nrequest = nrequests.load();

                                        if (nrequest >= keep_alive_max()) {
                                            connection->close();
                                            
                                            return true;
                                        }

                                        // Keep alive
                                        thread([nrequest, &nrequests, connection]() {
                                            for (int i = 0; i < keep_alive_timeout() && nrequest == nrequests.load(); i++)
                                                this_thread::sleep_for(chrono::milliseconds(1000));

                                            if (nrequest == nrequests.load())
                                                connection->close();
                                        }).detach();
                                        
                                        return false;
                                    };

                                    string method = toupperstr(request_obj.method());
                                    
                                    if (method == "OPTIONS") {
                                        if (next())
                                            return;
                                    } else {
                                        if (allow_methods().find(method) == allow_methods().end())
                                            throw http::error(BAD_REQUEST);
                                            
                                        if (next())
                                            return;
                                    }
                                } else {
                                    handle_response(response(BAD_REQUEST, strstatus(BAD_REQUEST), to_string(0), {
                                        { "Connection", "close" },
                                        { "Transfer-Encoding", "chunked "}
                                    }));

                                    return connection->close();
                                }
                            } catch (http::error& e) {
                                handle_response(response(BAD_REQUEST, strstatus(BAD_REQUEST), e.text(), {
                                    { "Connection", "close" }
                                }, false));
                        
                                return connection->close();
                            }
                        } catch (mysocket::error& e) {
                            // Connection timed out; suppress error
                            if (nrequests.load())
                                return;

                            throw e;
                        }
                    }
                }).detach();
            });
            
            signal(SIGINT, onsignal);
            signal(SIGTERM, onsignal);

            cout << "Server listening on port " << _port << "...\n";

            while (_alive.load())
                continue;

            break;
        } catch (mysocket::error& e) {
            // EADDRINUSE
            if (e.errnum() == 48)
                _port++;
            else
                throw e;
        }
    }
}
