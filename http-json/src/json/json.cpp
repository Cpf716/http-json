//
//  json.cpp
//  http-json
//
//  Created by Corey Ferguson on 1/28/26.
//

#include "json.h"
#include <iostream>

namespace json {
    // Non-Member Fields

    const std::string DELIMITERS[] { "[", "]", ",", ":", "{", "}" };

    // Constructors

    array::array() {
        this->type() = array_t;
    }
        
    array::array(const std::string key): array() {
        this->_key = key;
    }

    array::array(const size_t size): array() {
        this->_values.resize(size);

        for (size_t i = 0; i < this->size(); i++)
            this->_values[i] = new object();
    }

    array::array(const std::vector<object*> values): array() {
        for (object* value: values)
            this->set(value);
    }

    error::error(const std::string what) {
        this->_what = what;
    }

    array::iterator::iterator(const size_t size, std::vector<object*> values) {
        this->_size = size;
        this->_values = values;
    }

    object::object() { }

    object::object(const std::string key) {
        this->_key = key;
    }

    object::object(std::map<std::string, std::string> options) : object(options["k"], options["v"]) {
        if (options["t"].length() && this->value().length())
            throw json::error("Object cannot have both value and values");

        this->_parse(options["t"]);
    }

    object::object(const enum type type) {
        this->type() = type;
    }

    object::object(const std::string key, const enum type type) : object(key) {
        this->type() = type;
    }

    object::object(const std::string key, const std::string value) : object(key) {
        this->_value = value;
    }

    object::object(const std::vector<object*> values, const enum type type) : object(type) {
        for (object* value: values)
            this->set(value);
    }

    object::~object() {
        for (object* value: this->_values)
            delete value;
    }

    // Operators

    object* array::iterator::operator*() const {
        return this->_values[this->_index];
    }

    array::iterator& array::iterator::operator+(int value) {
        this->_index += value;

        if (this->_index > this->_size)
            this->_index = (int)this->_size;
        else if (this->_index < 0)
            this->_index = 0;

        return *this;
    }

    array::iterator& array::iterator::operator++() {
        if (this->_index != this->_size)
            this->_index++;

        return *this;
    }

    array::iterator& array::iterator::operator++(int) {
        array::iterator& temp = *this;

        if (this->_index != this->_size)
            this->_index++;

        return temp;
    }

    array::iterator& array::iterator::operator-(int value) {
        this->_index -= value;

        if (this->_index < 0)
            this->_index = 0;
        else if (this->_index > this->_size)
            this->_index = (int)this->_size;

        return *this;
    }

    array::iterator& array::iterator::operator--() {
        if (this->_index != 0)
            this->_index--;

        return *this;
    }

    array::iterator& array::iterator::operator--(int) {
        array::iterator& temp = *this;

        if (this->_index != 0)
            this->_index--;

        return temp;
    }

    bool array::iterator::operator==(const array::iterator& value) const {
        return this->_index == value._index;
    }

    bool array::iterator::operator!=(const array::iterator& value) const {
        return this->_index != value._index;
    }

    // Member Functions

    int object::_find(const std::string key) {
        return this->_find(key, 0, (int)this->_key_map.size());
    }

    int object::_find(const std::string key, const int start, const int end) {
        if (start == end)
            return -1;
        
        int len = floor((end - start) / 2);
        
        if (this->_key_map[start + len].first == key)
            return start + len;
        
        if (this->_key_map[start + len].first > key)
            return this->_find(key, start, start + len);
        
        return this->_find(key, start + len + 1, end);
    }

    void object::_map_keys() {
        size_t i = 0;

        while (i < this->_values.size() && this->_values[i]->key().empty())
            i++;

        for (; i < this->_values.size(); i++) {
            if (this->_values[i]->key().empty())
                throw json::error("undefined");

            this->_key_map.push_back({ this->_values[i]->key(), i });
            this->_values[i]->_map_keys();
        }

        if (this->type() == object_t && this->size())
            throw json::error("Objects cannot have anonymous properties");

        // Sort by key
        for (int i = 1; i < this->_key_map.size(); i++)
            for (int j = i - 1; j >= 0 && this->_key_map[j].first > this->_key_map[j + 1].first; j--)
                std::swap(this->_key_map[j], this->_key_map[j + 1]);
    }

