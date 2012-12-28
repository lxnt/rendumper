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

//#include <cstdarg>
//#include <string>
//#include <unordered_map>
//#include <forward_list>

//#include "iplatform.h"
#include <cstring>
#include <cstdio>
#include "logging.h"

#if defined(LOGGING_LOGGING)
#define LTRACE(fmt, args...) do { fprintf(stderr, fmt, ## args); } while(0)
#define LTRACE_DUMP(fmt, args...) do { fprintf(stderr, fmt, ## args); dump_config(); } while(0)
#else
#define LTRACE(fmt, args...)
#define LTRACE_DUMP(fmt, args...)
#endif

alogger::alogger(const char *n, int l, int e, ilogsink *p) {
    level = l;
    effective = e;
    name = n;
    bugor = p;
}

alogger::alogger(const std::string& n, int l, int e, ilogsink *p) {
    level = l;
    effective = e;
    name = n;
    bugor = p;
}

bool alogger::enabled(const int for_wha) {
    return (effective <= for_wha);
}

void alogger::trace(const char *fmt, ...) {
    if (enabled(LL_TRACE)) {
        va_list ap;
        va_start(ap, fmt);
        bugor->vlog_message(LL_TRACE, name, fmt, ap);
        va_end(ap);
    }
}

void alogger::info(const char *fmt, ...) {
    if (enabled(LL_INFO)) {
        va_list ap;
        va_start(ap, fmt);
        bugor->vlog_message(LL_INFO, name, fmt, ap);
        va_end(ap);
    }
}

void alogger::warn(const char *fmt, ...) {
    if (enabled(LL_WARN)) {
        va_list ap;
        va_start(ap, fmt);
        bugor->vlog_message(LL_WARN, name, fmt, ap);
        va_end(ap);
    }
}

void alogger::error(const char *fmt, ...) {
    if (enabled(LL_ERROR)) {
        va_list ap;
        va_start(ap, fmt);
        bugor->vlog_message(LL_ERROR, name, fmt, ap);
        va_end(ap);
    }
}

NORETURN void alogger::fatal(const char *fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    bugor->vlog_message(LL_FATAL, name, fmt, ap);
    va_end(ap);
    abort();
}

const char *log_implementation::ll_names(int l) {
    switch (l) {
    case LL_NONE:   return "none ";
    case LL_TRACE:  return "trace";
    case LL_INFO:   return "warn ";
    case LL_WARN:   return "info ";
    case LL_ERROR:  return "error";
    case LL_FATAL:  return "fatal";
    default:        return "~unk~";
    }
}

log_implementation::log_implementation(iplatform *p, FILE *d) {
    platform = p;
    dst = d;
    root_logger = new alogger("", LL_WARN, LL_WARN, this);
}

void log_implementation::dump_conf() {
#if defined(LOGGING_LOGGING)
    for (auto logr = config.cbegin(); logr != config.cend(); ++logr)
        fprintf(stderr, "confdump: '%s' l=%d e=%d\n",
                logr->first.c_str(), logr->second->level,
                                    logr->second->effective);
#endif
}

