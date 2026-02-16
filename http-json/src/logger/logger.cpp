//
//  logger.cpp
//  http-json
//
//  Created by Corey Ferguson on 2/5/26.
//

#include "logger.h"

namespace logger {
    void _write(const std::string message) {
        std::cout << message << std::endl;
    }

    void debug(const std::string message) {
#if LOGGING == LEVEL_DEBUG
        _write(message);
#endif
    }

    void error(const std::string message) {
        _write("Error - " + message);
    }

    void info(const std::string message) {
#if LOGGING >= LEVEL_INFO
        _write(message);
#endif
    }
}
