#ifndef DLIST_H
#define DLIST_H

typedef struct node
{
    struct node *next, *prev;
    void *data;
} Node;

Node *list_create();

void list_insert_front(Node *list, void *data);

void list_insert_rear(Node *list, void *data);

void list_foreach(Node *list, void (*doit)(void *));

Node *list_search(Node *list, int (*predicate)(void *, void *), void *context);

void list_remove(Node *node);

void list_delete(Node *list);

int list_empty(Node *list);

int list_size(Node *list);

void *list_data(Node *p);

Node *list_front(Node *list);

Node *list_rear(Node *list);

#endif
