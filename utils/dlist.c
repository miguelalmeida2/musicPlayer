#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "dlist.h"

Node *list_create()
{
    Node *list_sentinel = malloc(sizeof *list_sentinel);
    list_sentinel->next = list_sentinel;
    list_sentinel->prev = list_sentinel;
    return list_sentinel;
}

void list_delete(Node *list)
{
    Node *next;
    for (Node *p = list->next; p != list; p = next)
    {
        next = p->next;
        free(p);
    }
    free(list);
}

void list_insert_rear(Node *list, void *data)
{
    Node *new_node = malloc(sizeof *new_node);
    if (NULL == new_node)
    {
        fprintf(stderr, "Out of memory\n");
        exit(-1);
    }
    new_node->data = data;

    new_node->prev = list->prev;
    new_node->next = list;
    list->prev->next = new_node;
    list->prev = new_node;
}

void list_insert_front(Node *list, void *data)
{
    Node *new_node = malloc(sizeof *new_node);
    if (NULL == new_node)
    {
        fprintf(stderr, "Out of memory\n");
        exit(-1);
    }
    new_node->data = data;

    new_node->prev = list;
    new_node->next = list->next;
    list->next->prev = new_node;
    list->next = new_node;
}

void list_remove(Node *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    free(node);
}

void list_foreach(Node *list, void (*doit)(void *))
{
    for (Node *p = list->next; p != list; p = p->next)
        doit(p->data);
}

Node *list_search(Node *list, int (*predicate)(void *, void *), void *context)
{
    for (Node *p = list->next; p != list; p = p->next)
        if (predicate(p->data, context))
            return p;
    return NULL;
}

void *list_data(Node *p)
{
    return p->data;
}

Node *list_front(Node *list)
{
    return list->next;
}

Node *list_rear(Node *list)
{
    return list->prev;
}

int list_empty(Node *list)
{
    return list->next == list;
}

int list_size(Node *list)
{
    if (list == NULL)
    {
        return -1;
    }
    Node *head_node = list;
    if (head_node->next == NULL)
    {
        return 0;
    }
    int size = 0;
    Node *next_node = head_node->next;
    while (next_node != head_node)
    {
        size++;
        next_node = next_node->next;
    }
    return size;
}