    void object::_parse(const std::string text) {
        if (text.empty())
            return;

        size_t end = 0,
               start = 0;

        std::vector<std::string> tokens;
        
        while (end < text.length()) {
            size_t i;
            
            for (i = 0; i < sizeof(DELIMITERS) / sizeof(DELIMITERS[0]); i++) {
                if (end > text.length() - DELIMITERS[i].length())
                    continue;
                
                size_t j = 0;
                
                while (j < DELIMITERS[i].length() && text[end + j] == DELIMITERS[i][j])
                    j++;
                
                if (j == DELIMITERS[i].length())
                    break;
            }
            
            if (i == sizeof(DELIMITERS) / sizeof(DELIMITERS[0]))
                end++;
            else {
                if (start != end)
                    tokens.push_back(text.substr(start, end - start));
                
                tokens.push_back(DELIMITERS[i]);

                start = (end += DELIMITERS[i].length());
            }
        }
        
        if (start != end)
            tokens.push_back(text.substr(start));
        
        merge(tokens);

        end = 0;
        
        while (end < tokens.size()) {
            tokens[end] = trim(tokens[end]);
            
            if (tokens[end].empty())
                tokens.erase(tokens.begin() + end);
            else
                end++;
        }

        this->_parse(this, tokens, 0, tokens.size());
        this->_map_keys();
    }

    object* object::_parse(object* target, std::vector<std::string>& source, const size_t start, const size_t end) {
        for (size_t i = start; i < end; i++) {
            if (source[i] == ",")
                continue;
                // Anonymous value
            else if (source[i] == "{") {
                target->type() = object::object_t;
                
                size_t j,
                       p = 1;
                
                for (j = i + 1; j < end; j++) {
                    if (source[j] == "{")
                        p++;
                    else if (source[j] == "}") {
                        if (p == 1)
                            break;
                        
                        p--;
                    }
                }
                
                // Parse properties
                this->_parse(target, source, i + 1, j);
                
                i = j;
            } else if (source[i] == "[") {
                target->type() = object::array_t;
                
                size_t j,
                       p = 1;
                
                for (j = i + 1; j < end; j++) {
                    if (source[j] == "[")
                        p++;
                    else if (source[j] == "]") {
                        if (p == 1)
                            break;
                        
                        p--;
                    }
                }

                // An array's items must be parsed explicitly, as new objects are otherwise only allocated for named primitives
                for (size_t k = i + 1; k < j; k++) {
                    // Named or anonymous values
                    std::string key;
                    
                    if (k != j - 1 && source[k + 1] == ":") {
                        key = decode(source[k]);
                        k += 2;
                    }
                    
                    size_t l = k;
                    
                    p = 0;
                    
                    do {
                        if (source[l] == "[" || source[l] == "{")
                            p++;
                        else if (source[l] == "]" || source[l] == "}")
                            p--;
                        
                        l++;
                    } while (l < j && p);
                    
                    target->_values.push_back(this->_parse(new object(key), source, k, l));
                    
                    k = l;
                }

                i = j;
                // Primitive
            } else if (i != end - 1 && source[i + 1] == ":") {
                std::string key = decode(source[i]);
            
                i += 2;
                
                size_t j = i,
                       p = 0;
                
                do {
                    if (source[j] == "[" || source[j] == "{")
                        p++;
                    else if (source[j] == "]" || source[j] == "}")
                        p--;
                    
                    j++;
                } while (j < end && p);
                
                target->_values.push_back(this->_parse(new object(key), source, i, j));
                
                i = j;
            } else {
                target->type() = object::primitive_t;
                target->value() = source[i];
            }
        }
        
        return target;
    }

    json::array* array::_splice(const int start, const int delete_count, const std::vector<object*> values) {
        json::array* result = new json::array();

        for (int i = 0; i < delete_count; i++) {
            result->set(this->_values[start]);

            this->_values.erase(this->_values.begin() + start);
        }

        for (size_t i = 0; i < values.size(); i++) {
            if (values[i]->key().length())
                throw json::error("Cannot splice named properties");

            this->_values.insert(this->_values.begin() + start + i, values[i]);
        }
        
        return result;
    }

