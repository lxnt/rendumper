#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <vector>
#include <queue>
#include <pthread.h>
#include <sys/time.h>


#define DFMODULE_BUILD
#include "iplatform.h"


struct the_message {
    void *buf;
    size_t len;
    the_message() { buf = NULL; len = 0;}
    the_message(void *halla, size_t balla) : buf(halla), len(balla) { }
};

static void mutex_init(pthread_mutex_t *mutex) {
    int rv;
    if ((rv = pthread_mutex_init(mutex, NULL))) {
        fprintf(stderr, "pthread_mutex_init(): %s\n", strerror(rv));
        exit(1);
    }
}

static void mutex_destroy(pthread_mutex_t *mutex) {
    int rv;
    if ((rv = pthread_mutex_destroy(mutex))) {
        fprintf(stderr, "pthread_mutex_destroy(): %s\n", strerror(rv));
        exit(1);
    }
}

static int mutex_lock(pthread_mutex_t *mutex) {
    return pthread_mutex_lock(mutex);
}

static int mutex_unlock(pthread_mutex_t *mutex) {
    return pthread_mutex_unlock(mutex);
}

static void cond_init(pthread_cond_t *cond) {
    int rv;
    if ((rv = pthread_cond_init(cond, NULL))) {
        fprintf(stderr, "pthread_cond_init(): %s\n", strerror(rv));
        exit(1);
    }
}

static void cond_destroy(pthread_cond_t *cond) {
    int rv;
    if ((rv = pthread_cond_destroy(cond))) {
        fprintf(stderr, "pthread_cond_destroy(): %s\n", strerror(rv));
        exit(1);
    }
}

static void cond_signal(pthread_cond_t *cond) {
    int rv;
    if ((rv = pthread_cond_signal(cond))) {
        fprintf(stderr, "pthread_cond_destroy(): %s\n", strerror(rv));
        exit(1);
    }
}

static int cond_wait(pthread_cond_t *cond, pthread_mutex_t * mutex) {
    int rv;
    if ((rv = pthread_cond_wait(cond, mutex))) {
        fprintf(stderr, "pthread_cond_wait(): %s\n", strerror(rv));
        exit(1);
    }
    return 0;
}

static int cond_timedwait(pthread_cond_t *cond, pthread_mutex_t * mutex, struct timeval& abstime) {
    struct timespec abstime_ns;
    abstime_ns.tv_sec = abstime.tv_sec;
    abstime_ns.tv_nsec = abstime.tv_usec * 1000;

    int rv = pthread_cond_timedwait(cond, mutex, &abstime_ns);
    if ((rv != 0 ) && (rv != ETIMEDOUT)) {
        fprintf(stderr, "pthread_cond_timewait(): %s\n", strerror(rv));
        exit(1);
    }
    return rv;
}

struct the_queue {
    int refcount;               // protected by implementation::queues_mtx
    bool unlinked;              // protected by implementation::queues_mtx
    size_t max_messages;        // read-only
    char *name;                 // read-only
    imqd_t qd;                  // read-only

    std::queue<the_message> messages;
    pthread_mutex_t messages_mtx;

    pthread_cond_t readable_cond;
    pthread_mutex_t readable_mtx;
    bool readable;

    pthread_cond_t writable_cond;
    pthread_mutex_t writable_mtx;
    bool writable;

    the_queue(const char *_name, size_t _max_messages, imqd_t _qd) {
        name = strdup(_name);
        max_messages = _max_messages;
        qd = _qd;
        unlinked = false;
        refcount = 1;

        mutex_init(&messages_mtx);
        mutex_init(&readable_mtx);
        mutex_init(&writable_mtx);
        cond_init(&readable_cond);
        cond_init(&writable_cond);
        readable = false;
        writable = true;
    }

    ~the_queue() {
        mutex_destroy(&messages_mtx);
        mutex_destroy(&readable_mtx);
        mutex_destroy(&writable_mtx);
        cond_destroy(&readable_cond);
        cond_destroy(&writable_cond);
    }
    int pop(the_message& into, int timeout);
    int push(the_message what, int timeout);
};

static int wait_on_cond(bool *what, pthread_cond_t *cond, pthread_mutex_t *mutex, int timeout) {
    int rv = 0;

    mutex_lock(mutex);
    if (!*what) {
        if (timeout == 0) {
            mutex_unlock(mutex);
            return IMQ_TIMEDOUT;
        }
        if (timeout < 1) {
            while ((!*what) && (rv == 0))
                rv = cond_wait(cond, mutex);
            if ((!*what) && (rv != 0)) {
                /* man pthread_cond_wait says:
                        Except in the case of [ETIMEDOUT], all these error checks shall act  as
                        if  they  were performed immediately at the beginning of processing for
                        the function and shall cause an error return, in effect, prior to modi-
                        fying  the state of the mutex specified by mutex or the condition vari-
                        able specified by cond.

                    I take it since the state of the mutex hasn't been modified, we still have to
                    unlock it before returning. */
                mutex_unlock(mutex);
                return IMQ_CLOWNS;
            }
        } else {
            /* reimplement pthread_cond_timedwait()'s abstime. dunno why. */
            struct timeval abstime;
            gettimeofday(&abstime, NULL);
            abstime.tv_sec += timeout / 1000;
            abstime.tv_usec += 1000 * (timeout % 1000);
            while ((!*what) && (rv == 0))
                rv = cond_timedwait(cond, mutex, abstime);
            if (!*what) {
                mutex_unlock(mutex);
                if ((timeout < 1) || (rv == ETIMEDOUT))
                    return IMQ_TIMEDOUT;
                /* unknown error */
                return IMQ_CLOWNS;
            }
        }
        /*  '*what' is true, but we may have rv != 0. In this case
            we proceed as normal, since whatever error that had been reported
            has no bearing on the 'what' state. And we still have to unlock
            the mutex. */
    }
    return IMQ_OK;
}

