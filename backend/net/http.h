#ifndef __http_h__
#define __http_h__

#include <string>
#include <vector>
#include <regex>
#include <unordered_map>
#include <sstream>

using std::string, std::vector;

class RequestLine {
    string method;
    string endpoint;
    string httpVersion;
public:
    RequestLine() = default;
    
    RequestLine(const string& line) {
        std::istringstream iss(line);
        iss >> method >> endpoint >> httpVersion;
        if (iss.fail()) 
            method = endpoint = httpVersion = "";
    }

    bool isCorrectWebsocketUpgradeRequest() {
        return (method == "GET" && httpVersion == "HTTP/1.1");
    }
};

class HttpRequestParser {
private:
    RequestLine requestLine;
    std::unordered_map<string, string> header{};
public:
    HttpRequestParser() = default;
    HttpRequestParser(const string& data) {
        std::istringstream issMain(data);
        
        size_t lineIndex = 0;
        string line;
        while (std::getline(issMain, line)) {
            if (lineIndex++ == 0) {
                requestLine = RequestLine(line);
                continue;
            }
            std::istringstream issSub(line);
            string fieldName, value;
            issSub >> fieldName >> value;
            header.insert({fieldName, value});
        }
    }
    
    RequestLine getRequestLine() { return requestLine; }
    string operator[](const string& fieldName) const {
        auto it = header.find(fieldName);
        return (it != header.end()) ? it->second : "";
    }
};

#endif