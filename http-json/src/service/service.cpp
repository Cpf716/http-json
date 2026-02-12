//
//  service.cpp
//  http-json
//
//  Created by Corey Ferguson on 2/5/26.
//

#include "service.h"

string service::greeting(header::map headers, class request request) {
    headers["Content-Type"] = string("application/json");
    
    try {
        if (request.headers()["content-type"] != "application/json")
            throw runtime_error("must have required property 'firstName'");

        if (request.body().empty())
            throw runtime_error("must have required property 'firstName'");
        
        object* options = parse(request.body());

        if (options->type() != object::OBJECT)
            throw runtime_error("must be object");

        object* first_name = options->get("firstName");
        object* last_name = options->get("lastName");
        object* nickname = options->get("nickname");

        auto null_or_empty = [](object* value){
            return value == NULL || value->value().empty() || value->value() == null();
        };

        string result = "Hello, ";

        if (null_or_empty(nickname)) {
            if (null_or_empty(first_name))
                throw runtime_error("must have required property 'firstName'");

            if (!is_string(first_name->value()))
                throw runtime_error("must be string");
            
            result += decode(first_name->value());
            
            if (!null_or_empty(last_name)) {
                if (!is_string(last_name->value()))
                    throw runtime_error("must be string");

                result += " " + decode(last_name->value());
            }
        } else
            result += decode(nickname->value());
        
        result += "!";
        
        return response(encode(result), headers);
    } catch (std::exception& e) {
        logger::error(e.what());
        
        return response(BAD_REQUEST, strstatus(BAD_REQUEST), stringify(new object((vector<object*>){
                new object("message", encode(e.what())),
                new object("status", to_string(BAD_REQUEST))
            }
        )), headers);
    }
}

string service::ping(header::map headers) {
    headers["Content-Type"] = string("application/json");

    return response(encode("Hello, world!"), headers);
}
