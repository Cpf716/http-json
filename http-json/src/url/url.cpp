//
//  url.cpp
//  http-json
//
//  Created by Corey Ferguson on 1/28/26.
//

#include "url.h"

// Non-Member Functions

std::map<std::string, int> protocols() {
    return {
        { "http", 80 },
        { "https", 443 }
    };
}

// Constructors

url::error::error(const std::string what) {
    this->_what = what;
}

url::param::param() {
    this->_set("");
}

url::param::param(const char* value) {
    this->_set(std::string(value));
}

url::param::param(const double value) {
    this->_set(value);
}

url::param::param(const std::string value) {
    this->_set(value);
}

url::param::param(const std::vector<std::string> value) {
    this->_set(value);
}

url::portinfo::portinfo() {
    this->_value = INT_MIN;
    this->_typed = false;
}

url::portinfo::portinfo(const int value, const bool typed) {
    this->_value = value;
    this->_typed = typed;
}

url::query_string::query_string() { }

url::query_string::query_string(param::map params) {
    this->_params = params;
}

url::query_string::query_string(const std::string value) {
    if (value.empty())
        throw error("Unexpected end of URL input");

    std::vector<std::string> query = split(value, "&");

    for (std::string param: query) {
        std::vector<std::string> tokens = split(param, "=");

        if (tokens.size() >= 3)
            throw error("Unexpected token = in URL");
        
        this->_params[tokens[0]] = tokens.size() == 1 ? "" : tokens[1];
    }
}

url::url() {
    this->_host = "";
    this->_protocol = "";
    this->_target = "";
}

url::url(std::string value) {
    std::vector<std::string> target = split(value, "?");
    
    if (target.size() >= 3 || target[0].empty())
        throw url::error("Unexpected token ? in URL");
        
    value = target[0];
        
    int start = 0;

    while (start < (int) value.length() - 1 && (value[start] != '/' || value[start + 1] != '/'))
        start++;

    if (start == value.length() - 1)
        start = 0;
    else {
        if (start)
            this->protocol() = tolowerstr(value.substr(0, start - 1));

        start += 2;
    }

    size_t end = start;

    while (end < value.length() && value[end] != '/')
        end++;
    
    std::vector<std::string> host = split(value.substr(start, end - start), ":");

    if (host.size() >= 3)
        throw url::error("Unexpected token : in URL");

    this->host() = host[0];
    this->port() = host.size() == 1 ? portinfo(protocols()[this->protocol()], false) : portinfo(parse_int(host[1]), true);
    this->target() = value.substr(end);

    if (target.size() == 2)
        this->params() = query_string(target[1]).params();
}

// Operators

url::param::operator double() {
    return this->number();
}

url::param::operator std::string() {
    return this->str();
}

url::param::operator std::vector<std::string>() {
    return this->list();
}

double url::param::operator=(const double value) {
    return this->_set(value);
}

std::string url::param::operator=(const std::string value) {
    return this->_set(value);
}

std::vector<std::string> url::param::operator=(const std::vector<std::string> value) {
    return this->_set(value);
}

bool url::param::operator==(const char* value) {
    return this->str() == std::string(value);
}

bool url::param::operator==(const param value) {
    return this->str() == value.str();
}

bool url::param::operator==(const double value) {
    return this->number() == value;
}

bool url::param::operator==(const std::string value) {
    return this->str() == value;
}

bool url::param::operator!=(const char* value) {
    return !(*this == value);
}

bool url::param::operator!=(const param value) {
    return !(*this == value);
}

bool url::param::operator!=(const double value) {
    return !(*this == value);
}

bool url::param::operator!=(const std::string value) {
    return !(*this == value);
}

url::portinfo::operator int() {
    return this->value();
}

// Member Functions

const char* url::error::what() const throw() {
    return this->_what.c_str();
}

double url::param::_set(const double value) {
    this->_number = value;
    this->_str = std::to_string(this->number());
    this->_list = { this->str() };

    return this->number();
}

std::string url::param::_set(const std::string value) {
    this->_str = value;
    this->_number = parse_number(this->str());
    this->_list = split(this->str(), ",");

    for (size_t i = 0; i < this->_list.size(); i++)
        this->_list[i] = trim(this->_list[i]);

    return this->str();
}

std::vector<std::string> url::param::_set(const std::vector<std::string> value) {
    this->_list = value;
    this->_str = join(value, ",");
    this->_number = NAN;

    return this->list();
}

std::vector<std::string> url::param::list() const {
    return this->_list;
}

double url::param::number() const {
    return this->_number;
}

std::string url::param::str() const {
    return this->_str;
}

bool& url::portinfo::typed() {
    return this->_typed;
}

int& url::portinfo::value() {
    return this->_value;
}

url::param::map& url::query_string::params() {
    return this->_params;
}

std::string url::query_string::str() {
    std::ostringstream ss;

    if (this->params().size()) {
        size_t               index = 0;
        param::map           params = this->params();
        param::map::iterator it = params.begin();

        for (; index < params.size() - 1; index++, it++)
            ss << (* it).first << "=" << (* it).second.str() << "&";

        ss << (* it).first << "=" << (* it).second.str();
    }
    
    return ss.str();
}

std::string url::fully_qualified_host() {
    std::string host = this->host();

    if (this->port().typed())
        host += ":" + std::to_string((int) this->port());

    return host;
}

std::string& url::host() {
    return this->_host;
}

url::param::map& url::params() {
    return this->_params;
}

std::string& url::protocol() {
    return this->_protocol;
}

url::portinfo& url::port() {
    return this->_port;
}

std::string url::query() {
    return query_string(this->params()).str();
}

std::string& url::target() {
    return this->_target;
}

std::string url::str() {
    std::ostringstream ss;

    if (this->protocol().length())
        ss << this->protocol() << "://";

    ss << this->fully_qualified_host() << this->target();

    if (this->params().size())
        ss << "?" << this->query();

    return ss.str();
}
