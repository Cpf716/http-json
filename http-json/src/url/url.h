//
//  url.h
//  http-json
//
//  Created by Corey Ferguson on 1/28/26.
//

#ifndef url_h
#define url_h

#include "util.h"
#include <map>

struct url {
    // Typdef

    struct error: public std::exception {
        // Constructors

        error(const std::string what);

        // Member Fields

        const char* what() const throw();
    private:
        // Member Fields

        std::string _what;
    };

    struct param {
        // Typedef

        using map = std::map<std::string, param>;

        // Constructors

        param();

        param(const char* value);

        param(const double value);

        param(const std::string value);

        param(const std::vector<std::string> value);

        // Operators

        operator                 double();

        operator                 std::string();

        operator                 std::vector<std::string>();

        double                   operator=(const double value);

        std::string              operator=(const std::string value);

        std::vector<std::string> operator=(const std::vector<std::string> value);

        bool                     operator==(const char* value);

        bool                     operator==(const double value);

        bool                     operator==(const std::string value);

        bool                     operator==(const param value);

        bool                     operator!=(const char* value);

        bool                     operator!=(const double value);

        bool                     operator!=(const std::string value);

        bool                     operator!=(const param value);

        // Member Functions

        std::vector<std::string> list() const;

        double                   number() const;

        std::string              str() const;
    private:
        // Member Fields

        std::vector<std::string> _list;
        double                   _number;
        std::string              _str;

        // Member Functions

        double                   _set(const double value);

        std::string              _set(const std::string value);

        std::vector<std::string> _set(const std::vector<std::string> value);
    };

    class portinfo {
        // Constructors
        
        portinfo();

        portinfo(const int value, const bool typed);
        
        // Member Fields

        bool _typed;
        int  _value;
    public:
        // Typdef
        
        friend url;
        
        // Member Functions

        bool& typed();

        int&  value();
    };

    // Constructors

    url();

    url(const std::string value);

    // Member Functions

    std::string& host();

    std::string& protocol();

    param::map&  params();

    portinfo&    port();

    std::string  query();

    std::string  str();

    std::string& target();
private:
    // Member Fields

    std::string    _host;
    param::map     _params;
    class portinfo _port;
    std::string    _protocol;
    std::string    _target;

    // Member Functions
    
    std::string _query(std::ostringstream& oss);
};

#endif /* url_h */
