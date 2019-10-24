#include <stdio.h>
#include <pthread.h>
#include "phil_starve_included.h"
#define THINKING 0
#define HUNGRY 1
#define EATING 2

typedef struct {
  pthread_mutex_t *mon;
  pthread_cond_t **cv;
  int *state;
  int phil_count;
} Phil;


void pickup(Phil_struct *ps)
{
  Phil *pp;
  int phil_count;

  pp = (Phil *) ps->v;
  phil_count = pp->phil_count;
  
  pthread_mutex_lock(pp->mon);
  pp->state[ps->id] = HUNGRY;
  while (pp->state[(ps->id + (phil_count-1))%phil_count] == EATING ||
         pp->state[(ps->id + 1)%phil_count] == EATING) {
    pthread_cond_wait(pp->cv[ps->id], pp->mon);
  }
  pp->state[ps->id] = EATING;
  pthread_mutex_unlock(pp->mon);
}

void putdown(Phil_struct *ps)
{
  Phil *pp;
  int phil_count;

  pp = (Phil *) ps->v;
  phil_count = pp->phil_count;

  pthread_mutex_lock(pp->mon);
  pp->state[ps->id] = THINKING;
  if (pp->state[(ps->id+(phil_count-1))%phil_count] == HUNGRY) {
    pthread_cond_signal(pp->cv[(ps->id+(phil_count-1))%phil_count]);
  }
  if (pp->state[(ps->id+1)%phil_count] == HUNGRY) {
    pthread_cond_signal(pp->cv[(ps->id+1)%phil_count]);
  }
  pthread_mutex_unlock(pp->mon);
}

void *initialize_v(int phil_count)
{
  Phil *pp;
  int i;

  pp = (Phil *) malloc(sizeof(Phil));
  pp->phil_count = phil_count;
  pp->mon = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
  pp->cv = (pthread_cond_t **) malloc(sizeof(pthread_cond_t *)*phil_count);
  if (pp->cv == NULL) { perror("malloc"); exit(1); }
  pp->state = (int *) malloc(sizeof(int)*phil_count);
  if (pp->state == NULL) { perror("malloc"); exit(1); }
  pthread_mutex_init(pp->mon, NULL);
  for (i = 0; i < phil_count; i++) {
    pp->cv[i] = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
    if (pp->cv[i] == NULL) { perror("malloc"); exit(1); }
    pthread_cond_init(pp->cv[i], NULL);
    pp->state[i] = THINKING;
  }

  return (void *) pp;
}

void *philosopher(void *v)
{
  Phil_struct *ps;
  long st;
  long t;

  ps = (Phil_struct *) v;

  while(1) {

    /* First the philosopher thinks for a random number of seconds */

    st = (random()%ps->ms) + 1;
    printf("%3d Philosopher %d thinking for %d second%s\n", 
                time(0)-ps->t0, ps->id, st, (st == 1) ? "" : "s");
    fflush(stdout);
    sleep(st);

    /* Now, the philosopher wakes up and wants to eat.  He calls pickup
       to pick up the chopsticks */

    printf("%3d Philosopher %d no longer thinking -- calling pickup()\n", 
            time(0)-ps->t0, ps->id);
    fflush(stdout);
    t = time(0);
    pthread_mutex_lock(ps->blockmon);
    ps->blockstarting[ps->id] = t;
    pthread_mutex_unlock(ps->blockmon);

    pickup(ps);

    pthread_mutex_lock(ps->blockmon);
    ps->blocktime[ps->id] += (time(0) - t);
    ps->blockstarting[ps->id] = -1;
    pthread_mutex_unlock(ps->blockmon);

    /* When pickup returns, the philosopher can eat for a random number of
       seconds */

    st = (random()%ps->ms) + 1;
    printf("%3d Philosopher %d eating for %d second%s\n", 
                time(0)-ps->t0, ps->id, st, (st == 1) ? "" : "s");
    fflush(stdout);
    sleep(st);

    /* Finally, the philosopher is done eating, and calls putdown to
       put down the chopsticks */

    printf("%3d Philosopher %d no longer eating -- calling putdown()\n", 
            time(0)-ps->t0, ps->id);
    fflush(stdout);
    putdown(ps);
  }
}

main(argc, argv)
int argc; 
char **argv;
{
  int i;
  pthread_t *threads;
  Phil_struct *ps;
  void *v;
  long t0, ttmp, ttmp2;
  pthread_mutex_t *blockmon;
  int *blocktime;
  int *blockstarting;
  char s[500];
  int phil_count;
  char *curr;
  int total;

  if (argc != 3) {
    fprintf(stderr, "usage: dphil philosopher_count maxsleepsec\n");
    exit(1);
  }

  srandom(time(0));
  
  phil_count = atoi(argv[1]);
  threads = (pthread_t *) malloc(sizeof(pthread_t)*phil_count);
  if (threads == NULL) { perror("malloc"); exit(1); }
  ps = (Phil_struct *) malloc(sizeof(Phil_struct)*phil_count);
  if (ps == NULL) { perror("malloc"); exit(1); }
  
   
  v = initialize_v(phil_count);
  t0 = time(0);
  blocktime = (int *) malloc(sizeof(int)*phil_count);
  if (blocktime == NULL) { perror("malloc blocktime"); exit(1); }
  blockstarting = (int *) malloc(sizeof(int)*phil_count);
  if (blockstarting == NULL) { perror("malloc blockstarting"); exit(1); }

  blockmon = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(blockmon, NULL);
  for (i = 0; i < phil_count; i++) {
    blocktime[i] = 0;
    blockstarting[i] = -1;
  }

  for (i = 0; i < phil_count; i++) {
    ps[i].id = i;
    ps[i].t0 = t0;
    ps[i].v = v;
    ps[i].ms = atoi(argv[2]);
    ps[i].blocktime = blocktime;
    ps[i].blockstarting = blockstarting;
    ps[i].blockmon = blockmon;
    ps[i].phil_count = phil_count;
    pthread_create(threads+i, NULL, philosopher, (void *) (ps+i));
  }

  while(1) {
    pthread_mutex_lock(blockmon);
    ttmp = time(0);
    curr = s;
    total = 0;
    for(i=0; i < phil_count; i++) {
      total += blocktime[i];
      if (blockstarting[i] != -1) total += (ttmp - blockstarting[i]);
    }
    sprintf(curr,"%3d Total blocktime: %5d : ", ttmp-t0, total);

    curr = s + strlen(s);

    for(i=0; i < phil_count; i++) {
      ttmp2 = blocktime[i];
      if (blockstarting[i] != -1) ttmp2 += (ttmp - blockstarting[i]);
      sprintf(curr, "%5d ", ttmp2);
      curr = s + strlen(s);
    }
    pthread_mutex_unlock(blockmon);
    printf("%s\n", s);
    fflush(stdout);
    sleep(10);
  }
}
