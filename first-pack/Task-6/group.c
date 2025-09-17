#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "group.h"

int str_dup(const char *s, char **out)
{
    size_t n;
    char *p;
    if (s == NULL)
        return -1;
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (p == NULL)
        return -1;
    memcpy(p, s, n + 1);
    *out = p;
    return 0;
}

int flist_push(struct FileList *fl, const char *name)
{
    char *copy;
    if (str_dup(name, &copy) != 0)
        return -1;
    if (fl->count == fl->cap)
    {
        size_t ncap;
        if (fl->cap == 0)
        {
            ncap = 4;
        }
        else
        {
            ncap = fl->cap * 2;
        }

        char **nitems = (char **)realloc(fl->items, ncap * sizeof(char *));
        if (nitems == NULL)
        {
            free(copy);
            return -1;
        }
        fl->items = nitems;
        fl->cap = ncap;
    }
    fl->items[fl->count++] = copy;
    return 0;
}

void flist_free(struct FileList *fl)
{
    size_t i = 0;
    while (i < fl->count)
    {
        free(fl->items[i]);
        i++;
    }
    free(fl->items);
    fl->items = NULL;
    fl->count = 0;
    fl->cap = 0;
}

int group_init(struct Group *g)
{
    if (g == NULL)
        return -1;
    g->items = NULL;
    g->count = 0;
    g->cap = 0;
    return 0;
}

void group_free(struct Group *g)
{
    size_t i = 0;
    if (g == NULL)
        return;
    while (i < g->count)
    {
        free(g->items[i].dir);
        flist_free(&g->items[i].files);
        i++;
    }
    free(g->items);
    g->items = NULL;
    g->count = 0;
    g->cap = 0;
}

int split_dir_file(const char *abs_path, char **dir_out, char **file_out)
{
    const char *last = NULL;
    size_t dirlen;
    size_t flen;
    char *d;
    char *f;

    if (abs_path == NULL)
        return -1;
    if (abs_path[0] != '/')
        return -1;

    last = strrchr(abs_path, '/');
    if (last == NULL)
        return -1;
    if (last == abs_path)
        return -1;

    dirlen = (size_t)(last - abs_path);
    flen = strlen(last + 1);
    if (flen == 0)
        return -1;

    d = (char *)malloc(dirlen + 1);
    f = (char *)malloc(flen + 1);
    if (d == NULL || f == NULL)
    {
        free(d);
        free(f);
        return -1;
    }

    memcpy(d, abs_path, dirlen);
    d[dirlen] = '\0';
    memcpy(f, last + 1, flen + 1);

    *dir_out = d;
    *file_out = f;
    return 0;
}

ssize_t find_dir(const struct Group *g, const char *dir)
{
    size_t i = 0;
    while (i < g->count)
    {
        if (strcmp(g->items[i].dir, dir) == 0)
            return (ssize_t)i;
        i++;
    }
    return -1;
}

int push_dir(struct Group *g, const char *dir, size_t *index_out)
{
    if (g->count == g->cap)
    {
        size_t ncap = g->cap == 0 ? 4 : g->cap * 2;
        struct DirEntry *nitems = (struct DirEntry *)realloc(g->items, ncap * sizeof(struct DirEntry));
        if (nitems == NULL)
            return -1;
        g->items = nitems;
        g->cap = ncap;
    }
    {
        struct DirEntry *e = &g->items[g->count];
        if (str_dup(dir, &e->dir) != 0)
            return -1;
        e->files.items = NULL;
        e->files.count = 0;
        e->files.cap = 0;
        *index_out = g->count;
        g->count++;
        return 0;
    }
}

int group_add_path(struct Group *g, const char *abs_path)
{
    char *dir = NULL;
    char *file = NULL;
    ssize_t idx;
    size_t pos;
    if (split_dir_file(abs_path, &dir, &file) != 0)
        return -1;
    idx = find_dir(g, dir);
    if (idx < 0)
    {
        if (push_dir(g, dir, &pos) != 0)
        {
            free(dir);
            free(file);
            return -1;
        }
    }
    else
    {
        pos = (size_t)idx;
    }
    if (flist_push(&g->items[pos].files, file) != 0)
    {
        free(dir);
        free(file);
        return -1;
    }
    free(dir);
    free(file);
    return 0;
}

int append_str(char **buf, size_t *len, size_t *cap, const char *s)
{
    size_t slen = strlen(s);
    if (*len + slen + 1 > *cap)
    {
        size_t ncap = *cap == 0 ? 256 : *cap;
        while (*len + slen + 1 > ncap)
        {
            if (ncap > (size_t)-1 / 2)
                return -1;
            ncap *= 2;
        }
        {
            char *nbuf = (char *)realloc(*buf, ncap);
            if (nbuf == NULL)
                return -1;
            *buf = nbuf;
            *cap = ncap;
        }
    }
    memcpy(*buf + *len, s, slen);
    *len += slen;
    (*buf)[*len] = '\0';
    return 0;
}

char *group_format(const struct Group *g)
{
    char *buf = NULL;
    size_t len = 0;
    size_t cap = 0;
    size_t i = 0;
    while (i < g->count)
    {
        if (append_str(&buf, &len, &cap, g->items[i].dir) != 0)
        {
            free(buf);
            return NULL;
        }
        if (append_str(&buf, &len, &cap, "\n") != 0)
        {
            free(buf);
            return NULL;
        }
        {
            size_t j = 0;
            while (j < g->items[i].files.count)
            {
                if (append_str(&buf, &len, &cap, "  ") != 0)
                {
                    free(buf);
                    return NULL;
                }
                if (append_str(&buf, &len, &cap, g->items[i].files.items[j]) != 0)
                {
                    free(buf);
                    return NULL;
                }
                if (append_str(&buf, &len, &cap, "\n") != 0)
                {
                    free(buf);
                    return NULL;
                }
                j++;
            }
        }
        i++;
    }
    if (buf == NULL)
    {
        buf = (char *)malloc(1);
        if (buf == NULL)
        {
            return NULL;
        }
        buf[0] = '\0';
    }
    return buf;
}
