#if !defined(IMQUEUE_H)
#define IMQUEUE_H

#include <stddef.h>
#include <stdint.h>
#include "itypes.h"

/* Interthread message queues */

typedef int imqd_t;
enum mq_errors {
    IMQ_OK               = 0,
    IMQ_EXIST            = -17,    /* open() with maxmsg > 0  and queue already exists. */
    IMQ_NOENT            = -2,     /* open() with maxmsg == 0  and queue doesn't exist. */
    IMQ_TIMEDOUT         = -110,   /* ETIMEDOUT or timeout == 0 and no messages/no space in q. */
    IMQ_BADF             = -8,     /* EBADF bogus imqd supplied. */
    IMQ_INVAL            = -22,    /* EINVAL : null name, null buf or zero len */
    IMQ_CLOWNS           = -100500 /* unknown error, most likely from the implementation's backend */
};

/* interface loosely modelled on POSIX mqueues */

struct imqueue {
    virtual void release() = 0;

    /* max_messages == 0: open an existing queue or IMQ_NOENT, increment refcount
       max_messages >  0: create a new queue or IMQ_EXIST */
    virtual imqd_t open(const char *name, int max_messages) = 0;

    /* close: decrement refcount, destroy if it reaches 0 and queue is unlinked. */
    virtual int close(imqd_t qd) = 0;

    /* unlink, destroy if refcount == 0. Another queue with the same name can be created. */
    virtual int unlink(const char *name) = 0;

    /* timeout is in milliseconds.
        timeout > 0:    wait
        timeout == 0:   return at once
        timeout < 0:    block. */

    /* send a message contained in (buf, len), sender gives up ownership of the buffer. */
    virtual int send(imqd_t qd, void *buf, size_t len, int timeout) = 0;

    /* sender retains ownership of the buffer, implementation copies it. */
    virtual int copy(imqd_t qd, void *buf, size_t len, int timeout) = 0;

    /* incoming message buffer is in (*buf, *len); receiver owns it */
    virtual int recv(imqd_t qd, void **buf, size_t *len, int timeout) = 0;

    /* use this to free copied buffers */
    virtual void free(void *buf) = 0;
};

#if defined (DFMODULE_BUILD)
extern "C" DFM_EXPORT imqueue * DFM_APIEP getmqueue(void);
#else // using glue and runtime loading.
extern getmqueue_t getmqueue;
#endif

#endif
