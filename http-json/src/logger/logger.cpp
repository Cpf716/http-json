//
//  logger.cpp
//  http-json
//
//  Created by Corey Ferguson on 2/5/26.
//

#include "logger.h"

// Constructors

logger::logger() : logger(INFO) { }

logger::logger(const enum logging logging) {
    this->_logging = logging;
}

// Non-Member Functions

void _write(const std::string message) {
    std::cout << message << std::endl;
}

// Member Functions

void logger::error(const std::string message) {
    _write("Error - " + message);
}

void logger::extended(const std::string message) {
    if (this->logging() == EXTENDED)
        _write(message);
}

void logger::info(const std::string message) {
    if (this->logging() >= INFO)
        _write(message);
}

enum logging& logger::logging() {
    return this->_logging;
}
