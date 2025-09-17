#ifndef GROUP_H
#define GROUP_H

#include <stddef.h>

struct FileList
{
    char **items;
    size_t count;
    size_t cap;
};

struct DirEntry
{
    char *dir;
    struct FileList files;
};

struct Group
{
    struct DirEntry *items;
    size_t count;
    size_t cap;
};

int group_init(struct Group *g);
void group_free(struct Group *g);
int group_add_path(struct Group *g, const char *abs_path);
char *group_format(const struct Group *g);

#endif
