#include <stdio.h>
#include <string.h>

#include "puzzle.h"

#define BIT_W (1u << 0)
#define BIT_G (1u << 1)
#define BIT_C (1u << 2)

unsigned int bit_of(Object obj)
{
    switch (obj)
    {
    case OBJ_WOLF:
        return BIT_W;
    case OBJ_GOAT:
        return BIT_G;
    case OBJ_CABBAGE:
        return BIT_C;
    default:
        return 0u;
    }
}

void init(State *st)
{
    if (!st)
    {
        return;
    }
    st->boat_cargo = OBJ_NONE;
    st->boat_side = SIDE_LEFT;
    st->left_mask = BIT_W | BIT_G | BIT_C;
}

bool is_safe(const State *st)
{
    const unsigned ALL = BIT_W | BIT_G | BIT_C;
    unsigned boatbit = bit_of(st->boat_cargo);

    unsigned right = (ALL ^ st->left_mask) & (ALL ^ boatbit);

    if (st->boat_side == SIDE_LEFT)
    {
        bool wg = (right & BIT_W) && (right & BIT_G);
        bool gc = (right & BIT_G) && (right & BIT_C);
        if (wg || gc)
        {
            return false;
        }
    }
    else
    {
        bool wg = (st->left_mask & BIT_W) && (st->left_mask & BIT_G);
        bool gc = (st->left_mask & BIT_G) && (st->left_mask & BIT_C);
        if (wg || gc)
        {
            return false;
        }
    }
    return true;
}

bool is_solved(const State *st)
{
    if (!st)
    {
        return false;
    }
    const unsigned ALL = BIT_W | BIT_G | BIT_C;
    return st->boat_side == SIDE_RIGHT &&
           st->boat_cargo == OBJ_NONE &&
           (st->left_mask & ALL) == 0u;
}

const char *object_name(Object obj)
{
    switch (obj)
    {
    case OBJ_WOLF:
        return "wolf";
    case OBJ_GOAT:
        return "goat";
    case OBJ_CABBAGE:
        return "cabbage";
    case OBJ_NONE:
        return "none";
    }
    return "none";
}

Object parse_object(const char *s)
{
    if (!s)
    {
        return OBJ_NONE;
    }
    if (strcmp(s, "wolf") == 0)
    {
        return OBJ_WOLF;
    }
    if (strcmp(s, "goat") == 0)
    {
        return OBJ_GOAT;
    }
    if (strcmp(s, "cabbage") == 0)
    {
        return OBJ_CABBAGE;
    }
    return OBJ_NONE;
}

Result try_take(State *st, Object obj)
{
    if (obj == OBJ_NONE)
    {
        return ERR_INVALID_CMD;
    }
    if (st->boat_cargo != OBJ_NONE)
    {
        return ERR_BOAT_OCCUPIED;
    }

    unsigned int mask = bit_of(obj);
    if (mask == 0u)
    {
        return ERR_OBJECT_ABSENT;
    }

    bool on_left = (st->left_mask & mask) != 0;

    if ((st->boat_side == SIDE_LEFT && !on_left) ||
        (st->boat_side == SIDE_RIGHT && on_left))
    {
        return ERR_OBJECT_NOT_HERE;
    }

    State tmp = *st;
    if (tmp.boat_side == SIDE_LEFT)
    {
        tmp.left_mask = tmp.left_mask & ~mask;
    }

    tmp.boat_cargo = obj;

    if (!is_safe(&tmp))
    {
        return ERR_UNSAFE_STATE;
    }

    *st = tmp;
    return OK;
}

Result try_put(State *st)
{
    if (st->boat_cargo == OBJ_NONE)
    {
        return ERR_BOAT_EMPTY;
    }

    State tmp = *st;

    unsigned mask = bit_of(tmp.boat_cargo);

    if (tmp.boat_side == SIDE_LEFT)
    {
        tmp.left_mask = tmp.left_mask | mask;
    }
    tmp.boat_cargo = OBJ_NONE;

    if (!is_safe(&tmp))
    {
        return ERR_UNSAFE_STATE;
    }

    *st = tmp;
    return OK;
}

Result try_move(State *st)
{
    State tmp = *st;
    if (tmp.boat_side == SIDE_LEFT)
    {
        tmp.boat_side = SIDE_RIGHT;
    }
    else
    {
        tmp.boat_side = SIDE_LEFT;
    }

    if (!is_safe(&tmp))
    {
        return ERR_UNSAFE_STATE;
    }

    *st = tmp;
    return OK;
}

Result apply(State *st, const char *cmd, const char *arg)
{
    if (!st || !cmd)
    {
        return ERR_INVALID_CMD;
    }

    if (strcmp(cmd, "take") == 0)
    {
        Object obj = parse_object(arg);
        return try_take(st, obj);
    }
    if (strcmp(cmd, "put") == 0)
    {
        return try_put(st);
    }
    if (strcmp(cmd, "move") == 0)
    {
        return try_move(st);
    }
    return ERR_INVALID_CMD;
}

size_t format_state(const State *st, char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0)
        return 0;

    const char *boat;
    boat = "Right";
    if (st->boat_side == SIDE_LEFT)
    {
        boat = "Left";
    }

    char left[64] = {0};
    char right[64] = {0};
    size_t li = 0;
    size_t ri = 0;

    if (st->left_mask & BIT_W)
    {
        li += snprintf(left + li, sizeof(left) - li, "wolf ");
    }
    if (st->left_mask & BIT_G)
    {
        li += snprintf(left + li, sizeof(left) - li, "goat ");
    }
    if (st->left_mask & BIT_C)
    {
        li += snprintf(left + li, sizeof(left) - li, "cabbage ");
    }

    const unsigned ALL = BIT_W | BIT_G | BIT_C;
    unsigned boatbit = bit_of(st->boat_cargo);
    unsigned rightmask = (ALL ^ st->left_mask) & (ALL ^ boatbit);

    if (rightmask & BIT_W)
    {
        ri += snprintf(right + ri, sizeof(right) - ri, "wolf ");
    }
    if (rightmask & BIT_G)
    {
        ri += snprintf(right + ri, sizeof(right) - ri, "goat ");
    }
    if (rightmask & BIT_C)
    {
        ri += snprintf(right + ri, sizeof(right) - ri, "cabbage ");
    }

    const char *cargo = object_name(st->boat_cargo);

    return snprintf(buf, buf_size,
                    "Boat:%s; Left:{%s}; Right:{%s}; BoatCargo:{%s}",
                    boat, left, right, cargo);
}