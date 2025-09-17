#ifndef BATHROOM_H
#define BATHROOM_H

#include <pthread.h>
#include <stdbool.h>

typedef enum
{
    NONE = 0,
    WOMAN = 1,
    MAN = 2
} Gender;

typedef struct
{
    int capacity;
    int women_inside;
    int men_inside;
    int waiting_women;
    int waiting_men;
    Gender current;

    pthread_mutex_t lock;
    pthread_cond_t cv_women;
    pthread_cond_t cv_men;
} Bathroom;

int bathroom_init(Bathroom *b, int capacity);
int bathroom_destroy(Bathroom *b);

void woman_wants_to_enter(Bathroom *b);
void man_wants_to_enter(Bathroom *b);
void woman_leaves(Bathroom *b);
void man_leaves(Bathroom *b);

#endif