    void object::_erase(const size_t index) {
        std::pair<std::string, size_t> key_map = this->_key_map[index];

        delete this->_values[this->size() + key_map.second];

        this->_values.erase(this->_values.begin() + this->size() + key_map.second);
        this->_key_map.erase(this->_key_map.begin() + index);

        for (size_t i = 0; i < this->_key_map.size(); i++)
            if (this->_key_map[i].second > key_map.second)
                this->_key_map[i].second--;
    }

    object* array::at(int index) {
        if (index < 0) {
            index += this->size();

            if (index < 0)
                return NULL;
        } else if (index >= this->size())
            return NULL;

        return this->get(index);
    }

    array::iterator array::begin() {
        return array::iterator(this->size(), this->_values);
    }

    json::array* array::concat(std::vector<object*> values) {
        json::array* result = new json::array(this->_values);

        for (object* value: values)
            for (size_t i = 0; i < value->size(); i++)
                result->set(((json::array *)value)->get(i));
        
        return result;
    }

    array::iterator array::end() {
        return this->begin() + (int)this->size();
    }

    void object::erase() {
        this->type() = primitive_t;
        this->value().clear();
        this->_key_map.clear();

        // NOTE: values must be explicitly deallocated
        this->_values.clear();
    }

    void object::erase(const size_t index) {
        this->erase(std::to_string(index));
    }

    void object::erase(const std::string key) {
        std::function<void(void)> _erase = [&]() {
            int index = this->_find(key);

            if (index != -1)
                this->_erase(index);
        };

        if (this->type() == array_t) {
            int index = parse_int(key);
            
            // Named item
            if (index == INT_MIN)
                _erase();
            // Anonymous item
            else if (index < this->size())
                ((array *)this)->get(index)->erase();
            // (Named) property
        } else {
            if (this->type() != object_t)
                throw json::error("Operation not permitted");

            _erase();
        }
    }

    object* array::get(const size_t index) {
        return object::get(std::to_string(index));
    }

    object* array::get(std::string key) {
        return this->object::get(key);
    }

    object* object::get(const std::string key) {
        if (this->type() == object::array_t) {
            int index = parse_int(key);
            
            if (index == INT_MIN) {
                index = _find(key);
                
                if (index == -1)
                    return NULL;
                
                return this->_values[this->size() + this->_key_map[index].second];
            } else {
                if (index < 0) {
                    index = _find(key);
                    
                    if (index == -1)
                        return NULL;
                    
                    return this->_values[this->size() + this->_key_map[index].second];
                }
                
                if (index < this->size())
                    return this->_values[index];
                
                return NULL;
            }
        }
        
        if (this->type() != object::object_t)
            throw json::error("Operation not permitted");
        
        int index = _find(key);
        
        if (index == -1)
            return NULL;
        
        return this->_values[this->size() + this->_key_map[index].second];
    }

    std::string object::key() {
        return this->_key;
    }

    bool object::null() {
        return this->value() == json::null() && !this->_values.size();
    }

    void object::nullify() {
        this->value() = json::null();

        this->_key_map.clear();
        
        for (object* value: this->_values)
            delete value;

        this->_values.clear();
    }

    object* array::set(object* value) {
        return this->object::set(value);
    }

    object* array::set(const size_t index, object* value) {
        object* tmp = new object({{ "k", std::to_string(index) }, { "t", stringify(value) }});
        
        delete value;
        
        value = tmp;

        this->object::set(value);

        return value;
    }

    json::array* array::slice(const int start) {
        return this->slice(start, (int)this->size());
    }

    json::array* array::slice(int start, int end) {
        json::array* result = new json::array();

        if (start < 0) {
            start += this->size();

            if (start < 0)
                start = 0;
        }

        if (end < 0) {
            end += this->size();

            if (end < 0)
                end = 0;
        } else if (end > this->size())
            end = (int)this->size();

        for (int i = start; i < end; i++)
            result->set(this->get(i));

        return result;
    }

    json::array* array::splice(int start) {
        int delete_count;

        if (start < 0) {
            start += (int)this->size();
            delete_count = (int)this->size();

            if (start < 0)
                start = 0;
            else
                delete_count -= start;
        } else if (start >= this->size())
            delete_count = 0;
        else
            delete_count = (int)this->size() - start;

        return this->_splice(start, delete_count, std::vector<object*>());
    }

