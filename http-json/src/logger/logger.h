//
//  logger.h
//  http-json
//
//  Created by Corey Ferguson on 2/5/26.
//

#ifndef logger_h
#define logger_h

#include <iostream>

// Typedef

enum logging { NONE, INFO, EXTENDED };

class logger {
    // Member Fields

    logging _logging;
public:
    // Constructors

    logger();

    logger(const enum logging logging);

    // Member Functions

    void          error(const std::string message);

    void          extended(const std::string message);

    void          info(const std::string message);

    enum logging& logging();
};

// Non-Member Functions

// void _write(const std::string message);

#endif /* logger_h */
