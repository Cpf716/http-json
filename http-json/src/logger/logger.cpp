//
//  logger.cpp
//  http-json
//
//  Created by Corey Ferguson on 2/5/26.
//

#include "logger.h"

namespace logger {
    void error(const std::string message) {
        info("Error - " + message);
    }

    void info(const std::string message) {
        std::cout << message << std::endl;
    }
}
