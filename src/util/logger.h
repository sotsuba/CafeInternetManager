#ifndef __logger_h__
#define __logger_h__

#include <stdexcept>
#include <string>   
#include <cstring> // strerror()
#include <errno.h> // errno()
static void throw_errno(const std::string &msg) {
    throw std::runtime_error(msg + ": " + std::strerror(errno));
}

#endif