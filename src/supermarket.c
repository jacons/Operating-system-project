#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <sys/timeb.h>
#include <limits.h>

#include <hlist.h>
#include <intlist.h>

#define DEBUG 0
#define DIRECTOR_CALL 1e6
#define KDEFAULT 2


#define MALLOC(v,t,q) if( (v =(t*)malloc(q*sizeof(t))) == NULL ) \
    { printf("Error: memory allocation"); pthread_exit((void*)EXIT_FAILURE);}

#define CALLOC(v,t,q) if( (v =(t*)calloc(q,sizeof(t))) == NULL ) \
    { printf("Error: memory allocation"); pthread_exit((void*)EXIT_FAILURE);}

#define MUTEX_LOCK(a,e) if( ( e = pthread_mutex_lock(&a) ) != 0 ) \
    { errno = e; perror("Mutex lock"); pthread_exit((void*)EXIT_FAILURE);}

#define MUTEX_UNLOCK(a,e) if( ( e = pthread_mutex_unlock(&a) ) != 0 ) \
    { errno = e; perror("Mutex Unlock"); pthread_exit((void*)EXIT_FAILURE);}

#define MUTEX_DESTROY(a,e) if( ( e = pthread_mutex_destroy(a) ) != 0 ) \
    { errno = e; perror("Mutex Destroy"); pthread_exit((void*)EXIT_FAILURE);}

#define COND_DESTROY(a,e) if( ( e = pthread_cond_destroy(a) ) != 0 ) \
    { errno = e; perror("Cond Destroy"); pthread_exit((void*)EXIT_FAILURE);}

#define SIGNAL(a,e) if( ( e = pthread_cond_signal(a) ) != 0 ) \
    { errno = e; perror("Pthread Signal"); pthread_exit((void*)EXIT_FAILURE);}

#define WAIT(c,m,e) if( ( e = pthread_cond_wait(c,m) ) != 0 ) \
    { errno = e; perror("Pthread wait"); pthread_exit((void*)EXIT_FAILURE);}

#define THREAD_CREATE(p,a,r,e)    \
     if( ( e = pthread_create(p,a,r,NULL) ) != 0 ) \
    { errno = e; perror("Pthread create"); pthread_exit((void*)EXIT_FAILURE);}

#define THREAD_CREATE2(p,a,r,arg,e)    \
     if( ( e = pthread_create(p,a,r,arg) ) != 0 ) \
    { errno = e; perror("Pthread create"); pthread_exit((void*)EXIT_FAILURE);}

#define JOIN(c,e) if( ( e = pthread_join(c,NULL) ) != 0 ) \
    { errno = e; perror("Pthread join"); pthread_exit((void*)EXIT_FAILURE);}

#define SC(a,i) if( a != 0 ) \
    { printf(i); pthread_exit((void*)EXIT_FAILURE);}

#define MUTEX_INIT(c,e) if( ( e = pthread_mutex_init(c,NULL) ) != 0 ) \
    { errno = e; perror("Pthread mutex init"); pthread_exit((void*)EXIT_FAILURE);}

#define COND_INIT(c,e) if( ( e = pthread_cond_init(c,NULL) ) != 0 ) \
    { errno = e; perror("Pthread cond init"); pthread_exit((void*)EXIT_FAILURE);}

#define SIGACTION(st,se,e) if( ( sigaction(se,&st,NULL) ) != 0 ) \
    { errno = e; perror("Error sigaction"); pthread_exit((void*)EXIT_FAILURE);}


#define SC_FILE(fd,c,i) if( (fd = c) == NULL) { perror(i);exit(1);}

#define ISEMPTY(s) (s->head == NULL)

typedef struct i_cassa {
    unsigned short id;            // Nominativo della cassa      
    unsigned int tot_clienti;     // Numero di clienti serviti
    unsigned int nprodotti;       // Numero di prodotti processati
    unsigned long tot_attivita;   // Tempo (ms) di attività
    unsigned int chiusure;        // Numero di chisure in "giornata"
    intllist *t_servizio_cliente; // lista dei tempi di ogni cliente servito
    intllist *t_ogni_apertura;    // lista dei tempi di ogni apertura della "giornata"
} infocassa;

