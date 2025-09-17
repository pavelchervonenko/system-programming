#ifndef COMMON_H
#define COMMON_H

#include <stddef.h>
#include <stdint.h>

#define SERVER_QUEUE "/farmer_cmd"

#define MAX_QNAME_LEN 64
#define MAX_CMD_LEN 16
#define MAX_ARG_LEN 16
#define MAX_STATE_STR 128

typedef enum
{
    OP_TAKE = 1,
    OP_PUT = 2,
    OP_MOVE = 3
} OpCode;

typedef struct
{
    uint32_t user_id;
    char reply_queue[MAX_QNAME_LEN];
    char cmd[MAX_CMD_LEN];
    char arg[MAX_ARG_LEN];
} RequestMsg;

typedef struct
{
    int32_t result_code;
    uint8_t solved;
    char state_str[MAX_STATE_STR];
    char err_text[64];
} ResponseMsg;

#endif
