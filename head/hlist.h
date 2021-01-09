#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct node {
    unsigned long num[6];
    struct node *next;
} * strqueue;

typedef struct str_q {
    strqueue head;
    strqueue tail;
} llist;

// llist_create : create a new list
llist *llist_create(void);

// llist_push : add in tail the element
void llist_push(llist *, unsigned long[]);

// llist_pop : pop & remove first element of queue
unsigned long *llist_pop(llist *);

// llist_free : pop and remove all element
void llist_free(llist *);

// llist_print : print elements
void llist_print(llist *);

// llisti_len : return number of elements in queue
int llist_len(llist *l);
