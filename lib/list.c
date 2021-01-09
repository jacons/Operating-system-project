#include<hlist.h>

#define LMALLOC(v,t,q) if( (v =(t*)malloc(q*sizeof(t))) == NULL ) \
    { printf("Error: memory allocation"); exit(EXIT_FAILURE);}

#define ISEMPTY(s) (s->head == NULL)

#define ISN(a) (a==NULL)

llist *llist_create(void) {

    llist *p;
    LMALLOC(p,struct str_q,1)

    p->head = p->tail = NULL;
    return p;
}
void llist_push(llist *s, unsigned long c[]) {
    strqueue p;
    LMALLOC(p,struct node,1)

    p->num[0] = c[0];
    p->num[1] = c[1];
    p->num[2] = c[2];
    p->num[3] = c[3];
    p->num[4] = c[4];
    p->num[5] = c[5];

    p->next = NULL;

    // add in head
    if (ISN(s->head) && ISN(s->tail)) s->head = s->tail = p;
    else {
        // add in tail
        s->tail->next = p;
        s->tail = p;
    }
}

unsigned long *llist_pop(llist *s) {

    unsigned long *cl;
    LMALLOC(cl,unsigned long,6)

    strqueue h = NULL, p = NULL;

    if (ISN(s)) return NULL;
    else if (ISN(s->head) && ISN(s->tail)) return NULL;

    h = s->head;

    cl[0] = h->num[0];
    cl[1] = h->num[1];
    cl[2] = h->num[2];
    cl[3] = h->num[3];
    cl[4] = h->num[4];
    cl[5] = h->num[5];

    p = h->next;
    free(h);
    s->head = p;
    if (ISN(s->head)) s->tail = s->head;

    return cl;
}
void llist_free(llist *s) {

    unsigned long *pcli;
    while (s->head) {
        pcli = llist_pop(s);
        free(pcli);
    }
    free(s);
}

void llist_print(llist *ps) {
    if (ps != NULL) {

        strqueue p = ps->head;

        while (p != NULL) {
            printf("%ld ", p->num[0]);
            p = p->next;
        }
    }
    putchar('\n');
}
int llist_len(llist *l) {
    int len = 0;

    if (ISN(l)) return -1;

    strqueue p = l->head;

    while (p != NULL) {
        len++;
        p = p->next;
    }
    return len;
}