typedef struct s_parameter {
    unsigned int K;       // Numero cassiere massime
    unsigned int C;       // Numuero di clienti nel supermercato
    unsigned int E;       // Soglia dove non deve scendere il numero di clienti
    unsigned int TMSP;    // Tempo massimo per la spesa
    unsigned int NPM;     // Numeri di prodotti massimi
    unsigned int FRQC;    // Frequenza di cambio coda
    unsigned int TPPMIN;  // Tempo per prodotto Minimo
    unsigned int TPPMAX;  // Tempo per prodotto Massimo
    unsigned int OPNEXTC; // Soglia massima per aprire la prossima cassa
    unsigned int CLBEFC;  // Soglia minima per chiudere l'ultima cassa
    char PATH[20];        // Nome del file di log
} parameter;

//----------------------------------
parameter param; // Struttura del file di configurazione
struct timespec timestamp;
bool *worker_on;                 // Array di Worker aperti
infocassa *workers_timeline;     // Cronologia delle casse
unsigned short *client_in_queue; // Array che mantiene il numero di clienti in coda per ogni cassa
struct timeb timer_msec;         // timestamp in millisecond.
//------------------------------------------
pthread_t p_thecloser; // Il thread specifico che si occupa della chisura del supermercato
//------------------------------------------
pthread_mutex_t *mutex_worker;                                  // Mutex per ogni cassa inizializzata
pthread_mutex_t customers_timeline = PTHREAD_MUTEX_INITIALIZER; // Mutex per cronologia dei tutti i clienti serviti
pthread_mutex_t population_markert = PTHREAD_MUTEX_INITIALIZER;  //  Mutex per la gestione contingentata del supermercato
pthread_mutex_t mutex_director = PTHREAD_MUTEX_INITIALIZER;     // Mutex per il direttore
//------------------------------------------
pthread_cond_t *wakeup_worker;                            //  Condition variables per ogni cassa
pthread_cond_t wakeup_usher = PTHREAD_COND_INITIALIZER;   // Condition variables per l'usciere
pthread_cond_t wakeupdirector = PTHREAD_COND_INITIALIZER; //condition variables per il direttore
//------------------------------------------
llist **workers_queue;     // Liste di clinti in coda per ogni cassa
llist *costumers_timeline; // Cronogia dei clienti serviti
llist *directers_queue;    // Lista dei clienti con 0 prodotti che devono uscire
//------------------------------------------
volatile sig_atomic_t closesupermarket = false; // Variabile che simula la chisura del supermercato
volatile sig_atomic_t stopclient = false;       // Variabile che simula la chisura delle porte
volatile unsigned int ncostumers;               // Numero di clienti nel supermercato
volatile unsigned short allworkers = KDEFAULT;  // Il numero di casse aperte
volatile unsigned short check_w_queue = false;
//------------------------------------------

void *Portiere();
void *Costumer();
void *WorkerCassa(void *);
void *Closesupermarket();
void *director();


