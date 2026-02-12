//
//  logger.h
//  http-json
//
//  Created by Corey Ferguson on 2/5/26.
//

#ifndef logger_h
#define logger_h

#include <iostream>

#define LEVEL_OFF 0
#define LEVEL_INFO 1
#define LEVEL_ERROR 2
#define LEVEL_DEBUG 3

#define LOGGING LEVEL_INFO

namespace logger {
    void error(const std::string message);

    void info(const std::string message);
}

#endif /* logger_h */
