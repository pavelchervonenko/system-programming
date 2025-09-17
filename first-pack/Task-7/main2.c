#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <grp.h>
#include <pwd.h>
#include <stdbool.h>

typedef struct
{
    bool show_all;
    bool long_format;
} ls_options;

void print_long_format(char *dir_name, char *file_name)
{
    struct stat file_stat;
    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s/%s", dir_name, file_name);

    if (lstat(full_path, &file_stat) < 0)
    {
        perror("lstat");
        return;
    }

    printf((S_ISDIR(file_stat.st_mode)) ? "d" : "-");
    printf((file_stat.st_mode & S_IRUSR) ? "r" : "-");
    printf((file_stat.st_mode & S_IWUSR) ? "w" : "-");
    printf((file_stat.st_mode & S_IXUSR) ? "x" : "-");
    printf((file_stat.st_mode & S_IRGRP) ? "r" : "-");
    printf((file_stat.st_mode & S_IWGRP) ? "w" : "-");
    printf((file_stat.st_mode & S_IXGRP) ? "x" : "-");
    printf((file_stat.st_mode & S_IROTH) ? "r" : "-");
    printf((file_stat.st_mode & S_IWOTH) ? "w" : "-");
    printf((file_stat.st_mode & S_IXOTH) ? "x" : "-");

    printf(" %ld", (long)file_stat.st_nlink);

    struct passwd *pwd = getpwuid(file_stat.st_uid);
    struct group *grp = getgrgid(file_stat.st_gid);
    printf(" %s", pwd ? pwd->pw_name : "?");
    printf(" %s", grp ? grp->gr_name : "?");

    printf(" %8ld", (long)file_stat.st_size);

    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%b %d %H:%M", localtime(&file_stat.st_mtime));
    printf(" %s", time_buf);

    printf(" %s", file_name);

    // printf(" %lu", (unsigned long)file_stat.st_ino);

    printf("\n");
}

void list_directory(char *name, ls_options options)
{
    DIR *dir;
    struct dirent *entry = NULL;

    if (!(dir = opendir(name)))
    {
        printf("ERROR: opendir\n");
        return;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (!options.show_all && entry->d_name[0] == '.')
        {
            continue;
        }

        if (options.long_format)
        {
            print_long_format(name, entry->d_name);
        }
        else
        {
            printf("%s\n", entry->d_name);
        }
    }
    closedir(dir);
}

void parse_options(int argc, char *argv[], ls_options *options, int *first_dir_idx)
{
    options->show_all = false;
    options->long_format = false;
    *first_dir_idx = 1;

    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] == '-')
        {
            for (int j = 1; argv[i][j] != '\0'; j++)
            {
                switch (argv[i][j])
                {
                case 'a':
                    options->show_all = true;
                    break;
                case 'l':
                    options->long_format = true;
                    break;
                default:
                    printf("Unknown option\n");
                    return;
                }
            }
            (*first_dir_idx)++;
        }
        else
        {
            break;
        }
    }

    if (*first_dir_idx >= argc)
    {
        *first_dir_idx = argc - 1;
        argv[argc] = ".";
        argv[argc + 1] = NULL;
    }
}

int main(int argc, char *argv[])
{
    ls_options options;
    int first_dir_idx;

    parse_options(argc, argv, &options, &first_dir_idx);

    if (first_dir_idx >= argc)
    {
        printf("ERROR: no directories specified\n");
        return -1;
    }

    int dir_count = argc - first_dir_idx;

    for (int i = first_dir_idx; i < argc; ++i)
    {
        list_directory(argv[i], options);
    }

    return 0;
}
