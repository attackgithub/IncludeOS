// This file is a part of the IncludeOS unikernel - www.includeos.org
//
// Copyright 2015 Oslo and Akershus University College of Applied Sciences
// and Alfred Bratterud
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once
#ifndef SYSLOGD_HPP
#define SYSLOGD_HPP

#include <cstdio>
#include <type_traits>
#include <string>
#include <map>
#include <regex>

#include <syslog.h>

#include <net/inet4>

using namespace net;

const int BUFLEN = 2048;
const int TIMELEN = 32;
// const int SYSLOG_PORT = 514;
const int UDP_PORT = 6000;
const int MUL_VAL = 8;

const std::map<int, std::string> pri_colors = {
  { LOG_EMERG,   "\033[38;5;1m" },    // RED
  { LOG_ALERT,   "\033[38;5;160m" },  // RED (lighter)
  { LOG_CRIT,    "\033[38;5;196m" },  // RED (even lighter)
  { LOG_ERR,     "\033[38;5;208m" },  // DARK YELLOW
  { LOG_WARNING, "\033[93m" },        // YELLOW
  { LOG_NOTICE,  "\033[92m" },        // GREEN
  { LOG_INFO,    "\033[96m" },        // TURQUOISE
  { LOG_DEBUG,   "\033[94m" }         // BLUE
};
const std::string COLOR_END = "\033[0m";

struct Syslog_facility {
  virtual void syslog(const std::string& log_message) = 0;
  virtual std::string name() = 0;
  virtual int calculate_pri() = 0;
  
  Syslog_facility() {}
  Syslog_facility(const char* ident) : ident_{ident} {}
  ~Syslog_facility();

  bool ident_is_set();
  const char* ident() { return ident_; }
  
  void set_priority(const int priority) { priority_ = priority; }
  int priority() { return priority_; }
  std::string priority_string();

  void set_logopt(const int logopt) { logopt_ = logopt; }
  int logopt() { return logopt_; }

  void open_socket();
  void close_socket();
  void send_udp_data(const std::string& data);

private:
  const char* ident_ = nullptr;
  int priority_;
  int logopt_;
  UDPSocket* sock_ = nullptr;
};

struct Syslog_kern : public Syslog_facility {
  void syslog(const std::string& log_message) override;
  std::string name() override;
  int calculate_pri() override;

  Syslog_kern() : Syslog_facility() {}
  Syslog_kern(const char* ident) : Syslog_facility(ident) {}
};

struct Syslog_user : public Syslog_facility {
  void syslog(const std::string& log_message) override;
  std::string name() override;
  int calculate_pri() override;

  Syslog_user() : Syslog_facility() {}
  Syslog_user(const char* ident) : Syslog_facility(ident) {}
};

struct Syslog_mail : public Syslog_facility {
  void syslog(const std::string& log_message) override;
  std::string name() override;
  int calculate_pri() override;

  Syslog_mail() : Syslog_facility() {}
  Syslog_mail(const char* ident) : Syslog_facility(ident) {}
};

struct Syslog {

  // Parameter pack arguments
  template <typename... Args>
  static void syslog(const int priority, const char* message, Args&&... args) {
    // snprintf removes % if calling syslog with %m in addition to arguments
    // Find %m here first and escape % if found
    std::regex m_regex{"\\%m"};
    std::string msg = std::regex_replace(message, m_regex, "%%m");

    char buf[BUFLEN];
    snprintf(buf, BUFLEN, msg.c_str(), args...);
    syslog(priority, buf);
  }

  // va_list arguments (POSIX)
  static void syslog(const int priority, const char* message, va_list args);
  
  static void syslog(const int priority, const char* buf);

  template <typename Facility>
  static void openlog(const char* ident, const int logopt) {
    static_assert(std::is_base_of<Syslog_facility, Facility>::value, "Facility is not base of Syslog_facility");
    last_open = std::make_unique<Facility>(ident);

    if (valid_logopt(logopt) or logopt == 0)  // Should be possible to clear the logopt
      last_open->set_logopt(logopt);

    /* Check for this in send_udp_data() in Syslog_facility instead
    if (logopt & LOG_CONS and last_open->priority() == LOG_ERR) {
      // Log to system console
    }*/
    
    if (logopt & LOG_NDELAY) {
      // Connect to syslog daemon immediately
      last_open->open_socket();

      // TODO: Move to open? - don't want to do this all the time:
      /*auto& sock = Inet4::stack<>().udp().bind();
      printf("BINDING\n");*/
      /*if (sock_ == nullptr) {
        sock_ = &Inet4::stack<>().udp().bind();
      }*/
      //connected_ = true;

    } /*else if (logopt & LOG_ODELAY) {
      // Delay open until syslog() is called = do nothing (the converse of LOG_NDELAY)
    }*/

    /*if (logopt & LOG_NOWAIT) {
      // Do not wait for child processes - deprecated
    }*/

    /* Check for this in send_udp_data() in Syslog_facility instead
    if (logopt & LOG_PERROR) {
      // Log to stderr
    }*/
  }

  static void closelog();

  static bool valid_priority(const int priority) noexcept {
    return not ((priority < LOG_EMERG) or (priority > LOG_DEBUG));
  }

  static bool valid_logopt(const int logopt) noexcept {
    return (logopt & LOG_PID or logopt & LOG_CONS or logopt & LOG_NDELAY or
      logopt & LOG_ODELAY or logopt & LOG_NOWAIT or logopt & LOG_PERROR);
  }

private:
  static std::unique_ptr<Syslog_facility> last_open;

  // TODO: Resolve -> Unique_ptr ?
  // Posix src/include/udp_fd.hpp
  //static UDPSocket* sock_;
  //static std::unique_ptr<UDPSocket> sock_;

  //static bool connected_ = false;
};

#endif