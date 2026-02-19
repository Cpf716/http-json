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
    { "X-Powered-By", "http-json" }
};

int          _port = 8080;

atomic<bool> _alive = true;
logger       _logger;
mutex        _mutex;
tcp_server*  _server = NULL;
service      _service;

// Non-Member Functions

set<string> allow_methods() {
    return { "GET", "HEAD", "PUT", "PATCH", "POST", "DELETE" };
}

void log_request(class request request) {
    _logger.info("url: " + request.url() + ", body: " + (request.body().empty() ? null() : request.body()));
}

string handle_request(header::map headers, class request request) {
    auto options = [](header::map headers) {
        headers["Access-Control-Allow-Methods"] = allow_methods();

        return response(NO_CONTENT, "", headers);
    };
    
    auto not_found = [request, &headers]() {
        headers["Content-Type"] = string("text/plain; charset=utf-8");
        
        return response(NOT_FOUND, "Cannot " + toupperstr(request.method()) + " " + request.url(), headers);
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
                log_request(request);

                return _service.greeting(headers, request);
            };
            
            if (request.method() == "head") {
                greeting();
                
                return response(NO_CONTENT, "", headers);
            }

            if (request.method() == "post")
                return greeting();

            return not_found();
        }
        
        if (url == "/ping") {
            if (request.method() == "options")
                return options(headers);
            
            auto ping = [request, headers]() {
                log_request(request);
                
                return _service.ping(headers);
            };
            
            if (request.method() == "head") {
                ping();
                
                return response(NO_CONTENT, "", headers);
            }

            if (request.method() == "get")
                return ping();

            return not_found();
        }
        
        return not_found();
    }

    return not_found();
}

auto sync(auto cb) {
    _mutex.lock();
    
    auto result = cb();
    
    _mutex.unlock();
    
    return result;
}

header::map headers() {
    return sync([]() {
        return _headers;
    });
}

// Optional; assign a value < 0 to disable
int keep_alive_max() {
    return 200;
}

// HTTP/1.1 default
size_t keep_alive_timeout() {
    return 5;
}

void initialize() {
    // Preserve comma-separated header values' order
    vector<string> keep_alive = { join({ "timeout", to_string(keep_alive_timeout()) }, "=") };

    if (keep_alive_max() > 0)
        keep_alive.push_back(join({ "max", to_string(keep_alive_max()) }, "="));

    _headers["Keep-Alive"] = join(keep_alive, ",");
}

// Perform garbage collection
void onsignal(int signum) {
    thread([]() {
        cout << endl;
        cout << "Stop? (y/N) ";

        string line;

        getline(cin, line);

        if (tolowerstr(line) == "y") {
            _server->close();
            _alive.store(false);
        }
    }).detach();
}

enum logging parse_logging(const std::string value) {
    int index = ((map<string, int>) {
        { "none", 1 },
        { "info", 2 },
        { "extended", 3 }
    })[value] - 1;
    
    return index == -1 ? INFO : static_cast<enum logging>(index);
}

int main(int argc, const char* argv[]) {
    _logger.logging() = argc == 1 ? INFO : parse_logging(argv[1]);
    _service = service(_logger);

    initialize();

    bool flag = true;

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

                            _logger.extended(request + "\r\n");

                            nrequests.fetch_add(1);

                            auto handle_response = [connection](const string response) {
                                _logger.extended(response + "\r\n");

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
                                    handle_response(response(BAD_REQUEST, to_string(0), {
                                        { "Connection", "close" },
                                        { "Transfer-Encoding", "chunked "}
                                    }));

                                    return connection->close();
                                }
                            } catch (http::error& e) {
                                handle_response(response(BAD_REQUEST, e.text(), {
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

            return 0;
        } catch (mysocket::error& e) {
            // EADDRINUSE
            if (e.errnum() == 48) {
                if (flag) {
                    cout << "Port " << _port << " is already in use. Use a different port? (Y/n) ";

                    string line;

                    getline(cin, line);

                    if (tolowerstr(line) != "y")
                        return 0;

                    flag = false;
                }

                _port++;
            } else
                throw e;
        }
    }
}
