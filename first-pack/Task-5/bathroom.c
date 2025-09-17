#include "bathroom.h"
#include <stdio.h>

int bathroom_init(Bathroom *b, int capacity)
{
    if (!b || capacity <= 0)
    {
        return -1;
    }

    b->capacity = capacity;
    b->women_inside = 0;
    b->men_inside = 0;
    b->waiting_women = 0;
    b->waiting_men = 0;
    b->current = NONE;

    int rc = pthread_mutex_init(&b->lock, NULL);
    if (rc != 0)
    {
        return rc;
    }

    rc = pthread_cond_init(&b->cv_women, NULL);
    if (rc != 0)
    {
        pthread_mutex_destroy(&b->lock);
        return rc;
    }

    rc = pthread_cond_init(&b->cv_men, NULL);
    if (rc != 0)
    {
        pthread_cond_destroy(&b->cv_women);
        pthread_mutex_destroy(&b->lock);
        return rc;
    }

    return 0;
}

int bathroom_destroy(Bathroom *b)
{
    if (!b)
    {
        return -1;
    }

    if (b->women_inside > 0 || b->men_inside > 0)
    {
        fprintf(stderr, "destroy bathrom with people inside\n");
    }

    int rc1 = pthread_cond_destroy(&b->cv_women);
    int rc2 = pthread_cond_destroy(&b->cv_men);
    int rc3 = pthread_mutex_destroy(&b->lock);

    if (rc1)
    {
        return rc1;
    }
    if (rc2)
    {
        return rc2;
    }
    if (rc3)
    {
        return rc3;
    }

    return 0;
}

void woman_wants_to_enter(Bathroom *b)
{
    if (!b)
    {
        return;
    }

    int rc = pthread_mutex_lock(&b->lock);
    if (rc != 0)
    {
        fprintf(stderr, "pthread_mutex_lock failed\n");
        return;
    }

    b->waiting_women++;
    while (b->men_inside > 0 || b->women_inside >= b->capacity)
    {
        rc = pthread_cond_wait(&b->cv_women, &b->lock);
        if (rc != 0)
        {
            fprintf(stderr, "pthread_mutex_wait(cv_women) failed\n");
            b->waiting_women--;
            pthread_mutex_unlock(&b->lock);
            return;
        }
    }

    b->waiting_women--;
    b->women_inside++;
    b->current = WOMAN;

    pthread_mutex_unlock(&b->lock);
}

void man_wants_to_enter(Bathroom *b)
{
    if (!b)
    {
        return;
    }

    int rc = pthread_mutex_lock(&b->lock);
    if (rc != 0)
    {
        fprintf(stderr, "pthread_mutex_lock failed\n");
        return;
    }

    b->waiting_men++;
    while (b->women_inside > 0 || b->men_inside >= b->capacity)
    {
        rc = pthread_cond_wait(&b->cv_men, &b->lock);
        if (rc != 0)
        {
            fprintf(stderr, "pthread_mutex_wait(cv_men) failed\n");
            b->waiting_men--;
            pthread_mutex_unlock(&b->lock);
            return;
        }
    }

    b->waiting_men--;
    b->men_inside++;
    b->current = MAN;

    pthread_mutex_unlock(&b->lock);
}

void woman_leaves(Bathroom *b)
{
    if (!b)
    {
        return;
    }

    int rc = pthread_mutex_lock(&b->lock);
    if (rc != 0)
    {
        fprintf(stderr, "pthread_mutex_lock failed\n");
        return;
    }

    if (b->women_inside <= 0)
    {
        fprintf(stderr, "no woman inside\n");
        pthread_mutex_unlock(&b->lock);
        return;
    }

    b->women_inside--;
    if (b->women_inside == 0)
    {
        b->current = NONE;
        pthread_cond_broadcast(&b->cv_men);
        pthread_cond_broadcast(&b->cv_women);
    }
    else if (b->women_inside < b->capacity)
    {
        pthread_cond_signal(&b->cv_women);
    }

    pthread_mutex_unlock(&b->lock);
}

void man_leaves(Bathroom *b)
{
    if (!b)
    {
        return;
    }

    int rc = pthread_mutex_lock(&b->lock);
    if (rc != 0)
    {
        fprintf(stderr, "pthread_mutex_lock failed\n");
        return;
    }

    if (b->men_inside <= 0)
    {
        fprintf(stderr, "no mеn inside\n");
        pthread_mutex_unlock(&b->lock);
        return;
    }

    b->men_inside--;
    if (b->men_inside == 0)
    {
        b->current = NONE;
        pthread_cond_broadcast(&b->cv_women);
        pthread_cond_broadcast(&b->cv_men);
    }
    else if (b->men_inside < b->capacity)
    {
        pthread_cond_signal(&b->cv_men);
    }

    pthread_mutex_unlock(&b->lock);
}