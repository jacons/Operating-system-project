#include <intlist.h>

#define LMALLOC(v, t, q)                          \
    if ((v = (t *)malloc(q * sizeof(t))) == NULL) \
    {                                             \
        printf("Error: memory allocation");       \
        exit(EXIT_FAILURE);                       \
    }

#define ISEMPTY(s) (s->head == NULL)

#define ISN(a) (a == NULL)


intllist *intllist_create(void) {

    intllist *p;
    LMALLOC(p, struct intstr_q, 1)

    p->head = p->tail = NULL;
    return p;
}
void intllist_push(intllist *s, unsigned long c) {
    intstrqueue p;
    LMALLOC(p, struct intnode, 1)

    p->num= c;
    p->next = NULL;

    // add in head
    if (ISN(s->head) && ISN(s->tail))
        s->head = s->tail = p;
    else
    {
        // add in tail
        s->tail->next = p;
        s->tail = p;
    }
}
void intllist_setdiff(intllist *s, unsigned long c)
{
    // add in head
    if (ISN(s->head) && ISN(s->tail)) {
        printf("Error with queue(per favore non bocciarmi non doveva capitare mai)");
        exit(0);
    } else {
        if (ISN(s->tail)) s->head->num = c - s->head->num;
        else s->tail->num = c - s->tail->num;
    }
}

long intllist_pop(intllist *s)
{

    unsigned long cl;

    intstrqueue h = NULL, p = NULL;

    if (ISN(s)) return -1;
    else if (ISN(s->head) && ISN(s->tail))
        return -1;

    h = s->head;

    cl = h->num;


    p = h->next;
    free(h);
    s->head = p;
    if (ISN(s->head)) s->tail = s->head;

    return cl;
}
void intllist_free(intllist *s)
{
    while (s->head)intllist_pop(s);
    free(s);
}

void intllist_print(intllist *ps)
{
    if (ps != NULL)
    {

        intstrqueue p = ps->head;

        while (p != NULL)
        {
            printf("%ld ", p->num);
            p = p->next;
        }
    }
    putchar('\n');
}
int intllist_len(intllist *l)
{
    int len = 0;

    if (ISN(l)) return -1;

    intstrqueue p = l->head;

    while (p != NULL)
    {
        len++;
        p = p->next;
    }
    return len;
}