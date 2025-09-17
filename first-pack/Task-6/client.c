#define _POSIX_C_SOURCE 200809L
#include <mqueue.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "protocol.h"

static int open_req_queue(mqd_t *out)
{
    mqd_t q = mq_open(REQ_QUEUE_NAME, O_WRONLY);
    if (q == (mqd_t)-1)
        return -1;
    *out = q;
    return 0;
}

static int make_reply_name(char *buf, size_t n)
{
    pid_t pid = getpid();
    int r = snprintf(buf, n, "/filegrouper_rsp_%ld", (long)pid);
    if (r <= 0)
        return -1;
    if ((size_t)r >= n)
        return -1;
    return 0;
}

static int create_reply_queue(const char *name, mqd_t *out)
{
    struct mq_attr attr;
    mqd_t q;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(struct Msg);
    attr.mq_curmsgs = 0;
    q = mq_open(name, O_CREAT | O_RDONLY, 0600, &attr);
    if (q == (mqd_t)-1)
        return -1;
    *out = q;
    return 0;
}

static int send_text(mqd_t q, int type, const char *text)
{
    struct Msg msg;
    size_t n;
    if (text == NULL)
        return -1;
    n = strlen(text);
    if (n >= MSG_TEXT_SIZE)
        return -1;
    msg.type = type;
    memcpy(msg.text, text, n + 1);
    if (mq_send(q, (const char *)&msg, sizeof(msg), 0) != 0)
        return -1;
    return 0;
}

static int validate_abs_path(const char *p)
{
    if (p == NULL)
        return 0;
    if (p[0] != '/')
        return 0;
    if (strlen(p) < 2)
        return 0;
    return 1;
}

int main(int argc, char **argv)
{
    mqd_t req_q;
    mqd_t rsp_q;
    char reply_name[MSG_TEXT_SIZE];
    FILE *in = NULL;
    int rc = 0;

    if (argc != 2)
    {
        fprintf(stderr, "invalid args");
        return 2;
    }

    in = fopen(argv[1], "r");
    if (in == NULL)
    {
        fprintf(stderr, "cannot open input file\n");
        return 1;
    }

    if (open_req_queue(&req_q) != 0)
    {
        fprintf(stderr, "cannot open request queue\n");
        fclose(in);
        return 1;
    }

    if (make_reply_name(reply_name, sizeof reply_name) != 0)
    {
        fprintf(stderr, "cannot build reply name\n");
        mq_close(req_q);
        fclose(in);
        return 1;
    }

    if (create_reply_queue(reply_name, &rsp_q) != 0)
    {
        fprintf(stderr, "cannot create reply queue\n");
        mq_close(req_q);
        fclose(in);
        return 1;
    }

    if (send_text(req_q, MSG_HELLO, reply_name) != 0)
    {
        fprintf(stderr, "send hello failed\n");
        mq_close(req_q);
        mq_close(rsp_q);
        mq_unlink(reply_name);
        fclose(in);
        return 1;
    }

    for (;;)
    {
        char *line = NULL;
        size_t cap = 0;
        ssize_t r = getline(&line, &cap, in);
        if (r < 0)
        {
            free(line);
            break;
        }
        while (r > 0 && (line[r - 1] == '\n' || line[r - 1] == '\r'))
        {
            line[r - 1] = '\0';
            r--;
        }
        if (line[0] == '\0')
        {
            free(line);
            continue;
        }
        if (!validate_abs_path(line))
        {
            fprintf(stderr, "skip invalid path: %s\n", line);
            free(line);
            continue;
        }
        if (strlen(line) >= MSG_TEXT_SIZE)
        {
            fprintf(stderr, "skip too long: %s\n", line);
            free(line);
            continue;
        }
        if (send_text(req_q, MSG_PATH, line) != 0)
        {
            fprintf(stderr, "send path failed\n");
            free(line);
            rc = 1;
            break;
        }
        free(line);
    }

    if (send_text(req_q, MSG_DONE, "") != 0)
    {
        fprintf(stderr, "send done failed\n");
        rc = 1;
    }

    if (rc == 0)
    {
        struct Msg msg;
        for (;;)
        {
            ssize_t n = mq_receive(rsp_q, (char *)&msg, sizeof(msg), NULL);
            if (n < 0)
            {
                fprintf(stderr, "receive failed\n");
                rc = 1;
                break;
            }
            if ((size_t)n != sizeof(msg))
            {
                continue;
            }
            if (msg.type == MSG_RESULT)
            {
                fputs(msg.text, stdout);
                fflush(stdout);
            }
            else if (msg.type == MSG_END)
            {
                break;
            }
            else if (msg.type == MSG_ERROR)
            {
                fprintf(stderr, "%s\n", msg.text);
            }
        }
    }

    fclose(in);
    mq_close(req_q);
    mq_close(rsp_q);
    mq_unlink(reply_name);
    return rc;
}