    json::array* array::splice(const int start, const int delete_count) {
        return this->splice(start, delete_count, std::vector<object*>());
    }

    json::array* array::splice(int start, int delete_count, const std::vector<object*> values) {
        if (this->type() != object::array_t)
            throw json::error("Operation not permitted");

        if (start < 0) {
            start += (int)this->size();
            delete_count = (int)this->size();

            if (start < 0)
                start = 0;
            else
                delete_count -= start;
        } else if (start >= this->size())
            delete_count = 0;
        else if (start + delete_count > this->size())
            delete_count = (int)this->size() - start;

        return this->_splice(start, delete_count, values);
    }


    object* object::sanitize() {
        size_t i = this->size();

        while (i < this->_values.size()) {
            if (this->_values[i]->undefined())
                this->_erase(this->_find(this->_values[i]->key()));
            else {
                this->_values[i]->sanitize();
                i++;
            }
        }
        
        return this;
    }

    object* object::set(object* value) {
        if (this->type() == object::array_t) {
            if (value->key().empty()) {
                this->_values.push_back(value);
                
                // Sort before named values
                for (size_t i = 0; i < this->_key_map.size(); i++)
                    std::swap(this->_values[this->_values.size() - i - 1],this->_values[this->_values.size() - i - 2]);
            } else {
                int index = parse_int(value->key());
                
                // Named value
                if (index == INT_MIN) {
                    this->_values.push_back(value);
                    this->_key_map.push_back({ value->key(), this->_key_map.size() });
                    
                    for (size_t i = this->_key_map.size() - 1; i > 0 && this->_key_map[i].first < this->_key_map[i - 1].first; i--)
                        std::swap(this->_key_map[i], this->_key_map[i - 1]);
                } else if (index >= 0) {
                    value->_key = "";
                    
                    // Replace item
                    if (index < this->size())
                        this->_values[index] = value;
                        // Add item
                    else {
                        while (this->size() < index) {
                            this->_values.push_back(new object());
                            
                            // Sort before named values
                            for (size_t i = 0; i < this->_key_map.size(); i++)
                                std::swap(this->_values[this->_values.size() - i - 1], this->_values[this->_values.size() - i - 2]);
                        }
                        
                        this->_values.push_back(value);
                        
                        // Sort before named values
                        for (size_t i = 0; i < this->_key_map.size(); i++)
                            std::swap(this->_values[this->_values.size() - i - 1],this->_values[this->_values.size() - i - 2]);
                    }
                } else {
                    this->_values.push_back(value);
                    this->_key_map.push_back({ value->key(), this->_key_map.size() });
                    
                    for (size_t i = this->_key_map.size() - 1; i > 0 && this->_key_map[i].first < this->_key_map[i - 1].first; i--)
                        std::swap(this->_key_map[i], this->_key_map[i - 1]);
                }
            }
            
            return value;
        }

        if (this->type() != object::object_t)
            throw json::error("Operation not permitted");

        if (value->key().empty())
            throw json::error("Objects cannot have anonymous properties");
        
        int index = _find(value->key());
        
        if (index == -1) {
            this->_values.push_back(value);
            this->_key_map.push_back({ value->key(), this->_key_map.size() });
            
            // Sort by key
            for (size_t i = this->_key_map.size() - 1; i > 0 && this->_key_map[i].first < this->_key_map[i - 1].first; i--)
                std::swap(this->_key_map[i], this->_key_map[i - 1]);
        } else
            this->_values[this->size() + this->_key_map[index].second] = value;
        
        return value;
    }

    size_t object::size() {
        return this->_values.size() - this->_key_map.size();
    }

    enum object::type& object::type() {
        return this->_type;
    }

    bool object::undefined()  {
        return this->type() == object::primitive_t && this->value().empty();
    }

    std::string& object::value() {
        return this->_value;
    }

    const char* error::what() const throw() {
        return this->_what.c_str();
    }

    // Non-Member Functions

