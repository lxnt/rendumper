#include <cstdarg>
#include <string>
#include <unordered_map>

#include "iplatform.h"

struct ilogsink {
    virtual void vlog_message(int, const std::string&, const char *, va_list) = 0;
};

struct alogger : public ilogger {
    int level;
    std::string name;
    ilogsink *bugor;

    alogger(const char *n, int l, ilogsink *p): level(l), name(n), bugor(p) {}
    alogger(const std::string& n, int l, ilogsink *p): level(l), name(n), bugor(p) {}
    alogger(const alogger& other) {
        level = other.level;
        name = other.name;
        bugor = other.bugor;
    }

    void set(const int l) {
        level = l;
    }

    bool enabled(const int for_wha) {
        return (level >= for_wha);
    }

    void trace(const char *fmt, ...) {
        if (enabled(LL_TRACE)) {
            va_list ap;
            va_start(ap, fmt);
            bugor->vlog_message(LL_TRACE, name, fmt, ap);
            va_end(ap);
        }
    }

    void info(const char *fmt, ...) {
        if (enabled(LL_INFO)) {
            va_list ap;
            va_start(ap, fmt);
            bugor->vlog_message(LL_INFO, name, fmt, ap);
            va_end(ap);
        }
    }

    void warn(const char *fmt, ...) {
        if (enabled(LL_WARN)) {
            va_list ap;
            va_start(ap, fmt);
            bugor->vlog_message(LL_WARN, name, fmt, ap);
            va_end(ap);
        }
    }

    void error(const char *fmt, ...) {
        if (enabled(LL_ERROR)) {
            va_list ap;
            va_start(ap, fmt);
            bugor->vlog_message(LL_ERROR, name, fmt, ap);
            va_end(ap);
        }
    }

    NORETURN void fatal(const char *fmt, ...) {
        va_list ap;

        va_start(ap, fmt);
        bugor->vlog_message(LL_FATAL, name, fmt, ap);
        va_end(ap);
        abort();
    }
};

struct log_implementation : public ilogsink {
    FILE *dst;
    iplatform *plat;

    std::unordered_map<std::string, alogger *> config;
    typedef std::unordered_map<std::string,  alogger *>::const_iterator lchashi_t;
    typedef std::unordered_map<std::string,  alogger *>::value_type lchashp_t;
    alogger *root_logger;

    const char *ll_names(int l) {
        switch (l) {
        case LL_TRACE: return "trace";
        case LL_INFO:  return "warn ";
        case LL_WARN:  return "info ";
        case LL_ERROR: return "error";
        case LL_FATAL: return "fatal";
        default:       return "~unk~";
        }
    }

    log_implementation(iplatform *p, FILE *d): config() {
        plat = p;
        dst = d;
        root_logger = new alogger("", LL_WARN, this);
        lchashp_t iv(std::string(""), root_logger);
        config.insert(iv);
    }

    void logconf(const char *n, int level)  {
        if ((n == NULL) || (strlen(n) == 0)) {
            root_logger->level = level;
            return;
        }
        std::string name(n);
        lchashi_t got = config.find(name);
        if (got != config.end())
            got->second->set(level);
        else {
            lchashp_t iv(name, new alogger(name, level, this));
            config.insert(iv);
        }
    }

    ilogger *getlogr(const char *n) {
        if ((n == NULL) || (strlen(n) == 0))
            return root_logger;

        std::string name(n);
        std::string prefix;
        size_t pos = std::string::npos;

        lchashi_t got = config.find(name);
        if (got != config.end())
            return got->second;

        /* generate a logger since it doesn't exist yet.
           inherit level from whatever ancestor we can find. */
        alogger *rv = root_logger;

        while ((pos = name.rfind('.', pos)) != std::string::npos) {
            prefix = name.substr(0, pos);
            got = config.find(prefix);
            if (got != config.end()) {
                rv = got->second;
                break;
            }
            pos --;
        }
        rv = new alogger(name, rv->level, rv->bugor);
        lchashp_t iv(std::string(name), rv);
        config.insert(iv);
        return rv;
    }

    void vlog_message(int level, const std::string& name, const char *fmt, va_list ap) {
        fprintf(dst, "%06d %s [%s]: ", plat->GetTickCount() % 1000000, name.c_str(), ll_names(level));
        vfprintf(dst, fmt, ap);
        fputc('\n', dst);
        va_end(ap);
        if (level == LL_FATAL)
            fflush(dst);
    }
};
