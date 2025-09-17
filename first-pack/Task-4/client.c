#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <mqueue.h>
#include <unistd.h>

#include "common.h"

int parse_line(const char *line, char *cmd, char *arg)
{
    while (*line == ' ' || *line == '\t')
    {
        ++line;
    }
    if (*line == '\0' || *line == '\n' || *line == '#')
    {
        return 1;
    }

    int off = 0;
    if (sscanf(line, "%15s%n", cmd, &off) != 1)
    {
        return -1;
    }
    line += off;

    if (strcmp(cmd, "take") != 0 && strcmp(cmd, "put") != 0 && strcmp(cmd, "move") != 0)
    {
        return -2;
    }

    arg[0] = '\0';
    if (strcmp(cmd, "take") == 0)
    {
        while (*line == ' ' || *line == '\t')
        {
            ++line;
        }
        if (sscanf(line, "%15s", arg) != 1)
        {
            return -3;
        }
        if (strcmp(arg, "wolf") != 0 && strcmp(arg, "goat") != 0 && strcmp(arg, "cabbage") != 0)
        {
            return -4;
        }
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        fprintf(stderr, "Invalid args\n");
        return 1;
    }

    uint32_t user_id = (uint32_t)strtoul(argv[1], NULL, 10);
    const char *path = argv[2];

    char reply_name[MAX_QNAME_LEN];
    snprintf(reply_name, sizeof(reply_name), "/farmer_rsp_%u_%ld", user_id, (long)getpid());

    struct mq_attr attr;
    memset(&attr, 0, sizeof attr);
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(ResponseMsg);

    mqd_t qout = mq_open(reply_name, O_CREAT | O_RDONLY, 0644, &attr);
    if (qout == (mqd_t)-1)
    {
        fprintf(stderr, "client: cannot create reply queue %s\n", reply_name);
        return 1;
    }

    mqd_t qin = mq_open(SERVER_QUEUE, O_WRONLY);
    if (qin == (mqd_t)-1)
    {
        fprintf(stderr, "client: cannot open server queue %s\n", SERVER_QUEUE);
        mq_close(qout);
        mq_unlink(reply_name);
        return 1;
    }

    FILE *file = fopen(path, "r");
    if (!file)
    {
        fprintf(stderr, "client: cannot open file\n");
        mq_close(qin);
        mq_close(qout);
        mq_unlink(reply_name);
        return 1;
    }

    char line[128];
    char cmd[MAX_CMD_LEN];
    char arg[MAX_ARG_LEN];

    RequestMsg req;
    memset(&req, 0, sizeof req);
    req.user_id = user_id;

    strncpy(req.reply_queue, reply_name, sizeof(req.reply_queue) - 1);

    while (fgets(line, sizeof(line), file))
    {
        int pr = parse_line(line, cmd, arg);
        if (pr == 1)
        {
            continue;
        }
        if (pr != 0)
        {
            fprintf(stderr, "client: skip invalid line");
            continue;
        }

        memset(req.cmd, 0, sizeof(req.cmd));
        memset(req.arg, 0, sizeof(req.arg));

        strncpy(req.cmd, cmd, sizeof(req.cmd) - 1);

        if (arg[0])
            strncpy(req.arg, arg, sizeof(req.arg) - 1);

        if (mq_send(qin, (const char *)&req, sizeof(req), 0) != 0)
        {
            fprintf(stderr, "client: mq_send failed\n");
            break;
        }

        ResponseMsg resp;
        ssize_t r = mq_receive(qout, (char *)&resp, sizeof(resp), NULL);
        if (r < 0)
        {
            fprintf(stderr, "client: mq_receive failed\n");
            break;
        }

        const char *arg_to_print;
        if (req.arg[0] != '\0')
        {
            arg_to_print = req.arg;
        }
        else
        {
            arg_to_print = "";
        }

        printf("[%s %s] -> result=%d, solved=%u\n  %s\n",
               req.cmd, arg_to_print,
               resp.result_code, resp.solved, resp.state_str);

        if (resp.result_code != 0)
        {
            printf("  note: %s\n", resp.err_text);
        }
    }

    fclose(file);
    mq_close(qin);
    mq_close(qout);
    mq_unlink(reply_name);

    return 0;
}