int the_queue::pop(the_message& into, int timeout) {
    int rv = wait_on_cond(&readable, &readable_cond, &readable_mtx, timeout);

    if (rv == IMQ_OK) {
        mutex_lock(&messages_mtx); // someone might be sending a message
        if (messages.size() > 0) {
            if (max_messages && (messages.size() == max_messages - 1)) {
                mutex_lock(&writable_mtx);
                writable = true;
                /* man pthread_cond_signal says the mutex be locked
                   if predictable scheduling is to be hoped for. */
                cond_signal(&writable_cond);
                mutex_unlock(&writable_mtx);
            }
            into = messages.front();
            messages.pop();
            if (messages.size() == 0)
                readable = false;
        } else {
            readable = false;
            rv = IMQ_TIMEDOUT;
        }
        mutex_unlock(&messages_mtx);
        mutex_unlock(&readable_mtx);
    }
    return rv;
}

int the_queue::push(the_message what, int timeout) {
    int rv = wait_on_cond(&writable, &writable_cond, &writable_mtx, timeout);

    if (rv == IMQ_OK) {
        mutex_lock(&messages_mtx);
        messages.push(what);
        if (messages.size() == max_messages)
            writable = false;
        if (messages.size() == 1) {
            mutex_lock(&readable_mtx);
            readable = true;
            cond_signal(&readable_cond);
            mutex_unlock(&readable_mtx);
        }
        mutex_unlock(&messages_mtx);
        mutex_unlock(&writable_mtx);
    }
    return rv;
}

struct implementation : public imqueue {
  private:
    std::vector<the_queue *> queues;
    pthread_mutex_t queues_mtx;
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
    void free(void *buf) { free(buf); }
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

    mutex_lock(&queues_mtx);
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
    mutex_unlock(&queues_mtx);
    return rv;
}

int implementation::close(imqd_t qd) {
    mutex_lock(&queues_mtx);
    if (qd >= queues.size()) {
        mutex_unlock(&queues_mtx);
        return IMQ_BADF;
    }
    queues[qd]->refcount -= 1;
    if (queues[qd]->refcount == 0) {
        if (queues[qd]->unlinked) {
            delete queues[qd];
            queues[qd] = NULL; // keep qd from being reused.
        }
    }
    mutex_unlock(&queues_mtx);
    return IMQ_OK;
}

int implementation::unlink(const char *name) {
    mutex_lock(&queues_mtx);
    the_queue *q = find_queue(name);
    if (q == NULL) {
        mutex_unlock(&queues_mtx);
        return IMQ_NOENT;
    }
    q->unlinked = true;
    if (q->refcount == 0) {
        imqd_t qd = q->qd;
        delete q;
        queues[qd] = NULL;
    }
    mutex_unlock(&queues_mtx);
    return IMQ_OK;
}

the_queue *implementation::get_queue(imqd_t qd) {
    mutex_lock(&queues_mtx);
    if (qd >= queues.size()) {
        mutex_unlock(&queues_mtx);
        return NULL;
    }
    the_queue *q = queues[qd];
    mutex_unlock(&queues_mtx);
    return q;
}

int implementation::send(imqd_t qd, void *buf, size_t len, int timeout) {
    if ((buf == NULL) || (len == 0))
        return IMQ_INVAL;

    the_queue *q = get_queue(qd);
    if (q == NULL)
        return IMQ_BADF;

    the_message m(buf, len);
    return q->push(m, timeout);
}

int implementation::copy(imqd_t qd, void *buf, size_t len, int timeout) {
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
    if (rv)
        return rv;

    *buf = m.buf;
    *len = m.len;
    return IMQ_OK;
}

static implementation *impl = NULL;
static int impl_refcount = 0;
static pthread_mutex_t impl_spinlock = PTHREAD_MUTEX_INITIALIZER;

implementation::implementation() {
    mutex_init(&queues_mtx);
}

implementation::~implementation() {
    mutex_destroy(&queues_mtx);
    for (size_t i = 0; i<queues.size(); i++)
        if (queues[i] != NULL)
            delete queues[i];
}

void implementation::release() {
    mutex_lock(&impl_spinlock);
    impl_refcount -= 1;
    if (impl_refcount == 0) {
        delete impl;
        impl = NULL;
    }
    mutex_unlock(&impl_spinlock);
}

extern "C" DECLSPEC imqueue * APIENTRY   getmqueue(void) {
    mutex_lock(&impl_spinlock);
    if (!impl)
        impl = new implementation();
    impl_refcount += 1;
    mutex_unlock(&impl_spinlock);
    return impl;
}
