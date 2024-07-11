#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

#include "dlist.h"
#include "utils.h"

int string_match(const char *pattern, const char *candidate)
{
    if (*pattern == '\0')
    {
        return *candidate == '\0';
    }
    else if (*pattern == '*')
    {
        for (const char *c = candidate; *c != '\0'; c++)
        {
            if (string_match(pattern + 1, c))
                return 1;
        }
        return string_match(pattern + 1, candidate);
    }
    else if (*pattern != '?' && *pattern != *candidate)
    {
        return 0;
    }
    else
    {
        return string_match(pattern + 1, candidate + 1);
    }
}

int file_tree_foreach(const char *dirpath, void (*doit)(const char *, void *),
                      void *context)
{
    DIR *dir;
    dir = opendir(dirpath);
    if (dir == NULL)
    {
        fprintf(stderr, "opendir(%s): %s\n", dirpath, strerror(errno));
        return -1;
    }
    else
    {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            struct stat statbuf;
            char filepath[strlen(dirpath) + 1 + strlen(entry->d_name) + 1];
            strcpy(filepath, dirpath);
            strcat(filepath, "/");
            strcat(filepath, entry->d_name);

            int result = lstat(filepath, &statbuf);
            if (result == -1)
            {
                fprintf(stderr, "stat(%s,...): %s\n", filepath, strerror(errno));
                continue;
            }

            if (S_ISDIR(statbuf.st_mode) &&
                strcmp(entry->d_name, ".") != 0 &&
                strcmp(entry->d_name, "..") != 0)
            {
                file_tree_foreach(filepath, doit, context);
            }

            doit(filepath, context);
        }
        closedir(dir);
    }
    return 0;
}