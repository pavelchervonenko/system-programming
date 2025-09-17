#ifndef PUZZLE_H
#define PUZZLE_H

#include <stddef.h>
#include <stdbool.h>

typedef enum
{
    OBJ_WOLF = 0,
    OBJ_GOAT = 1,
    OBJ_CABBAGE = 2,
    OBJ_NONE = 3
} Object;

typedef enum
{
    SIDE_LEFT = 0,
    SIDE_RIGHT = 1
} Side;

typedef enum
{
    OK = 0,
    ERR_INVALID_CMD,
    ERR_BOAT_OCCUPIED,
    ERR_BOAT_EMPTY,
    ERR_OBJECT_NOT_HERE,
    ERR_OBJECT_IN_BOAT,
    ERR_OBJECT_ABSENT,
    ERR_UNSAFE_STATE
} Result;

typedef struct
{
    Side boat_side;
    unsigned left_mask;
    Object boat_cargo;
} State;

void init(State *st);

Object parse_object(const char *s);

Result apply(State *st, const char *cmd, const char *arg);

bool is_solved(const State *st);

const char *object_name(Object obj);

size_t format_state(const State *st, char *buf, size_t buf_size);

#endif