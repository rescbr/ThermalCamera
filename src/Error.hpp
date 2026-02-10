#ifndef ERROR_HPP
#define ERROR_HPP

#include <string>
#include <stdexcept>
#include <iostream>
#include <sstream>

namespace Error
{
    enum class ErrorCode
    {
        Success = 0,
        CameraError,
        RenderError,
        ProcessError,
        ConfigError,
        MemoryError,
        InvalidArgument,
        InitializationError,
        RuntimeError
    };

    inline const char* ErrorCodeToString(ErrorCode code)
    {
        switch (code)
        {
            case ErrorCode::Success: return "Success";
            case ErrorCode::CameraError: return "Camera Error";
            case ErrorCode::RenderError: return "Render Error";
            case ErrorCode::ProcessError: return "Process Error";
            case ErrorCode::ConfigError: return "Configuration Error";
            case ErrorCode::MemoryError: return "Memory Error";
            case ErrorCode::InvalidArgument: return "Invalid Argument";
            case ErrorCode::InitializationError: return "Initialization Error";
            case ErrorCode::RuntimeError: return "Runtime Error";
            default: return "Unknown Error";
        }
    }

    class Exception : public std::runtime_error
    {
    public:
        Exception(ErrorCode code, const std::string& message, const char* file, int line)
            : std::runtime_error(message), _code(code), _file(file), _line(line)
        {
        }

        ErrorCode GetCode() const { return _code; }
        const char* GetFile() const { return _file; }
        int GetLine() const { return _line; }

        std::string GetFullMessage() const
        {
            std::ostringstream oss;
            oss << "[" << ErrorCodeToString(_code) << "] "
                << what() << " at " << _file << ":" << _line;
            return oss.str();
        }

    private:
        ErrorCode _code;
        const char* _file;
        int _line;
    };
}

#define ERROR_CODE(code, message) Error::Exception(code, message, __FILE__, __LINE__)

#define LOG_ERROR(message) \
    std::cerr << "[ERROR] " << __FILE__ << ":" << __LINE__ << " - " << message << std::endl

#define LOG_WARN(message) \
    std::cerr << "[WARN] " << __FILE__ << ":" << __LINE__ << " - " << message << std::endl

#define LOG_INFO(message) \
    std::cerr << "[INFO] " << __FILE__ << ":" << __LINE__ << " - " << message << std::endl

#define LOG_DEBUG(message) \
    do { \
        if (false) \
            std::cerr << "[DEBUG] " << __FILE__ << ":" << __LINE__ << " - " << message << std::endl; \
    } while (0)

#endif