    std::string _stringify(object* value) {
        std::ostringstream ss;
        
        // Named value
        if (!value->key().empty())
            ss << encode(value->key()) << ":";

        if (value->null()) {
            ss << null();

            return ss.str();
        }

        if (value->undefined()) {
            ss << "undefined";

            return ss.str();
        }
        
        std::string delimiters[2];
        
        switch (value->type()) {
            case object::array_t:
                delimiters[0] = "[";
                delimiters[1] = "]";
                break;
            case object::object_t:
                delimiters[0] = "{";
                delimiters[1] = "}";
                break;
            default:
                break;
        }
        
        ss << delimiters[0];
     
        if (value->_values.size()) {
            size_t i;
            
            for (i = 0; i < value->_values.size() - 1; i++)
                ss << _stringify(value->_values[i]) << ",";

            ss << _stringify(value->_values[i]);
        } else
            ss << value->value();
        
        ss << delimiters[1];
            
        return ss.str();
    }

    /**
     * Deep copy source and assign its contents to target
     */
    object* assign(object* target, object* source) {
        // Target is an array; clear its items
        if (target->type() == object::array_t) {
            if (source->type() == object::array_t) {
                for (size_t i = 0; i < source->size(); i++)
                    ((json::array *)target)->set(i, source->_values[i]);
                // Source is an object; do nothing
            }
            // Target is an object
        } else {
            if (target->type() != object::object_t)
                throw json::error("Operation not permitted");
            
            // Source is an array; assign its items' keys by index
            // Cloning is required to mutate keys
            for (size_t i = 0; i < source->size(); i++)
                target->set(new object({{ "k", std::to_string(i) }, { "t", stringify(((array *)source)->get(i)) }}));
            
            std::vector<object*> values = json::values(source);
    
            for (size_t i = source->size(); i < values.size(); i++)
                target->set(new object({{ "k", values[i]->key() }, { "t", stringify(values[i]) }}));
        }
      
        return target;
    }

    std::vector<std::pair<std::string, object*>> entries(object* value) {
        std::vector<std::pair<std::string, object*>> result;

        for (object* _value: values(value))
            result.push_back({ _value->key(), _value });

        return result;
    }

    std::vector<std::string> keys(object* value) {
        std::vector<std::string> result;
        
        if (value->type() == object::primitive_t) {
            if (!is_number(value->value()))
                for (size_t i = 0; i < value->value().length(); i++)
                    result.push_back(std::to_string(i));
        } else {
            if (value->type() == object::array_t)
                for (size_t i = 0; i < value->size(); i++)
                    result.push_back(std::to_string(i));

            std::vector<object*> values = json::values(value);

            for (size_t i = value->size(); i < values.size(); i++)
                result.push_back(values[i]->key());
        }
        
        return result;
    }

    std::string null() {
        return "null";
    }

    object* parse(const std::string text) {
        return new object({{ "t", text }});
    }

    std::string stringify(object* value) {
        if (value->null())
            throw json::error("null");

        if (value->undefined())
            throw json::error("undefined");

        std::string        delimiters[2];
        std::ostringstream ss;
        
        switch (value->type()) {
            case object::array_t:
                delimiters[0] = "[";
                delimiters[1] = "]";
                break;
            case object::object_t:
                delimiters[0] = "{";
                delimiters[1] = "}";
                break;
            default:
                break;
        }
        
        ss << delimiters[0];
     
        if (value->_values.size()) {
            size_t i;

            for (i = 0; i < value->_values.size() - 1; i++)
                ss << _stringify(value->_values[i]) << ",";

            ss << _stringify(value->_values[i]);
        } else
            ss << value->value();
        
        ss << delimiters[1];
            
        return ss.str();
    }

    std::string type_str(object* value) {
        switch (value->type()) {
            case object::array_t:
                return "array";
            case object::object_t:
                return "object";
            case object::primitive_t: {
                if (value->value() == "null")
                    return "unknown";

                std::string lower_value = value->value();

                std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(), ::tolower);

                if (lower_value == "true" || lower_value == "false")
                    return "boolean";

                return is_number(value->value()) ? "number" : "string";
            }
        }
    }

    std::vector<object*> values(object* value) {
        if (value->type() == object::primitive_t) {
            std::vector<object*> result;
            
            if (!is_number(value->value()))
                for (char c: decode(value->value()))
                    result.push_back(new object({{ "v", encode(std::string((char[]){ c, '\0' })) }}));
            
            return result;
        }

        return value->_values;
    }
}
