#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mqueue.h>
#include <signal.h>

#include "common.h"
#include "puzzle.h"

#define SLOTS 64

typedef struct
{
    uint32_t user_id;
    State st;
    int in_use;
} Session;

volatile sig_atomic_t g_stop = 0;

void on_sig(int sig)
{
    (void)sig;
    g_stop = 1;
}

Session *find_or_create_session(Session *tab, uint32_t uid)
{
    for (int i = 0; i < SLOTS; ++i)
    {
        if (tab[i].in_use && tab[i].user_id == uid)
        {
            return &tab[i];
        }
    }
    for (int i = 0; i < SLOTS; ++i)
    {
        if (!tab[i].in_use)
        {
            tab[i].in_use = 1;
            tab[i].user_id = uid;
            init(&tab[i].st);
            return &tab[i];
        }
    }
    return NULL;
}

int main(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_sig;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    struct mq_attr attr = {0};
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(RequestMsg);

    mqd_t qin = mq_open(SERVER_QUEUE, O_CREAT | O_RDONLY, 0644, &attr);
    if (qin == (mqd_t)-1)
    {
        fprintf(stderr, "server: cannot open command queue\n");
        return -1;
    }
    printf("Server started\n");

    Session sessions[SLOTS];
    memset(sessions, 0, sizeof(sessions));

    while (!g_stop)
    {
        RequestMsg req;
        ssize_t r = mq_receive(qin, (char *)&req, sizeof(req), NULL);
        if (r < 0)
        {
            if (g_stop)
            {
                break;
            }
            continue;
        }
        if (r != sizeof(req))
        {
            fprintf(stderr, "error in size req\n");
            return -1;
        }

        ResponseMsg resp;
        memset(&resp, 0, sizeof(resp));

        Session *sess = find_or_create_session(sessions, req.user_id);
        if (!sess)
        {
            resp.result_code = ERR_INVALID_CMD;
            snprintf(resp.err_text, sizeof(resp.err_text), "no free session slots");
        }
        else
        {
            Result rc;
            if (req.arg[0])
            {
                rc = apply(&sess->st, req.cmd, req.arg);
            }
            else
            {
                rc = apply(&sess->st, req.cmd, NULL);
            }

            resp.result_code = rc;
            resp.solved = 0u;
            if (is_solved(&sess->st))
            {
                resp.solved = 1u;
            }

            format_state(&sess->st, resp.state_str, sizeof(resp.state_str));
        }

        mqd_t qout = mq_open(req.reply_queue, O_WRONLY);
        if (qout == (mqd_t)-1)
        {
            fprintf(stderr, "server: cannot open reply queue\n");
            continue;
        }
        if (mq_send(qout, (const char *)&resp, sizeof(resp), 0) != 0)
        {
            fprintf(stderr, "server: mq_send failed\n");
        }
        mq_close(qout);
    }

    mq_close(qin);
    mq_unlink(SERVER_QUEUE);
    puts("Server stopperd");

    return 0;
}