void log_implementation::reconf_chain(alogger *leaf) {
    /*  requires logging be locked up.
        is the most expensive operation here.

        - creates any missing ancestors.
        - recalculates effective logging levels for
          the relevant part (up or down) part of chain.

        - is triggered by logconf() unconditionally.
        - is triggered by getlogr() if logger in
          question does not exist at the time of call.
    */

    std::forward_list<std::string> descendants;

    LTRACE("reconf_chain(%s)\n", leaf->name.c_str());

    if (leaf == root_logger) {
        if ( (leaf->effective != leaf->level) ||
             (leaf->effective == LL_NONE) ) {
                 /* aw, it's all fubar */
                LTRACE("logging.h: root's fubar.\n");
                abort();
             }

        /* total reconf */
        for (auto logr = config.cbegin(); logr != config.cend(); ++logr)
            descendants.push_front(logr->first);
    } else {

        /* make a list of logger names for this chain.
            - first, all ancestors, inserting missing as we go
            - second, all existing descendants
            missing descendants - they can't exist since we
            inserted them as missing ancestors right here.

            so basically we first find the farthest descendant.
            if it exists, we're not leaf, all ancestors must exist,
            and we should reconfigure all descendants with
            level == NOTSET down until first one level != NONE

            if we don't have any descendants we've just been created
            and can have gaps in ancestry which must be fixed.

            for this we create a descending list of names which are
            going to have to be fixed and fix'em.
        */

        std::string our_prefix(leaf->name);
        our_prefix += '.';
        bool has_descendants = false;

        for (auto logr = config.cbegin(); logr != config.cend(); ++logr) {
            if (logr->first.size() <= our_prefix.size())
                continue;

            if (0 == logr->first.compare(0, our_prefix.size(), our_prefix)) {
                descendants.push_front(logr->first);
                has_descendants = true;
            }
        }

        if (!has_descendants)
            descendants.push_front(leaf->name);
    }

    LTRACE("reconf_chain(%s) descendants (%u) list:\n", leaf->name.c_str(), config.size());
#if defined(LOGGING_LOGGING)
    for (auto boo = descendants.cbegin(); boo != descendants.cend(); ++boo) {
        LTRACE("reconf_chain(%s): %s\n", leaf->name.c_str(), boo->c_str());
    }
#endif
    fflush(stderr);
    /* don't weed out non-leaf descendants - not worth it. */
    for (auto logr = descendants.begin(); logr != descendants.end(); ++logr) {
        LTRACE("reconfing %s\n", logr->c_str());

        size_t lpos, pos;
        std::string ancestor;
        int effective_so_far = root_logger->effective;
        lpos = 0;
        do {
            pos = logr->find('.', lpos);
            ancestor = logr->substr(0, pos);
            LTRACE("pos=%u lpos=%u ancestor='%s'\n", pos, lpos, ancestor.c_str());
            auto got = config.find(ancestor);
            if (got != config.end()) {
                if (got->second->level == LL_NONE)
                    got->second->effective = effective_so_far;
                else
                    effective_so_far = got->second->effective = got->second->level;
            } else {
                config.insert({{ ancestor, new alogger(ancestor, LL_NONE, effective_so_far, this) }});
            }
            lpos = pos + 1;
        } while (pos != std::string::npos);
    }
}

ilogger *log_implementation::logconf(const char *n, int level)  {
    LTRACE_DUMP("logconf(%s, %d): %u loggers out.\n", n, level, config.size());
    if ((n == NULL) || (strlen(n) == 0)) {
        if (root_logger->level != level) {
            root_logger->level = root_logger->effective = level;
            platform->lock_logging();
            reconf_chain(root_logger);
            platform->unlock_logging();
            LTRACE_DUMP("logconf(%s, %d): after: %u loggers out.\n", n, level, config.size());
        }
        return root_logger;
    }
    std::string name(n);

    platform->lock_logging();

    auto got = config.find(name);
    alogger *rv;
    if (got != config.end()) {
        rv = got->second;
        rv->level = level;
        LTRACE("logconf(%s, %d): found it.\n", n, level);
    } else {
        rv = new alogger(name, level, level, this);
        config.insert({{ name, rv }});
        LTRACE("logconf(%s, %d): created it.\n", n, level);
    }

    reconf_chain(rv);

    platform->unlock_logging();

    LTRACE_DUMP("logconf(%s, %d): after: %u loggers out.\n", n, level, config.size());
    return rv;
}

ilogger *log_implementation::getlogr(const char *n) {
    if ((n == NULL) || (strlen(n) == 0))
        return root_logger;

    std::string name(n);

    platform->lock_logging();

    auto got = config.find(name);

    platform->unlock_logging();

    if (got != config.end())
        return got->second;

    /* generate a logger since it doesn't exist yet.
       inherit level from whatever ancestor we can find. */

    LTRACE("getlogr(%s) triggers logconf()", n);
    return logconf(n, LL_NONE);
}

void log_implementation::vlog_message(int level, const std::string& name, const char *fmt, va_list ap) {
    fprintf(dst, "%06lu %s [%s]: ", platform->GetTickCount() % 1000000, name.c_str(), ll_names(level));
    vfprintf(dst, fmt, ap);
    fputc('\n', dst);
    va_end(ap);
    if (level == LL_FATAL)
        fflush(dst);
}

