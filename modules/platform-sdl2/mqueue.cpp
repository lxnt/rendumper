#include <vector>
#include <queue>

#include "SDL_thread.h"
#include "SDL_mutex.h"
#include "SDL_atomic.h"
#include "SDL_timer.h"


#include "errno.h"
#include "string.h"

#include "iplatform.h"

#define DFMODULE_BUILD
#include "imqueue.h"

extern iplatform *platform;

namespace {

const char *strmsgt(const itc_message_t *msg) {
    switch (msg->t) {
    case itc_message_t::quit: return "quit";
    case itc_message_t::complete: return "complete";
    case itc_message_t::set_fps: return "set_fps";
    case itc_message_t::set_gfps: return "set_gfps";
    case itc_message_t::push_resize: return "push_resize";
    case itc_message_t::pop_resize: return "pop_resize";
    case itc_message_t::reset_textures: return "reset_textures";
    case itc_message_t::pause: return "pause";
    case itc_message_t::start: return "start";
    case itc_message_t::render: return "render";
    case itc_message_t::inc: return "inc";
    case itc_message_t::zoom_in: return "zoom_in";
    case itc_message_t::zoom_out: return "zoom_out";
    case itc_message_t::zoom_reset: return "zoom_reset";
    case itc_message_t::zoom_fullscreen: return "zoom_fullscreen";
    case itc_message_t::zoom_resetgrid: return "zoom_resetgrid";
    case itc_message_t::render_buffer: return "render_buffer";
    case itc_message_t::input_event: return "input_event";
    default: return "~unk~";
    }
}

const char *strievtype(const itc_message_t *msg) {
    switch (msg->d.event.type) {
    case df_input_event_t::DF_KEY_UP: return "DF_KEY_UP";
    case df_input_event_t::DF_KEY_DOWN: return "DF_KEY_DOWN";
    case df_input_event_t::DF_BUTTON_UP: return "DF_BUTTON_UP";
    case df_input_event_t::DF_BUTTON_DOWN: return "DF_BUTTON_DOWN";
    case df_input_event_t::DF_QUIT: return "DF_QUIT";
    default: return "~unk~";
    }
}

void mq_trace(const char *wha, imqd_t qd, void *vbuf, size_t len) {
    ilogger *logr = platform->getlogr("mq.trace");
    if (!logr->enabled(LL_TRACE))
        return;

    uint8_t *buf = (uint8_t *)vbuf;

    char hexdump[3*8 + 1];
    memset(hexdump, 0, sizeof(hexdump));

    for (unsigned i = 0; i < ( len < 8 ? len : 8 ); i++)
        sprintf(hexdump + i*3, "%02x ", (unsigned)buf[i]);

    if (sizeof(void *) == len) {
        void *p = *( (void **) buf );
        logr->trace("mq_%s(%d): %p[%u] (asptr %p) %s", wha, qd, buf, len, p, hexdump);
        return;
    }

    if (sizeof(itc_message_t) == len) {
        itc_message_t *m = (itc_message_t *) vbuf;
        switch (m->t) {
        case itc_message_t::input_event:
            logr->trace("mq_%s(%d): %p[%u] (asmsg t=%s sender=%p { event=%s }) %s",
                wha, qd, buf, len, strmsgt(m), m->sender, strievtype(m), hexdump);
            break;
        case itc_message_t::set_fps:
        case itc_message_t::set_gfps:
            logr->trace("mq_%s(%d): %p[%u] (asmsg t=%s sender=%p { fps=%d }) %s",
                wha, qd, buf, len, strmsgt(m), m->sender, m->d.fps, hexdump);
            break;
        case itc_message_t::render_buffer:
            logr->trace("mq_%s(%d): %p[%u] (asmsg t=%s sender=%p { buffer=%p }) %s",
                wha, qd, buf, len, strmsgt(m), m->sender, m->d.buffer, hexdump);
            break;
        default:
            logr->trace("mq_%s(%d): %p[%u] (asmsg t=%s sender=%p { }) %s",
                wha, qd, buf, len, strmsgt(m), m->sender, hexdump);
            break;
        }
        return;
    }

    logr->trace("mq_%s(%d): %p[%u] %s", wha, qd, buf, len, hexdump);
}

struct the_message {
    void *buf;
    size_t len;
    the_message() { buf = NULL; len = 0;}
    the_message(const the_message &o) { buf = o.buf, len = o.len; }
    the_message(void *halla, size_t balla) : buf(halla), len(balla) { }
};

struct the_queue {
    int refcount;               // protected by implementation::queues_mtx
    bool unlinked;              // protected by implementation::queues_mtx
    size_t max_messages;        // read-only
    char *name;                 // read-only
    imqd_t qd;                  // read-only

