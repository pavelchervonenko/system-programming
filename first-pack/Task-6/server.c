#define _POSIX_C_SOURCE 200809L
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "protocol.h"
#include "group.h"

struct Session
{
    char reply_name[MSG_TEXT_SIZE];
    struct Group group;
    mqd_t reply_q;
    int has_reply;
};

static int send_text(mqd_t q, int type, const char *text)
{
    struct Msg msg;
    size_t n;
    if (text == NULL)
    {
        return -1;
    }
    n = strlen(text);
    if (n >= MSG_TEXT_SIZE)
    {
        return -1;
    }
    msg.type = type;
    memcpy(msg.text, text, n + 1);
    if (mq_send(q, (const char *)&msg, sizeof(msg), 0) != 0)
    {
        return -1;
    }
    return 0;
}

static int send_chunks(mqd_t q, const char *data)
{
    size_t pos = 0;
    size_t n = strlen(data);
    while (pos < n)
    {
        size_t chunk = MSG_TEXT_SIZE - 1;
        size_t rem = n - pos;
        if (rem < chunk)
            chunk = rem;
        {
            struct Msg msg;
            msg.type = MSG_RESULT;
            memcpy(msg.text, data + pos, chunk);
            msg.text[chunk] = '\0';
            if (mq_send(q, (const char *)&msg, sizeof(msg), 0) != 0)
            {
                return -1;
            }
        }
        pos += chunk;
    }
    {
        struct Msg endmsg;
        endmsg.type = MSG_END;
        endmsg.text[0] = '\0';
        if (mq_send(q, (const char *)&endmsg, sizeof(endmsg), 0) != 0)
        {
            return -1;
        }
    }
    return 0;
}

static int open_req_queue(mqd_t *out)
{
    struct mq_attr attr;
    mqd_t q;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(struct Msg);
    attr.mq_curmsgs = 0;
    q = mq_open(REQ_QUEUE_NAME, O_CREAT | O_RDONLY, 0660, &attr);
    if (q == (mqd_t)-1)
    {
        return -1;
    }
    *out = q;
    return 0;
}

static int open_reply_queue(const char *name, mqd_t *out)
{
    mqd_t q;
    q = mq_open(name, O_WRONLY);
    if (q == (mqd_t)-1)
    {
        return -1;
    }
    *out = q;
    return 0;
}

static int handle_hello(struct Session *s, const struct Msg *msg)
{
    size_t n;
    if (s->has_reply)
    {
        return -1;
    }
    n = strlen(msg->text);
    if (n == 0)
    {
        return -1;
    }
    if (n >= sizeof(s->reply_name))
    {
        return -1;
    }
    memcpy(s->reply_name, msg->text, n + 1);
    if (open_reply_queue(s->reply_name, &s->reply_q) != 0)
    {
        return -1;
    }
    s->has_reply = 1;
    if (group_init(&s->group) != 0)
    {
        return -1;
    }
    return 0;
}

static int validate_abs_path(const char *p)
{
    if (p == NULL)
        return 0;
    if (p[0] != '/')
        return 0;
    if (strstr(p, " ") != NULL)
        return 0;
    if (strlen(p) < 2)
        return 0;
    return 1;
}

static int handle_path(struct Session *s, const struct Msg *msg)
{
    if (!s->has_reply)
        return -1;
    if (!validate_abs_path(msg->text))
        return -1;
    if (group_add_path(&s->group, msg->text) != 0)
        return -1;
    return 0;
}

static int handle_done(struct Session *s)
{
    char *out;
    int rc;
    if (!s->has_reply)
        return -1;
    out = group_format(&s->group);
    if (out == NULL)
        return -1;
    rc = send_chunks(s->reply_q, out);
    free(out);
    return rc;
}

int main(void)
{
    mqd_t req_q;
    struct mq_attr attr;
    struct Session session;
    struct Msg msg;
    ssize_t rcvd;

    if (open_req_queue(&req_q) != 0)
    {
        fprintf(stderr, "cannot open request queue\n");
        return 1;
    }
    if (mq_getattr(req_q, &attr) != 0)
    {
        fprintf(stderr, "cannot get queue attrs\n");
        mq_close(req_q);
        return 1;
    }

    memset(&session, 0, sizeof session);

    while (1)
    {
        rcvd = mq_receive(req_q, (char *)&msg, sizeof(msg), NULL);
        if (rcvd < 0)
        {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "receive failed\n");
            break;
        }
        if ((size_t)rcvd != sizeof(msg))
        {
            continue;
        }
        if (msg.type == MSG_HELLO)
        {
            if (session.has_reply)
            {
                continue;
            }
            if (handle_hello(&session, &msg) != 0)
            {
                continue;
            }
        }
        else if (msg.type == MSG_PATH)
        {
            if (handle_path(&session, &msg) != 0)
            {
                if (session.has_reply)
                {
                    send_text(session.reply_q, MSG_ERROR, "bad path");
                }
            }
        }
        else if (msg.type == MSG_DONE)
        {
            if (handle_done(&session) != 0)
            {
                if (session.has_reply)
                {
                    send_text(session.reply_q, MSG_ERROR, "internal error");
                }
            }
            if (session.has_reply)
            {
                mq_close(session.reply_q);
            }
            group_free(&session.group);
            memset(&session, 0, sizeof session);
        }
        else
        {
            if (session.has_reply)
            {
                send_text(session.reply_q, MSG_ERROR, "unknown message");
            }
        }
    }

    mq_close(req_q);
    mq_unlink(REQ_QUEUE_NAME);
    return 0;
}
