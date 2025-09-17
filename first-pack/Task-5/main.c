#include "bathroom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <pthread.h>

typedef struct
{
    Bathroom *bath;
    Gender gender;
    int id;
    int visits;
    int think_ms_min;
    int think_ms_max;
    int use_ms_min;
    int use_ms_max;
} Person;

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

int parser_int(const char *s, int *out)
{
    const int MIN = 0;
    const int MAX = 100000;
    if (!s || !*s)
    {
        return -1;
    }
    int value = 0;
    const unsigned char *p;
    for (p = (const unsigned char *)s; *p; ++p)
    {
        if (*p < '0' || *p > '9')
        {
            return -1;
        }
        int digit = (int)(*p - '0');
        if (value > (MAX - digit) / 10)
        {
            return -1;
        }
        value = value * 10 + digit;
    }
    if (value < MIN || value > MAX)
    {
        return -1;
    }
    *out = value;
    return 0;
}

void *person_thread(void *arg)
{
    Person *p = (Person *)arg;
    unsigned int r = (unsigned int)(time(NULL) + p->id);

    for (int i = 0; i < p->visits; ++i)
    {
        int tspan = p->think_ms_max - p->think_ms_min + 1;
        int t = 0;
        if (tspan > 0)
        {
            t = p->think_ms_min + (int)(rand_r(&r) % tspan);
        }

        msleep(t);

        if (p->gender == WOMAN)
        {
            woman_wants_to_enter(p->bath);
            printf("[W%d] entered (visit%d)\n", p->id, i + 1);

            int uspan = p->use_ms_max - p->use_ms_min + 1;
            int u = 0;
            if (uspan > 0)
            {
                u = p->use_ms_min + (int)(rand_r(&r) % (unsigned)uspan);
            }
            msleep(u);

            woman_leaves(p->bath);
            printf("[W%d] left (visit%d)\n", p->id, i + 1);
        }
        else
        {
            man_wants_to_enter(p->bath);
            printf("[M%d] entered (visit%d)\n", p->id, i + 1);

            int uspan = p->use_ms_max - p->use_ms_min + 1;
            int u = 0;
            if (uspan > 0)
            {
                u = p->use_ms_min + (int)(rand_r(&r) % (unsigned)uspan);
            }
            msleep(u);

            man_leaves(p->bath);
            printf("[M%d] left (visit%d)\n", p->id, i + 1);
        }
    }
    return NULL;
}

int main(int argc, char *argv[])
{
    int N = 0;
    int W = 3;
    int M = 3;
    int K = 3;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-n") == 0 && i + 1 < argc)
        {
            if (parser_int(argv[i + 1], &N) != 0 || N <= 0)
            {
                fprintf(stderr, "invalid -n\n");
                return 1;
            }
            ++i;
        }
        else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc)
        {
            if (parser_int(argv[i + 1], &W) != 0 || W <= 0)
            {
                fprintf(stderr, "invalid -w\n");
                return 1;
            }
            ++i;
        }
        else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc)
        {
            if (parser_int(argv[i + 1], &M) != 0 || M <= 0)
            {
                fprintf(stderr, "invalid -m\n");
                return 1;
            }
            ++i;
        }
        else if (strcmp(argv[i], "-k") == 0 && i + 1 < argc)
        {
            if (parser_int(argv[i + 1], &K) != 0 || K <= 0)
            {
                fprintf(stderr, "invalid -k\n");
                return 1;
            }
            ++i;
        }
        else
        {
            fprintf(stderr, "invalid args\n");
            return 1;
        }
    }

    Bathroom bath;

    int rc = bathroom_init(&bath, N);
    if (rc != 0)
    {
        fprintf(stderr, "error bathrom_init\n");
        return -1;
    }

    int total = M + W;
    pthread_t *th = (pthread_t *)calloc((size_t)total, sizeof(*th));
    Person *ps = (Person *)calloc((size_t)total, sizeof(*ps));

    if (!th || !ps)
    {
        fprintf(stderr, "error memory allocation\n");
        free(th);
        free(ps);
        bathroom_destroy(&bath);
        return -1;
    }

    int created = 0;
    int exit_status = 0;
    int failed = 0;

    for (int i = 0; i < W && !failed; ++i)
    {
        ps[created] = (Person){
            .bath = &bath,
            .gender = WOMAN,
            .id = i + 1,
            .visits = K,
            .think_ms_min = 10,
            .think_ms_max = 50,
            .use_ms_min = 30,
            .use_ms_max = 120};

        if (created >= total)
        {
            fprintf(stderr, "error created=%d exceeds total=%d\n", created, total);
            return -1;
        }

        rc = pthread_create(&th[created], NULL, person_thread, &ps[created]);
        if (rc != 0)
        {
            fprintf(stderr, "error pthread_create (woman)\n");
            exit_status = 1;
            failed = 1;
        }
        else
        {
            created++;
        }
    }

    for (int i = 0; i < W && !failed; ++i)
    {
        ps[created] = (Person){
            .bath = &bath,
            .gender = MAN,
            .id = i + 1,
            .visits = K,
            .think_ms_min = 10,
            .think_ms_max = 50,
            .use_ms_min = 30,
            .use_ms_max = 120};

        if (created >= total)
        {
            fprintf(stderr, "error created=%d exceeds total=%d\n", created, total);
            return -1;
        }

        rc = pthread_create(&th[created], NULL, person_thread, &ps[created]);
        if (rc != 0)
        {
            fprintf(stderr, "error pthread_create (man)\n");
            exit_status = 1;
            failed = 1;
        }
        else
        {
            created++;
        }
    }

    for (int i = 0; i < created; ++i)
    {
        void *ret = NULL;
        int jrc = pthread_join(th[i], &ret);
        if (jrc != 0)
        {
            fprintf(stderr, "error pthread_join\n");
            exit_status = 1;
        }
    }

    free(th);
    free(ps);

    rc = bathroom_destroy(&bath);
    if (rc != 0)
    {
        fprintf(stderr, "error bathroom_destory\n");
        return 1;
    }

    return exit_status;
}