    std::queue<the_message> messages;
    SDL_mutex *messages_mtx;

    SDL_cond *readable_cond;
    SDL_mutex *readable_mtx;
    bool readable;

    SDL_cond *writable_cond;
    SDL_mutex *writable_mtx;
    bool writable;

    the_queue(const char *_name, size_t _max_messages, imqd_t _qd) : messages() {
        name = strdup(_name);
        max_messages = _max_messages;
        qd = _qd;
        unlinked = false;
        refcount = 1;
        messages_mtx = SDL_CreateMutex();
        readable_mtx = SDL_CreateMutex();
        writable_mtx = SDL_CreateMutex();
        readable_cond = SDL_CreateCond();
        writable_cond = SDL_CreateCond();
        readable = false;
        writable = true;
    }

    ~the_queue() {
        SDL_DestroyMutex(messages_mtx);
        SDL_DestroyMutex(readable_mtx);
        SDL_DestroyMutex(writable_mtx);
        SDL_DestroyCond(readable_cond);
        SDL_DestroyCond(writable_cond);
    }
    int pop(the_message& into, int timeout);
    int push(the_message what, int timeout);
};

static int wait_on_cond(bool *what, SDL_cond *cond, SDL_mutex *mutex, int timeout) {
    int rv = 0;

    SDL_LockMutex(mutex);
    if (!*what) {
        if (timeout == 0) {
            SDL_UnlockMutex(mutex);
            return IMQ_TIMEDOUT;
        }
        if (timeout < 1) {
            while ((!*what) && (rv == 0))
                rv = SDL_CondWait(cond, mutex);
            if ((!*what) && (rv != 0)) {
                /* man pthread_cond_wait says:
                        Except in the case of [ETIMEDOUT], all these error checks shall act  as
                        if  they  were performed immediately at the beginning of processing for
                        the function and shall cause an error return, in effect, prior to modi-
                        fying  the state of the mutex specified by mutex or the condition vari-
                        able specified by cond.

                    I take it since the state of the mutex hasn't been modified, we still have to
                    unlock it before returning. */
                SDL_UnlockMutex(mutex);
                platform->getlogr("mq.wait_on_cond")->error("timeout=%d, rv=%d, clowns detected.", timeout, rv);
                return IMQ_CLOWNS;
            }
        } else {
            /* man pthread_cond_timedwait says:
                    TLDR, races, unavoidable, blah, blah;

                    It is thus recommended that a condition wait be enclosed in the equivalent
                    of a "while loop" that checks the predicate. */
            unsigned abstime = timeout + SDL_GetTicks();
            while ((!*what) && (rv == 0) && (timeout > 0)) {
                rv = SDL_CondWaitTimeout(cond, mutex, timeout);
                timeout = abstime - SDL_GetTicks();
            }
            if (!*what) {
                SDL_UnlockMutex(mutex);
                if ((timeout < 1) || (rv == SDL_MUTEX_TIMEDOUT))
                    return IMQ_TIMEDOUT;
                /* unknown error */
                /* got here with
                    118098 mq.wait_on_cond_timed [error]: timeout=6, rv=-1, errno=Success, clowns detected.
                    118098 mq.the_queue [error]: pop(): rv=-100500 (known)
                    118098 mq.recv [error]: q->pop(): rv=-100500 (known)
                    118098 cc.simuloop [fatal]: -167800200 from mqueue->recv().
                */

                platform->getlogr("mq.wait_on_cond_timed")->error("timeout=%d, rv=%d, errno=%s (%d), SDL Error '%s', clowns detected.",
                    timeout, rv, strerror(errno), errno, SDL_GetError());
                //return IMQ_CLOWNS;
                return IMQ_TIMEDOUT;
            }
        }
        /*  'what' is true, but we may have sdl_rv != 0. In this case
            we proceed as normal, since whatever error that had been reported
            has no bearing on the 'what' state. And we still have to unlock
            the mutex. */
    }
    return IMQ_OK;
}

int the_queue::pop(the_message& into, int timeout) {
    int rv = wait_on_cond(&readable, readable_cond, readable_mtx, timeout);

    if (rv == IMQ_OK) {
        SDL_LockMutex(messages_mtx); // someone might be sending a message
        if (messages.size() > 0) {
            if (max_messages && (messages.size() == max_messages - 1)) {
                SDL_LockMutex(writable_mtx);
                writable = true;
                /* man pthread_cond_signal says the mutex be locked
                   if predictable scheduling is to be hoped for. */
                SDL_CondSignal(writable_cond);
                SDL_UnlockMutex(writable_mtx);
            }
            into = messages.front();
            messages.pop();
            if (messages.size() == 0)
                readable = false;
        } else {
            readable = false;
            rv = IMQ_TIMEDOUT;
        }
        SDL_UnlockMutex(messages_mtx);
        SDL_UnlockMutex(readable_mtx);
    } else {
        switch (rv) {
        case IMQ_OK:
        case IMQ_TIMEDOUT:
            break;
        case IMQ_EXIST:
        case IMQ_NOENT:
        case IMQ_BADF:
        case IMQ_INVAL:
        case IMQ_CLOWNS:
            platform->getlogr("mq.the_queue")->error("pop(): rv=%d (known)", rv);
            break;
        default:
            platform->getlogr("mq.the_queue")->error("pop(): rv=%d", rv);
            break;
        }
    }
    return rv;
}

int the_queue::push(the_message what, int timeout) {
    int rv = wait_on_cond(&writable, writable_cond, writable_mtx, timeout);

    if (rv == IMQ_OK) {
        SDL_LockMutex(messages_mtx);
        messages.push(what);
        if (messages.size() == max_messages)
            writable = false;
        if (messages.size() == 1) {
            SDL_LockMutex(readable_mtx);
            readable = true;
            SDL_CondSignal(readable_cond);
            SDL_UnlockMutex(readable_mtx);
        }
        SDL_UnlockMutex(messages_mtx);
        SDL_UnlockMutex(writable_mtx);
    }
    return rv;
}

struct implementation : public imqueue {
  private:
    std::vector<the_queue *> queues;
    SDL_mutex *queues_mtx;

