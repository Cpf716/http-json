//
//  http.cpp
//  http-json
//
//  Created by Corey Ferguson on 1/28/26.
//

#include "http.h"

namespace http {
    // Non-Member Functions

    std::string strstatus(const status_code status) {
        switch (status) {
            case UNKNOWN_ERROR:
                return "Unknown error";
            case OK:
                return "OK";
            case NO_CONTENT:
                return "No Content";
            case FOUND:
                return "Found";
            case TEMPORARY_REDIRECT:
                return "Temporary Redirect";
            case PERMANENT_REDIRECT:
                return "Permanent Redirect";
            case BAD_REQUEST:
                return "Bad Request";
            case UNAUTHORIZED:
                return "Unauthorized";
            case NOT_FOUND:
                return "Not Found";
            case INTERNAL_SERVER_ERROR:
                return "Internal Server Error";
            default:
                break;
        }
        
        return "";
    }

    std::string http_version() {
        return "HTTP/1.1";
    }

    size_t timeout() {
        return 30;
    }

    request parse_request(const std::string message) {
        std::istringstream iss(message);
        std::string        line;

        getline(iss, line);

        std::vector<std::string> tokens = ::tokens(line);

        if (!(tokens.size() == 3 && tokens[2] == http_version()))
            throw http::error(BAD_REQUEST);

        std::string method = tolowerstr(tokens[0]),
                     target = tokens[1];
        header::map headers;

        while (getline(iss, line)) {
            std::vector<std::string> header = split(line, ":");

            if (header.size() == 1)
                break;

            headers[tolowerstr(header[0])] = trim(line.substr(header[0].length() + 1));
        }

        auto        it = headers.find("content-length");
        std::string body = "";

        if (it != headers.end()) {
            std::vector<std::string> value;

            while (getline(iss, line))
                value.push_back(trim_end(line));

            body = join(value, "\r\n");
            body = body.substr(0, std::min((int) body.length(), (int) (* it).second));
        }

        return request(method, target, headers, body);
    }

    std::string redirect(header::map& headers, const status_code status, const std::string location) {
        headers["Location"] = location;

        return response(status, strstatus(status) + ". Redirecting to " + location, headers);
    }

    std::string redirect(header::map& headers, const std::string location) {
        return redirect(headers, FOUND, location);
    }

    std::string response(const std::string text, header::map headers) {
        return response(OK, text, headers);
    }

    std::string response(const status_code status, const std::string text, header::map headers, const bool date) {
        std::ostringstream ss(http_version() + " ");

        ss.seekp(0, std::ios::end);

        ss << std::to_string(status) << " " << strstatus(status) << "\r\n";

        // Response headers
        if (date) {
            time_t now = time(0);
            tm*    gmtm = gmtime(&now);
            char*  dt = asctime(gmtm);
            
            std::vector<std::string> tokens = ::tokens(std::string(dt));

            ss << "Date" << ": ";

            std::string day = tokens[0],
                         month = tokens[1],
                         date = tokens[2],
                         time = tokens[3],
                         year = tokens[4];

            ss << day << ", " << date << " " << month << " " << year << " " << time << " GMT";
        }

        for (const auto& [key, value]: headers) {
            ss << "\r\n";
            ss << key << ": " << value.str();
        }
        
        if (text.length()) {
            if (headers.find("Transfer-Encoding") == headers.end()) {
                ss << "\r\n";
                ss << "Content-Length: " << text.length();
            }

            ss << "\r\n\r\n";
            ss << text;
        }
        
        return ss.str();
    }

    // Constructors

    error::error(const status_code status) {
        this->_status = status;
        this->_status_text = strstatus(this->status());
        this->_text = "";
    }

    error::error(const status_code status, const std::string text) {
        this->_status = status;
        this->_status_text = strstatus(this->status());
        this->_text = text;
    }

    header::header() {
        this->_set("");
    }

    header::header(const char* value) {
        this->_set(std::string(value));
    }

    header::header(const int value) {
        this->_set(value);
    }

    header::header(const std::string value) {
        this->_set(value);
    }

    header::header(std::set<std::string> value) {
        this->_set(value);
    }

    request::request(const std::string method, const std::string url, header::map headers, const std::string body) {
        this->_method = method;

        class url url_obj(url);

        this->_url = url_obj.target();
        this->_params = url_obj.params();
        this->_headers = headers;
        this->_body = body;
    }

    // Operators

    header::operator int() {
        return this->int_value();
    }

    header::operator std::string() {
        return this->str();
    }

    header::operator std::set<std::string>() {
        return this->list();
    }

    int header::operator=(const int value) {
        return this->_set(value);
    }

    std::string header::operator=(const std::string value) {
        return this->_set(value);
    }

    std::set<std::string> header::operator=(std::set<std::string> value) {
        return this->_set(value);
    }

    bool header::operator==(const char* value) {
        return this->str() == std::string(value);
    }

    bool header::operator==(const header value) {
        return this->str() == value.str();
    }

    bool header::operator==(const int value) {
        return this->int_value() == value;
    }

    bool header::operator==(const std::string value) {
        return this->str() == value;
    }

    bool header::operator!=(const char* value) {
        return !(*this == value);
    }

    bool header::operator!=(const header value) {
        return !(*this == value);
    }

    bool header::operator!=(const int value) {
        return !(*this == value);
    }

    bool header::operator!=(const std::string value) {
        return !(*this == value);
    }

    // Member Functions

    int header::_set(const int value) {
        this->_int = value;
        this->_str = std::to_string(this->int_value());
        this->_list = { this->str() };

        return this->int_value();
    }

    std::string header::_set(const std::string value) {
        this->_str = value;
        this->_int = parse_int(this->str());
        
        this->_list.clear();
        
        for (std::string item: split(value, ","))
            this->_list.insert(trim(item));

        return this->str();
    }

    std::set<std::string> header::_set(std::set<std::string> value) {
        this->_list = value;
        
        std::vector<std::string> list;
        
        for (std::string item: this->list())
            list.push_back(item);
        
        this->_str = join(list, ",");
        this->_int = INT_MIN;

        return this->list();
    }

    std::string request::body() const {
        return this->_body;
    }

    header::map request::headers() {
        return this->_headers;
    }

    int header::int_value() const {
        return this->_int;
    }

    std::set<std::string> header::list() {
        return this->_list;
    }

    std::string request::method() const {
        return this->_method;
    }

    url::param::map request::params() {
        return this->_params;
    }

    status_code error::status() const {
        return this->_status;
    }

    std::string error::status_text() const {
        return this->_status_text;
    }

    std::string header::str() const {
        return this->_str;
    }

    std::string error::text() const {
        return this->_text;
    }

    std::string request::url() const {
        return this->_url;
    }

    const char* error::what() const throw() {
        return this->_text.c_str();
    }
}
