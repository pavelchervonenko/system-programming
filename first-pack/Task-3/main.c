#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include <pthread.h>
#include <semaphore.h>

typedef enum
{
    MODE_NAIVE = 0,
    MODE_WAITER = 1
} Mode;

typedef enum
{
    OK = 0,
    E_ARGS = 2,
    E_MEM = 3,
    E_SYS = 4
} Err;

typedef struct
{
    int n;
    int meals;
    Mode mode;
    int id;
    sem_t *forks;
    sem_t *waiter;
} PhilosopherContext;

typedef struct
{
    int n;
    int meals;
    Mode mode;
} Config;

static void msleep(int ms)
{
    if (ms <= 0)
    {
        return;
    }
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

int parse_int(const char *s, int *out)
{
    if (!s || !*s)
    {
        return 0;
    }
    char *end = NULL;
    long value = strtol(s, &end, 10);

    if (*end != '\0')
    {
        return 0;
    }
    if (value < INT_MIN || value > INT_MAX)
    {
        return 0;
    }
    *out = (int)value;

    return 1;
}

int parse_mode(const char *s, Mode *out)
{
    if (!s)
    {
        return 0;
    }
    if (strcmp(s, "naive") == 0)
    {
        *out = MODE_NAIVE;
        return 1;
    }
    if (strcmp(s, "waiter") == 0)
    {
        *out = MODE_WAITER;
        return 1;
    }
    return 0;
}

void *philosopher_thread(void *arg)
{
    PhilosopherContext *ctx = (PhilosopherContext *)arg;
    const int id = ctx->id;
    const int n = ctx->n;
    const int left = id;
    const int right = (id + 1) % n;

    for (int meal = 0; meal < ctx->meals; ++meal)
    {
        msleep(10 + (id * 3 % 17));

        if (ctx->mode == MODE_WAITER && ctx->waiter)
        {
            if (sem_wait(ctx->waiter) != 0)
            {
                fprintf(stderr, "sem_wait(waiter) failed\n");
                return NULL;
            }
        }

        if (sem_wait(&ctx->forks[left]) != 0)
        {
            fprintf(stderr, "sem_wait(left) failed\n");
            if (ctx->mode == MODE_WAITER && ctx->waiter)
            {
                sem_post(ctx->waiter);
            }
            return NULL;
        }

        msleep(5);

        if (sem_wait(&ctx->forks[right]) != 0)
        {
            fprintf(stderr, "sem_wait(right) failed\n");
            sem_post(&ctx->forks[left]);

            if (ctx->mode == MODE_WAITER && ctx->waiter)
            {
                sem_post(ctx->waiter);
            }
            return NULL;
        }

        msleep(10);

        sem_post(&ctx->forks[right]);
        sem_post(&ctx->forks[left]);

        if (ctx->mode == MODE_WAITER && ctx->waiter)
        {
            sem_post(ctx->waiter);
        }
    }
    return NULL;
}

Err run_simulation(const Config *cfg)
{
    if (cfg->n < 2 || cfg->meals < 1)
    {
        fprintf(stderr, "invalid args\n");
        return E_ARGS;
    }

    Err rc = OK;

    pthread_t *threads = (pthread_t *)calloc((size_t)cfg->n, sizeof(*threads));
    PhilosopherContext *ph = (PhilosopherContext *)calloc((size_t)cfg->n, sizeof(*ph));
    sem_t *forks = (sem_t *)calloc((size_t)cfg->n, sizeof(*forks));
    if (!threads || !ph || !forks)
    {
        fprintf(stderr, "memory allocation failed\n");
        free(threads);
        free(ph);
        free(forks);
        return E_MEM;
    }

    int forks_inited = 0;
    for (int i = 0; i < cfg->n; ++i)
    {
        if (sem_init(&forks[i], 0, 1) != 0)
        {
            fprintf(stderr, "sem_init() failed\n");
            for (int j = 0; j < i; ++j)
            {
                sem_destroy(&forks[j]);
                free(forks);
                free(ph);
                free(forks);
                return E_SYS;
            }
        }
        forks_inited++;
    }

    sem_t waiter;
    sem_t *waiter_ptr = NULL;
    if (cfg->mode == MODE_WAITER)
    {
        if (sem_init(&waiter, 0, (unsigned)(cfg->n - 1)) != 0)
        {
            fprintf(stderr, "sem_init(waiter) failed\n");
            for (int j = 0; j < forks_inited; ++j)
            {
                sem_destroy(&forks[j]);
                free(forks);
                free(ph);
                free(forks);
                return E_SYS;
            }
        }
        waiter_ptr = &waiter;
    }

    int started = 0;
    for (int i = 0; i < cfg->n; ++i)
    {
        ph[i].id = i;
        ph[i].n = cfg->n;
        ph[i].meals = cfg->meals;
        ph[i].forks = forks;
        ph[i].waiter = waiter_ptr;
        ph[i].mode = cfg->mode;

        int e = pthread_create(&threads[i], NULL, philosopher_thread, &ph[i]);
        if (e != 0)
        {
            fprintf(stderr, "pthread_create() failed\n");
            rc = E_SYS;

            for (int j = 0; j < started; ++j)
            {
                pthread_join(threads[j], NULL);
            }
            if (waiter_ptr)
            {
                sem_destroy(waiter_ptr);
            }
            for (int j = 0; j < forks_inited; ++j)
            {
                sem_destroy(&forks[j]);
            }
            free(forks);
            free(ph);
            free(threads);
            return rc;
        }
        started++;
    }

    for (int i = 0; i < cfg->n; ++i)
    {
        pthread_join(threads[i], NULL);
    }

    if (waiter_ptr)
    {
        sem_destroy(waiter_ptr);
    }
    for (int i = 0; i < forks_inited; ++i)
    {
        sem_destroy(&forks[i]);
    }

    free(forks);
    free(ph);
    free(threads);

    return rc;
}

int main(int argc, char *argv[])
{
    Config cfg = {
        .n = 5,
        .meals = 20,
        .mode = MODE_NAIVE};

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--philos") == 0 && i + 1 < argc)
        {
            int value;
            if (!parse_int(argv[i + 1], &value))
            {
                return E_ARGS;
            }
            cfg.n = value;
            i++;
        }
        else if (strcmp(argv[i], "--meals") == 0 && i + 1 < argc)
        {
            int value;
            if (!parse_int(argv[i + 1], &value))
            {
                return E_ARGS;
            }
            cfg.meals = value;
            i++;
        }
        else if (strcmp(argv[i], "--mode") == 0 && i + 1 < argc)
        {
            Mode m;
            if (!parse_mode(argv[i + 1], &m))
            {
                return E_ARGS;
            }
            cfg.mode = m;
            i++;
        }
        else
        {
            return E_ARGS;
        }
    }

    Err rc = run_simulation(&cfg);
    if (rc == 0)
    {
        printf("Nice\n");
    }
    return rc;
}