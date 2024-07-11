#ifndef UTILS_H
#define UTILS_H

int string_match(const char *pattern, const char *candidate);

int file_tree_foreach(const char *dirpath, void (*doit)(const char *, void *),
                      void *context);

#endif