static void Signal_close_supermarker(int signum){
    closesupermarket = true;
    pthread_kill(p_thecloser, SIGQUIT);
    return;
}
static void Signal_close_door(int signum) {
    stopclient = true;
    pthread_kill(p_thecloser, SIGHUP);
    return;
}
//------------------------------------------
void cleanup() {

    unsigned short j;
    int error;

    for (j = 0; j < param.K; j++) {
        MUTEX_DESTROY(&mutex_worker[j], error)
        COND_DESTROY(&wakeup_worker[j], error)
        llist_free(workers_queue[j]);
    }
    MUTEX_DESTROY(&population_markert, error)
    MUTEX_DESTROY(&mutex_director, error)
    MUTEX_DESTROY(&customers_timeline, error)

    COND_DESTROY(&wakeup_usher, error)
    COND_DESTROY(&wakeupdirector, error)

    llist_free(directers_queue);

    free(mutex_worker);
    free(wakeup_worker);
    free(worker_on);
    free(workers_queue);
    free(client_in_queue);

    printf("\nPer visualizzare i risultati digitare 'make result' \nSoftware Close\n");
}
int explode(char ***arr_ptr, char *str, char delimiter)
{
    char *src = str, *end, *dst,**arr;
    int size = 1, i;

    // Find number of strings
    while ((end = strchr(src, delimiter)) != NULL) {
        ++size;
        src = end + 1;
    }

    if( (arr = malloc( size * sizeof(char *) + (strlen(str) + 1) * sizeof(char) )) == NULL )
        {printf("Error: memory allocation"); pthread_exit((void*)EXIT_FAILURE);}

    src = str;
    dst = (char *)arr + size * sizeof(char *);

    for (i = 0; i < size; ++i) {
        if ((end = strchr(src, delimiter)) == NULL) end = src + strlen(src);
        arr[i] = dst;
        strncpy(dst, src, end - src);
        dst[end - src] = '\0';
        dst += end - src + 1;
        src = end + 1;
    }
    *arr_ptr = arr;

    return size;
}
void open_loadfile() {

    FILE *fd;
    char buff[200]; // buffer di linea per il file di confg
    char **params; // Array di parametri di configurazione che si crea con l'esplode

    memset(buff, 0, 200); 

    SC_FILE(fd, fopen("./output/config.txt", "r"), "Open file")

    // Legge il file di confg in una sola riga
    if (fgets(buff, 100, fd) == NULL)
        { printf("Errore nella lettura del file di configurazione!\n"); exit(EXIT_FAILURE);}

    if (explode(&params, buff, ';') != 11) 
        { printf("Errore nella lettura dei parametri del file di configurazione");exit(EXIT_FAILURE); }

    // parsing dei parametri
    param.K       = atoi(params[0]);
    param.C       = atoi(params[1]);
    param.E       = atoi(params[2]);
    param.TMSP    = atoi(params[3]);
    param.NPM     = atoi(params[4]);
    param.FRQC    = atoi(params[5]);
    param.TPPMIN  = atoi(params[6]);
    param.TPPMAX  = atoi(params[7]);
    param.OPNEXTC = atoi(params[8]);
    param.CLBEFC  = atoi(params[9]);

    if (!strlen(params[10]))
     { printf("Il nome del file di statistica non può essere vuoto!\n"); exit(EXIT_FAILURE); }

    strncpy(param.PATH,params[10], 19);

    free(params);
    SC(fclose(fd), "Close file")
}
void createlog() {
    // lunghezza dell nome dell file di statistica
    unsigned short npath = strlen(param.PATH), j;
    FILE *fd;
    char *path;
    unsigned long *tmpclients; // puntatore all'elemento della lista
    
    // alloca e azzera
    CALLOC(path, char, (npath + 14));

    strncpy(path, "./output/", 10);
    strncat(path, param.PATH, npath);
    strncat(path, ".txt", 5);
    SC_FILE(fd, fopen(path, "w+"), "Creating file")

    // -------- Inserimento dei Clienti del log --------
    // In questo ordine (Nome|Prodotti|Tempo totale di permamenza|Tempo speso in coda|Numero di code visitate)
    // Oss: Code visitate = 1 (X versione semplificata)????
    while (costumers_timeline->head) {
        tmpclients = llist_pop(costumers_timeline); 
        fprintf(fd,"Customer: |%ld|%2ld|%2.3f|%2.3f|%ld|\n", tmpclients[0], tmpclients[1],(float)(tmpclients[4]+tmpclients[2])/1000,(float)tmpclients[4]/1000,tmpclients[5]);
        free(tmpclients); // Leggo l'elemento della lista poi lo dealloco
    }
    fprintf(fd, "\n");
    free(costumers_timeline);
    // -------- Inserimento dei Clienti del log --------

    // -------- Inserimento dei Cassieri del log --------

    //---------------   INSERISCE I CASSIERI -------------------
    // In questo ordine (Id Cassa|Numero di prodotti processati|Tempo totale apetura|Tempo medio clienti|Numero chisure)
    unsigned long t_servizio_m,t_aperture; // Per calcolare i tempi medi
    unsigned short c1,c2; // Varibili contatore

    for (j = 0; j < param.K; j++) {

        t_servizio_m = 0;c1 = 0;

        while (workers_timeline[j].t_servizio_cliente->head) {
            t_servizio_m += intllist_pop(workers_timeline[j].t_servizio_cliente);
            c1++;
        }
        free(workers_timeline[j].t_servizio_cliente);

        t_aperture = 0;c2 = 0;
        while (workers_timeline[j].t_ogni_apertura->head) {
            t_aperture += intllist_pop(workers_timeline[j].t_ogni_apertura);
            c2++;
        }
        free(workers_timeline[j].t_ogni_apertura);

        fprintf(fd, "Worker: %2d|%3d|%3d|%2.3f|%2.3f|%d\n",
                workers_timeline[j].id, workers_timeline[j].nprodotti, workers_timeline[j].tot_clienti,
                (!c1) ? 0 : (float)t_servizio_m / (c1 * 1000),
                (!c2) ? 0 : (float) t_aperture  / (c2 * 1000), workers_timeline[j].chiusure);
    }
    free(workers_timeline);
    // -------- Inserimento dei Clienti del log --------
    free(path);
    SC(fclose(fd), "Close file")
}
unsigned long get_time() {
    clock_gettime(CLOCK_REALTIME, &timestamp); 
    return (timestamp.tv_sec) * 1000 + (timestamp.tv_nsec) / 1000000;
}
void *Portiere() {
    // Si occupa di far rientrare le persone una volta che sono uscite
    unsigned short i;
    int error;
    pthread_t clienti[param.E];
    pthread_attr_t tattr;

    SC(pthread_attr_init(&tattr), "Error : Pthread attr init");
    SC(pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED), "Error : Pthread setdetachstate");

    while (1) {

        MUTEX_LOCK(population_markert, error)
        // fino a quando non si scende sotto la soglia o il supermercato è chiuso sta in wait
        while (((param.C - ncostumers) <= param.E) && closesupermarket == false && stopclient == false)
            WAIT(&wakeup_usher, &population_markert, error)


        if (closesupermarket == true || stopclient == true) {
            MUTEX_UNLOCK(population_markert, error)
            break;
        }
        if ((param.C - ncostumers) > param.E)
        {
            // Sono uscite altre persone quindi ne faccio entrare altre
            for (i = 0; i < param.E; i++){
                printf("Faccio entrare altra gente\n");
                THREAD_CREATE(clienti + i, &tattr, Costumer, error)
                ncostumers ++;
            }
        }
       if(DEBUG) printf("\nAttualmente: %d\n", ncostumers);
        MUTEX_UNLOCK(population_markert, error)
    }
    if (DEBUG) printf("Il portiere ne se va!!\n");
    pthread_exit(EXIT_SUCCESS);
}
void *Closesupermarket() {

    sigset_t set;
    int error,sig;
    unsigned short i;

    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGHUP);
    pthread_sigmask(SIG_SETMASK, &set, NULL);

    while (1) {
        sigwait(&set, &sig);

        if (stopclient == true)
        {
            // In questa situazione i clienti non possono più entrare
            // di conseguenza il l'usciere lo mando via,quindi segnalo che è arrivato il "Segnale"
            MUTEX_LOCK(population_markert, error)
            SIGNAL(&wakeup_usher, error)
            MUTEX_UNLOCK(population_markert, error)

            // Le casse che stanno servendo lo hanno capito che
            // appena finito la coda devono uscire, ma quelle che stanno in wait? 
            // mando un segnale a tutti quelle che non hanno clienti escono subito
            for (i = 0; i < param.K; i++) {
                MUTEX_LOCK(mutex_worker[i], error)
                SIGNAL(&wakeup_worker[i], error)
                MUTEX_UNLOCK(mutex_worker[i], error)
            }

            break;
        }
        if (closesupermarket == true) {
            // Sveglio tutti gli attori del supermercato
            


            //Sveglio l'usciere
            MUTEX_LOCK(population_markert, error)
            SIGNAL(&wakeup_usher, error)
            MUTEX_UNLOCK(population_markert, error)

            // Nell' eveutalità che le cassiere sono in wait mando il segnale a tutti
            for (i = 0; i < param.K; i++) {
                MUTEX_LOCK(mutex_worker[i], error)
                worker_on[i] = false; // cosi che i clienti possano immediatamente uscire
                SIGNAL(&wakeup_worker[i], error)
                MUTEX_UNLOCK(mutex_worker[i], error)
            }
            break;
        }
    }
     if (DEBUG) printf("The closer esce\n");
    pthread_exit(EXIT_SUCCESS);
}
void *Costumer() {

    int error;
    long *pcli; // Puntatore all' element della lista
    time_t t;
    unsigned int seed = pthread_self() % UINT_MAX - 1;
    unsigned short random_worker; // Numero casuale della cassiera
    bool found = false; // Cassiera libera trovata

    // creo la struttura che conterrà il customer
    unsigned long customer[] = {
        pthread_self(),                                                 // Nome cliente
        rand_r(&seed) % param.NPM,                                      // Prodotti acquistati
        (rand_r(&seed) % (param.TMSP - 10)) + 10,                       // Tempo (ms) per spesa
        (rand_r(&seed) % (param.TPPMAX - param.TPPMIN)) + param.TPPMIN, // Tempo (ms) per prodotto
        0,                                                              // Tempo (ms) per coda
        1,                                                              // Numero di code cambiate
    };
    struct timespec ts = {(customer[2] / 1000), ((customer[2] % 1000) * 1000000)};
    nanosleep(&ts, NULL);

    while (!found) {
        random_worker = rand_r(&seed) % allworkers;

        if(DEBUG) printf("Sono %ld (%ld ms) e cerco una cassiera(%d)\n", customer[0], customer[2], random_worker);
        
        // Check supermarket 
        if (customer[1] == 0 || closesupermarket == true) {
            // Situazione 1) non ha nessun prodotto che faccio a fare la cosa?
            // Situazione 2) Il supermercato chiuso vado dal direttore
            if (DEBUG)printf("Vado dal direttore %ld\n", customer[0]);
            MUTEX_LOCK(mutex_director, error);
            llist_push(directers_queue, customer);
            SIGNAL(&wakeupdirector, error)
            MUTEX_UNLOCK(mutex_director, error);
            found = true;
        }
        // controllo se la cassiera è attiva, altrimenti cambio 
        else if (worker_on[random_worker] == true) {

            customer[4] = get_time(); // salvo il "momento" in cui mi metto in coda
            customer[5]++; // mi sono messo in coda
            llist_push(workers_queue[random_worker], customer);
            // mi metto in coda alla mutex
            MUTEX_LOCK(mutex_worker[random_worker], error)

            // una volta acqusita la mutex verifico che la cassiera sia ancora attiva
            if (worker_on[random_worker] == true){

                // la "sveglio" perchè potrebbe stare in una condizione di wait
                SIGNAL(&wakeup_worker[random_worker], error)
                if (DEBUG) printf("Entro in cassa %ld\n", customer[0]);
                found = true;
            }
            else
            {
                // se la mutex_worker è chiusa mi levo dalla coda no
                if (!ISEMPTY(workers_queue[random_worker])) {
                    pcli = llist_pop(workers_queue[random_worker]);
                    free(pcli);
                }
                // ok toccava a me ma la mutex_worker è chiusa
                if(DEBUG) printf("Cassa chiusa ne devo cercare un altra!\n");
            }
            MUTEX_UNLOCK(mutex_worker[random_worker], error)
        }
    }
    pthread_exit(EXIT_SUCCESS);
}
void *WorkerCassa(void *arg){

    unsigned short id = *((int *)arg);
    int error;
    unsigned long *pcli;

    // utlilizzato per la generazione dei numeri random
    time_t t;
    struct timespec ts;
    srand((unsigned)time(&t) * (id + 1));

    //80-20 = 60ms
    unsigned short Kcost = (rand() % 60) + 20; // Tempo costante
    unsigned int timetosleep;
    unsigned long timeInterval = clock();

    printf("Buongiorno sono la cassa %d\n", id);
    workers_timeline[id].id = id;
    while (1)
    {
        // mutua escusione sulla mutex_worker
        MUTEX_LOCK(mutex_worker[id], error)
        // la coda è vuota mi metti in attesa
        while (ISEMPTY(workers_queue[id]) && closesupermarket == false && stopclient == false)
            WAIT(&wakeup_worker[id], &mutex_worker[id], error)

        if (closesupermarket == true || (stopclient == true && !llist_len(workers_queue[id]))) 
        {
            
            if(worker_on[id]==true)
                intllist_setdiff(workers_timeline[id].t_ogni_apertura, get_time());

            printf("La cassiera %d esce\n",id);
            MUTEX_UNLOCK(mutex_worker[id], error)
            break;
        }
        if (stopclient == true) printf("Sono %d sto facendo gli ultimi clienti\n", id);
        if (!ISEMPTY(workers_queue[id]))
        {
            // Rimuovo il customer della coda
            pcli = llist_pop(workers_queue[id]);

            // Lo pongo nella cronologia
            pcli[4] = get_time() - pcli[4];

            MUTEX_LOCK(customers_timeline, error);
            llist_push(costumers_timeline, pcli);
            MUTEX_UNLOCK(customers_timeline, error);

            // "Passo gli articoli"
            timetosleep = Kcost + pcli[1] * pcli[3];
            ts.tv_sec = (timetosleep / 1000);
            ts.tv_nsec = (timetosleep % 1000) * 1000000;
            nanosleep(&ts, NULL);


            workers_timeline[id].nprodotti += pcli[1];
            workers_timeline[id].tot_clienti++;
            intllist_push(workers_timeline[id].t_servizio_cliente, pcli[4] + pcli[2]+timetosleep);

            printf("Sono %d Elaboro: %ld (%d)ms coda :%d \n",id,pcli[0], timetosleep,llist_len(workers_queue[id]));
        
            MUTEX_LOCK(population_markert, error)
            ncostumers--;
            if ((param.C - ncostumers) > param.E ) { 
                SIGNAL(&wakeup_usher, error) 
            }
            MUTEX_UNLOCK(population_markert, error)

            free(pcli);

            if (stopclient == false && (get_time() - timeInterval) > 200)
            {
                MUTEX_LOCK(mutex_director, error)
                client_in_queue[id] = llist_len(workers_queue[id]);
                //if(DEBUG) printf("\n\nA DIRETTORE(%d) %d\n\n", client_in_queue[id], id);
                check_w_queue = true;
                SIGNAL(&wakeupdirector, error)
                MUTEX_UNLOCK(mutex_director, error)
                timeInterval = get_time();
            }
        }
        MUTEX_UNLOCK(mutex_worker[id], error)
    }
    printf("Cassiera uscita\n");
    pthread_exit(EXIT_SUCCESS);
}
void *director() {
    unsigned short j;
    int error;
    long *clienteexit; // puntatore all'elemento della lista (per deallocazione)

    if(DEBUG)printf("Buongiono sono il direttore!!\n");
    while (1) {
        MUTEX_LOCK(mutex_director, error)

        while (ISEMPTY(directers_queue) && check_w_queue == false && closesupermarket == false && stopclient == false)
            WAIT(&wakeupdirector, &mutex_director, error)

        if (!ISEMPTY(directers_queue)) {

            if(DEBUG)printf("Il customer con 0 prodotti esce con la mia autorizzazione\n");

            clienteexit = llist_pop(directers_queue);
            MUTEX_LOCK(customers_timeline, error);
            llist_push(costumers_timeline, clienteexit);
            MUTEX_UNLOCK(customers_timeline, error);
            free(clienteexit);

            MUTEX_LOCK(population_markert, error)
            ncostumers--;
            if ((param.C - ncostumers) > param.E)SIGNAL(&wakeup_usher, error)
            MUTEX_UNLOCK(population_markert, error)
        }
        if (closesupermarket == true || (stopclient == true )) {
            MUTEX_UNLOCK(mutex_director, error)
            break;
        }
        // apertura e chisura casse per valori soglia
        if (check_w_queue == true) {

            if (allworkers < param.K && client_in_queue[allworkers - 1] > param.OPNEXTC) {
                // in questo caso 1) è possibile aprire un altra cassa 2) i clienti in coda superano la soglia
                if(DEBUG) printf("\nApro una cassa\n");
                worker_on[allworkers] = true;
                intllist_push(workers_timeline[allworkers].t_ogni_apertura, get_time());
                allworkers++;
            }

            if (allworkers > 1 && client_in_queue[allworkers - 1] < param.CLBEFC) {
                // in questo caso 1) si può chiudere una cassa 2) nell'ultima cassa aperta ci sonon poche persone
                if(DEBUG) printf("\nChiudo una cassa\n");
                allworkers--;
                worker_on[allworkers] = false;
                intllist_setdiff(workers_timeline[allworkers].t_ogni_apertura, get_time());
                workers_timeline[allworkers].chiusure++;
            }

            check_w_queue = false;
        }
        MUTEX_UNLOCK(mutex_director, error)
    }
    if(DEBUG)printf("Il direttore se ne va!\n");
    pthread_exit(EXIT_SUCCESS);
}
int main()
{
    atexit(cleanup);

    open_loadfile();

    int error;
    unsigned short i;
    unsigned short ind[param.K];
    pthread_t p_cassiera[param.K], p_portiere, p_director, p_clienti[param.C]; // Attori del supermecato
    pthread_attr_t tattr;
    struct sigaction s1; // segnale per la chisura immediata
    struct sigaction s2; // segnale per la chisura delle porte

    ncostumers = param.C; // numero dei clienti che entrerà subito nel supermercato

    memset(&s1, 0, sizeof(struct sigaction));
    memset(&s2, 0, sizeof(struct sigaction));

    SC(pthread_attr_init(&tattr), "Error: Pthread attr init")
    SC(pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED), "Error: pthread attr setdetachstate")

    s1.sa_handler = Signal_close_supermarker;
    s2.sa_handler = Signal_close_door;

    SIGACTION(s1, SIGQUIT, error);
    SIGACTION(s2, SIGHUP, error);

    MALLOC(mutex_worker, pthread_mutex_t, param.K)
    MALLOC(wakeup_worker,pthread_cond_t, param.K)
    MALLOC(worker_on, bool, param.K)
    MALLOC(workers_queue, llist *, param.K)
    CALLOC(workers_timeline, infocassa, param.K)
    CALLOC(client_in_queue, unsigned short, param.K)

    for (i = 0; i < param.K; i++) {
        workers_queue[i] = llist_create();

        workers_timeline[i].t_ogni_apertura = intllist_create();
        workers_timeline[i].t_servizio_cliente = intllist_create();

        MUTEX_INIT(&mutex_worker[i], error)
        COND_INIT(&wakeup_worker[i], error)
    }
    directers_queue = llist_create();
    costumers_timeline = llist_create();

    // Accendo il Chiusore
    THREAD_CREATE(&p_thecloser, NULL, Closesupermarket, error)
    THREAD_CREATE(&p_director, NULL, director, error)

   // inizializzo tutte le casse
    for (i = 0; i < param.K; i++) {
        ind[i] = i;
        THREAD_CREATE2(&p_cassiera[i], NULL, WorkerCassa, (void *)&ind[i], error)
        worker_on[i] = false; // all'inizio tutte le casse sono "dormienti" le "attivo" per necessità
    }
  
    // per le casse di default inserisco il timestamp dell inizio del servizio
    for (i = 0; i < KDEFAULT; i++) {
        worker_on[i] = true; // e le imposto come attive
        intllist_push(workers_timeline[i].t_ogni_apertura, get_time());
    }
        
    THREAD_CREATE(&p_portiere, NULL, Portiere, error)

    for (i = 0; i < param.C; i++) THREAD_CREATE(p_clienti + i, &tattr, Costumer, error)


    for (i = 0; i < param.C; i++) pthread_join(p_clienti[i], NULL);
    for (i = 0; i < param.K; i++) pthread_join(p_cassiera[i], NULL);

    MUTEX_LOCK(mutex_director, error)
    SIGNAL(&wakeupdirector, error)
    MUTEX_UNLOCK(mutex_director, error)

    pthread_join(p_portiere, NULL);
    pthread_join(p_director, NULL);
    pthread_join(p_thecloser, NULL);


    createlog();

    return EXIT_SUCCESS;
}