    virtual ~implementation();
    implementation(implementation&) {};
    the_queue *find_queue(const char *name);
    the_queue *get_queue(imqd_t qd);

  public:
    implementation();
    void release();

    imqd_t open(const char *name, int maxmsg);
    int close(imqd_t qd);
    int unlink(const char *name);
    int send(imqd_t qd, void *buf, size_t len, int timeout);
    int copy(imqd_t qd, void *buf, size_t len, int timeout);
    int recv(imqd_t qd, void **buf, size_t *len, int timeout);
    void free(void *buf) { ::free(buf); }
};

the_queue *implementation::find_queue(const char *name) {
    for (size_t i=0; i<queues.size(); i++)
        if ((not queues[i]->unlinked) && (0 == strcmp(name, queues[i]->name)))
            return queues[i];
    return NULL;
}

imqd_t implementation::open(const char *name, int max_messages) {
    if ((name == 0) || (max_messages < 1))
        return IMQ_INVAL;

    SDL_LockMutex(queues_mtx);
    the_queue *q = find_queue(name);
    imqd_t rv = IMQ_CLOWNS;
    if (q) {
        if (max_messages > 0) { // O_CREAT | O_EXCL
            rv = IMQ_EXIST;
        } else {
            q->refcount += 1;
            rv = q->qd;
        }
    } else {
        if (max_messages > 0) {
            q = new the_queue(name, max_messages, queues.size());
            queues.push_back(q);
            rv = q->qd;
        } else {
            rv = IMQ_NOENT;
        }
    }
    SDL_UnlockMutex(queues_mtx);
    return rv;
}

int implementation::close(imqd_t qd) {
    SDL_LockMutex(queues_mtx);
    if (qd >= (int)queues.size()) {
        SDL_UnlockMutex(queues_mtx);
        return IMQ_BADF;
    }
    queues[qd]->refcount -= 1;
    if (queues[qd]->refcount == 0) {
        if (queues[qd]->unlinked) {
            delete queues[qd];
            queues[qd] = NULL; // keep qd from being reused.
        }
    }
    SDL_UnlockMutex(queues_mtx);
    return IMQ_OK;
}

int implementation::unlink(const char *name) {
    SDL_LockMutex(queues_mtx);
    the_queue *q = find_queue(name);
    if (q == NULL) {
        SDL_UnlockMutex(queues_mtx);
        return IMQ_NOENT;
    }
    q->unlinked = true;
    if (q->refcount == 0) {
        imqd_t qd = q->qd;
        delete q;
        queues[qd] = NULL;
    }
    SDL_UnlockMutex(queues_mtx);
    return IMQ_OK;
}

the_queue *implementation::get_queue(imqd_t qd) {
    SDL_LockMutex(queues_mtx);
    if (qd >= (int)queues.size()) {
        SDL_UnlockMutex(queues_mtx);
        return NULL;
    }
    the_queue *q = queues[qd];
    SDL_UnlockMutex(queues_mtx);
    return q;
}

int implementation::send(imqd_t qd, void *buf, size_t len, int timeout) {
    mq_trace("send", qd, buf, len);
    if ((buf == NULL) || (len == 0))
        return IMQ_INVAL;

    the_queue *q = get_queue(qd);
    if (q == NULL)
        return IMQ_BADF;

    the_message m(buf, len);
    return q->push(m, timeout);
}

int implementation::copy(imqd_t qd, void *buf, size_t len, int timeout) {
    mq_trace("copy", qd, buf, len);
    void *new_buf = malloc(len);
    memmove(new_buf, buf, len);
    int rv = send(qd, new_buf, len, timeout);
    if (rv != IMQ_OK)
        free(new_buf);
    return rv;
}

int implementation::recv(imqd_t qd, void **buf, size_t *len, int timeout) {
    if ((buf == NULL) || (len == NULL))
        return IMQ_INVAL;

    the_queue *q = get_queue(qd);
    if (q == NULL)
        return IMQ_BADF;

    the_message m;
    int rv = q->pop(m, timeout);
    if (rv) {
        switch (rv) {
        case IMQ_OK:
        case IMQ_TIMEDOUT:
            break;
        case IMQ_EXIST:
        case IMQ_NOENT:
        case IMQ_BADF:
        case IMQ_INVAL:
        case IMQ_CLOWNS:
            platform->getlogr("mq.recv")->error("q->pop(): rv=%d (known)", rv);
            break;
        default:
            platform->getlogr("mq.recv")->error("q->pop(): rv=%d (wtf)", rv);
            break;
        }
        return rv;
    }

    *buf = m.buf;
    *len = m.len;
    mq_trace("recv", qd, m.buf, m.len);
    return IMQ_OK;
}

static implementation *impl = NULL;
static int impl_refcount = 0;
static SDL_SpinLock impl_spinlock = 0;

implementation::implementation() : queues() {
    queues_mtx = SDL_CreateMutex();
}

implementation::~implementation() {
    SDL_DestroyMutex(queues_mtx);
    for (size_t i = 0; i<queues.size(); i++)
        if (queues[i] != NULL)
            delete queues[i];
}

void implementation::release() {
    SDL_AtomicLock(&impl_spinlock);
    impl_refcount -= 1;
    if (impl_refcount == 0) {
        delete impl;
        impl = NULL;
    }
    SDL_AtomicUnlock(&impl_spinlock);
}

extern "C" DFM_EXPORT imqueue * DFM_APIEP getmqueue(void) {
    SDL_AtomicLock(&impl_spinlock);
    if (!impl)
        impl = new implementation();
    impl_refcount += 1;
    SDL_AtomicUnlock(&impl_spinlock);
    return impl;
}
} /* ns */
