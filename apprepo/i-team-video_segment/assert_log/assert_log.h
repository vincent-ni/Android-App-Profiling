/*
 *  assert_log.h
 *  assert_log
 *
 *  Created by Matthias Grundmann on 5/22/09.
 *  Copyright 2009 Matthias Grundmann. All rights reserved.
 *
 *  Based on: http://www.ddj.com/cpp/184403861   ('Assertions' by Andrei Alexandrescu)
 *  Logging inspired by google logging framework.
 */

// Defines two macros
// ASSERT_LOG
// ASSURE_LOG

// Usage: [ASSERT|ERROR]_LOG(condition) << {Some message} << {another message} ...
// ASSERT_LOG will be ignored if NDEBUG is defined - otherwise it breaks
// at the current line.
// ASSURE_LOG is ALWAYS evaluated.
// It aborts if NDEBUG is defined otherwise breaks at the current line.

// Logging interface.
// Usage: LOG(LogLevel) << "Message";
// Supported LogLevel's (according to priority):
// FATAL
// ERROR
// WARNING
// INFO
// INFO_V1 .. INFO_V4    <- Not logged by default, Info is lowest priority
//
// Adapt log level by setting lowest priority
// Log<LogToStdErr>::ReportLevel() == DESIRED_LEVEL

#ifndef ASSERT_LOG_H__
#define ASSERT_LOG_H__

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <cstdio>
#include <ctime>

class AssertLog {
public:

  AssertLog(const std::string& file, const std::string& function, int line) :
    file_(file), function_(function), line_(line) {}

  AssertLog& operator()(bool condition) {
    cond_ = condition;
    return *this;
  }

  AssertLog(const AssertLog& log) :
      cond_(log.cond_), file_(log.file_), function_(log.function_), line_(log.line_) {
    log_message_ << log.log_message_.str();
  }

  bool evaluate_condition() const {
    if (cond_)
      return true;
    else {
      // Try to strip path from file_.
      std::string filename = file_;
      int strip_pos = file_.find_last_of("/\\");
      if (strip_pos >= 0) {
        filename = file_.substr(strip_pos + 1);
      }

      std::cerr << "\n[ERROR] " << filename << ":" << line_ << ":" << function_
                << ": " << log_message_.str() << "\n";
      return false;
    }
  }

  template <class T>
  AssertLog& operator<<(const T& output) {
    log_message_ << output;
    return *this;
  }

private:
  bool cond_;
  std::ostringstream log_message_;
  std::string file_;
  std::string function_;
  int line_;

};

#if defined __APPLE__
  #define DEBUG_BREAK { __asm {int 3} }
#elif defined _WIN32
  #include <windows.h>
  // ... nt ...
  #undef ERROR
  #undef FATAL
  #undef min
  #undef max
  #define DEBUG_BREAK DebugBreak()
#elif defined __linux

  extern "C"{
    #include <signal.h>
  }

  #define DEBUG_BREAK ::raise(SIGINT)
#endif

// ASSERT_LOG definition.

#ifdef NDEBUG

#define ASSERT_LOG \
if (true) ; \
  else \
    struct Local { \
      Local(const AssertLog& assert_log) { \
        if (!assert_log.evaluate_condition()) { \
          DEBUG_BREAK; \
        } \
      } \
    } local_assert = AssertLog(__FILE__, __FUNCTION__, __LINE__)

#else

#define ASSERT_LOG \
if (false) ; \
else \
  struct Local { \
    Local(const AssertLog& assert_log) { \
      if (!assert_log.evaluate_condition()) { \
      DEBUG_BREAK; } \
    } \
  } local_assert = AssertLog(__FILE__, __FUNCTION__, __LINE__)

#endif  // NDEBUG


// ERROR_LOG definition


#ifdef NDEBUG

#define ASSURE_LOG \
  if (false) ; \
    else \
  struct Local { \
    Local(const AssertLog& assert_log) { \
      if (!assert_log.evaluate_condition()) { \
      exit(1);  } \
    } \
  } local_assert = AssertLog(__FILE__, __FUNCTION__, __LINE__)

#else

#define ASSURE_LOG ASSERT_LOG

#endif  // NDEBUG



// Logging class.
// Based on: http://www.drdobbs.com/cpp/201804215

// Log Level with descending priority.
enum LogLevel {FATAL = 0,
               ERROR = 1,
               WARNING = 2,
               INFO = 3,
               INFO_V1 = 4,
               INFO_V2 = 5,
               INFO_V3 = 6,
               INFO_V4 = 7};

class LogToStdErr {
public:
  static void Log(const char* out_string) {
    fprintf(stderr, "%s", out_string);
    fflush(stderr);
  }
};

// Logging class for various LogLevels, to be used via macros
// LOG(FATAL) << "Message";
// Logging exits on FATAL errors.
// Standard maximum level is set to INFO.
// Output has to support static void Log(const char*) to output logging information.
template<class Output>
class Log {
public:
  Log() {}
  virtual ~Log();
  std::ostringstream& GetStream(LogLevel level,
                                const std::string& file,
                                const std::string& function,
                                int line);
public:
  static LogLevel& ReportLevel();

private:
  // Protect copying.
  Log(const Log&);
  Log& operator =(const Log&);

  const char* StringToLevel(int level) {
    switch (level) {
      case FATAL:
        return "FATAL  ";
      case ERROR:
        return "ERROR  ";
      case WARNING:
        return "WARNING";
      case INFO:
        return "INFO   ";
      case INFO_V1:
        return "INFO_V1";
      case INFO_V2:
        return "INFO_V2";
      case INFO_V3:
        return "INFO_V3";
      case INFO_V4:
        return "INFO_V4";
      default:
        break;
    }

    return "UNKNOWN";
  }

private:
  LogLevel log_level;
  std::ostringstream os;
};

template<class Output>
std::ostringstream& Log<Output>::GetStream(LogLevel level,
                                           const std::string& file,
                                           const std::string& function,
                                           int line) {
  // Try to strip path from file.
  std::string filename = file;
  int strip_pos = file.find_last_of("/\\");
  if (strip_pos >= 0) {
    filename = file.substr(strip_pos + 1);
  }

  // Get current time.
  time_t rawtime;
  struct tm* timeinfo;
  char time_string[16];

  // Get time and convert to local time.
  time (&rawtime);
  timeinfo = localtime (&rawtime);

  strftime (time_string, 16, "%b %d %X", timeinfo);

  os << "[" << StringToLevel(level) << " ";
  os << time_string << " ";
  os << filename << ":" << line << ":" << function << "]  ";

  log_level = level;

  return os;
}

template<class Output>
LogLevel& Log<Output>::ReportLevel() {
  static LogLevel log_level = INFO;
  return log_level;
}

template<class Output>
Log<Output>::~Log() {
  os << std::endl;
  Output::Log(os.str().c_str());

  if (log_level == FATAL) {
    #ifdef NDEBUG
    exit(1);
    #else
    DEBUG_BREAK;
    #endif
  }
}

#define LOG(level) \
if (level > Log<LogToStdErr>::ReportLevel()) ; \
else Log<LogToStdErr>().GetStream(level, __FILE__, __FUNCTION__, __LINE__)

#endif  // ASSERT_LOG_H__
