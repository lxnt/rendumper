/* Why write another log4cpp?

    Because it needs to work across module boundaries.
    Because it must be thread-safe in the face of abstracted away threading.
    Because it doesn't need to pull tons of dependencies.
    Because it doesn't need idiocy like stringstream support.
    Because it doesn't need bloat like rolling appenders or smtp support.

    Because it's less work than to integrate something external.

    Differences from common log4j clones:
        - propagate is effectively set to false and cannot be changed.
        - no handlers, appenders or formatters.
        - their functionality is in the ilogsink::vlog_message().
        - which can't be changed or configured at runtime.
        - getting a logger for the first time is expensive.
        - changing its level is more expensive.
*/

#include <cstdarg>
#include <string>
#include <unordered_map>
#include <forward_list>

#include "iplatform.h"

struct ilogsink {
    virtual void vlog_message(int, const std::string&, const char *, va_list) = 0;
};

struct alogger : public ilogger {
    int level, effective;
    std::string name;
    ilogsink *bugor;

    alogger(const char *n, int l, int e, ilogsink *p);
    alogger(const std::string& n, int l, int e, ilogsink *p);
    bool enabled(const int for_wha);
    void trace(const char *fmt, ...);
    void info(const char *fmt, ...);
    void warn(const char *fmt, ...);
    void error(const char *fmt, ...);
    NORETURN void fatal(const char *fmt, ...);
};

struct log_implementation : public ilogsink {
    FILE *dst;
    iplatform *platform;

    std::unordered_map<std::string, alogger *> config;
    alogger *root_logger;

    const char *ll_names(int l);

    log_implementation(iplatform *p, FILE *d);
    void configure(const char*);
    void dump_conf();
    void reconf_chain(alogger *leaf);
    ilogger *logconf(const char *n, int level);
    ilogger *getlogr(const char *n);
    void vlog_message(int level, const std::string& name, const char *fmt, va_list ap);
};
