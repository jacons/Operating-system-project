#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct intnode {
    unsigned long num;
    struct intnode *next;
} * intstrqueue;

typedef struct intstr_q
{
    intstrqueue head;
    intstrqueue tail;
} intllist;

// llist_create : create a new list
intllist *intllist_create(void);

// llist_push : add in tail the element
void intllist_push(intllist *, unsigned long);

// llist_push : remove from last element a value
void intllist_setdiff(intllist *, unsigned long);

// llist_pop :(-1 error) pop & remove first element of queue
long intllist_pop(intllist *);

// llist_free : pop and remove all element
void intllist_free(intllist *);

// llist_print : print elements
void intllist_print(intllist *);

// llisti_len : return number of elements in queue
int intllist_len(intllist *l);
