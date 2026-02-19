//
//  service.h
//  http-json
//
//  Created by Corey Ferguson on 2/5/26.
//

#ifndef service_h
#define service_h

#include "http.h"
#include "json.h"
#include "logger.h"

using namespace http;
using namespace json;
using namespace std;

class service {
    // Member Fields

    logger _logger;
public:
    // Constructors

    service();

    service(class logger logger);

    // Member Functions

    string greeting(header::map headers, class request request);
    
    string ping(header::map headers);
};

#endif /* service_h */
