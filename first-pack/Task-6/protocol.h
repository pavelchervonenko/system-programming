#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>

#define REQ_QUEUE_NAME "/filegrouper_req"
#define MSG_TEXT_SIZE 960

enum MsgType
{
    MSG_HELLO = 1,
    MSG_PATH = 2,
    MSG_DONE = 3,
    MSG_RESULT = 4,
    MSG_END = 5,
    MSG_ERROR = 6
};

struct Msg
{
    int type;
    char text[MSG_TEXT_SIZE];
};

#endif
