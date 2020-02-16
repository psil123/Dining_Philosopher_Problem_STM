#include <stdio.h>
#include<stdlib.h>
#include <pthread.h>

#define THINKING 0
#define HUNGRY 1
#define EATING 2

typedef struct
{
int id;
long program_starting_time,maximum_sleeping_time;
int *total_blocking_time,*block_starting_time;
void *v;
int philosopher_count;
pthread_mutex_t *blocktime_monitor;
} philosopher_structure;

typedef struct {
  pthread_mutex_t *monitor;
  pthread_cond_t **checker;
  int *state;
  int *tblock;
 int *blocking_time_count;
  int philosopher_count;
} philosopher;


int can_I_eat(philosopher *pp, int n, int ms)
{
  int t;
  int philosopher_count;

  struct timeval tv;
  philosopher_count = pp->philosopher_count;

  if (pp->state[(n+(philosopher_count-1))%philosopher_count] == EATING ||
      pp->state[(n+1)%philosopher_count] == EATING) return 0;
  gettimeofday(&tv,NULL);
  t=tv.tv_sec*(int)1e6+tv.tv_usec;
  if (pp->state[(n+(philosopher_count-1))%philosopher_count] == HUNGRY &&
      pp->tblock[(n+(philosopher_count-1))%philosopher_count] < pp->tblock[n] &&
      (t - pp->tblock[(n+(philosopher_count-1))%philosopher_count]) >= ms*16000000) return 0;
  if (pp->state[(n+1)%philosopher_count] == HUNGRY &&
      pp->tblock[(n+1)%philosopher_count] < pp->tblock[n] &&
      (t - pp->tblock[(n+1)%philosopher_count]) >= ms*16000000) return 0;
  return 1;
}

void take_fork(philosopher_structure *ps)
{
  philosopher *pp;
  struct timeval tv;
  pp = (philosopher *) ps->v;

  pthread_mutex_lock(pp->monitor);
  pp->state[ps->id] = HUNGRY;
  gettimeofday(&tv,NULL);
  pp->tblock[ps->id] = tv.tv_sec*(int)1e6+tv.tv_usec;
  while (!can_I_eat(pp, ps->id, ps->maximum_sleeping_time)) {
    pthread_cond_wait(pp->checker[ps->id], pp->monitor);
  }
  pp->state[ps->id] = EATING;
  pthread_mutex_unlock(pp->monitor);
}

void put_fork(philosopher_structure *ps)
{
  philosopher *pp;
  int philosopher_count;

  pp = (philosopher *) ps->v;
  philosopher_count = pp->philosopher_count;

  pthread_mutex_lock(pp->monitor);
  pp->state[ps->id] = THINKING;
  if (pp->state[(ps->id+(philosopher_count-1))%philosopher_count] == HUNGRY) {
    pthread_cond_signal(pp->checker[(ps->id+(philosopher_count-1))%philosopher_count]);
  }
  if (pp->state[(ps->id+1)%philosopher_count] == HUNGRY) {
    pthread_cond_signal(pp->checker[(ps->id+1)%philosopher_count]);
  }
  pthread_mutex_unlock(pp->monitor);
}

void *initialize_v(int philosopher_count)
{
  philosopher *pp;
  int i;

  pp = (philosopher *) malloc(sizeof(philosopher));
  pp->philosopher_count = philosopher_count;
  pp->monitor = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
  pp->checker = (pthread_cond_t **) malloc(sizeof(pthread_cond_t *)*philosopher_count);
  if (pp->checker == NULL) { perror("malloc"); exit(1); }

pp->blocking_time_count=(int*)malloc(philosopher_count*sizeof(int));
        if(pp->blocking_time_count==NULL)
        {
                perror("malloc total_blocking_time not defined");
                exit(1);
}

 pp->state = (int *) malloc(sizeof(int)*philosopher_count);
  if (pp->state == NULL) { perror("malloc"); exit(1); }
  pp->tblock = (int *) malloc(sizeof(int)*philosopher_count);
  if (pp->tblock == NULL) { perror("malloc"); exit(1); }
  pthread_mutex_init(pp->monitor, NULL);
  for (i = 0; i < philosopher_count; i++) {
    pp->checker[i] = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
    pthread_cond_init(pp->checker[i], NULL);
    pp->state[i] = THINKING;
    pp->tblock[i] = 0;
  }

  return (void *) pp;
}





void *philosopher_create(void *v)
{
  struct timeval tv1,tv2;
        philosopher_structure *ps;
        long int st,t;

        ps=(philosopher_structure*)v;
        philosopher* pp=(philosopher*)ps->v;
        while(1)
        {
                gettimeofday(&tv1,NULL);
                pthread_mutex_lock(ps->blocktime_monitor);
                ps->block_starting_time[ps->id] = tv1.tv_sec*(int)1e6+tv1.tv_usec;//block time not added yet
                pthread_mutex_unlock(ps->blocktime_monitor);
                take_fork(ps);

                //calculate block time
                pthread_mutex_lock(ps->blocktime_monitor);
                gettimeofday(&tv2,NULL);
                if((tv2.tv_sec-tv1.tv_sec)*(int)1e6+(tv2.tv_usec-tv1.tv_usec)>=16*ps->maximum_sleeping_time)
                        pp->blocking_time_count[ps->id]++;
                ps->block_starting_time[ps->id] = -1; // indicates that blocktime has been added
               pthread_mutex_unlock(ps->blocktime_monitor);

            usleep(ps->maximum_sleeping_time);

                put_fork(ps);
}

}


int main(int argc,char **argv)
{
    struct timeval timev;
        pthread_t *threads;//store all threads
        philosopher_structure *ps;
        philosopher* pp;
        int i,maximum_sleeping_time=atoi(argv[2]);
        void *v;
        long int t_temp1,t_temp2;
        long int *total_blocking_time,*block_starting_time;
        int philosopher_count;
        pthread_mutex_t *blocktime_monitor;//mutex to block all process while calc and printing block_time
        //char s[500];
        //int total;
        char *current;

        if(argc != 3)
        {
                fprintf(stderr, "Invalid Arugument!\n");
                exit(1);
        }

        srandom(time(0));


        philosopher_count=atoi(argv[1]);
        threads=(pthread_t*)malloc(philosopher_count*sizeof(pthread_t));
        if(threads==NULL)
        {
                perror("malloc");
                exit(1);
        }

        ps=(philosopher_structure*)malloc(philosopher_count*sizeof(philosopher_structure));
        if(ps==NULL)
        {
                perror("malloc");
                exit(1);
        }

        v = initialize_v(philosopher_count);
        pp=(philosopher*)v;
        total_blocking_time=(long int*)malloc(philosopher_count*sizeof(long int));
        if(total_blocking_time==NULL)
        {
                perror("malloc total_blocking_time not defined");
                exit(1);
        }

        block_starting_time=(long int*)malloc(philosopher_count*sizeof(long int));
        if(block_starting_time==NULL)
        {
                perror("malloc block_starting_time not defined");
                exit(1);
        }

        blocktime_monitor=(pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
        pthread_mutex_init(blocktime_monitor,NULL);
        for(i=0;i<philosopher_count;i++)
        {
                total_blocking_time[i]=0;
                block_starting_time[i]=-1;
        }

        //initialize philosophers
        for (i=0;i<philosopher_count;i++)
        {
                ps[i].id = i;
                ps[i].v = v;
                ps[i].maximum_sleeping_time = maximum_sleeping_time;
                ps[i].total_blocking_time=total_blocking_time;
                ps[i].block_starting_time=block_starting_time;
                ps[i].blocktime_monitor=blocktime_monitor;
                ps[i].philosopher_count=philosopher_count;
                pthread_create(threads+i,NULL,philosopher_create,(void *)(ps+i));
        }

        //calculating blocktime
        while(1)
        {
            pthread_mutex_lock(blocktime_monitor);
            gettimeofday(&timev,NULL);
            t_temp1 = timev.tv_sec*(int)1e6+timev.tv_usec;
                printf("Starvation Count :  ");

            for(i=0; i < philosopher_count; i++)
            {
              t_temp2 = total_blocking_time[i];
              if (block_starting_time[i]!=-1 && t_temp1-block_starting_time[i]>=16*maximum_sleeping_time)
                 pp->blocking_time_count[i]++;
          printf("(%d)",pp->blocking_time_count[i]);
            }

            pthread_mutex_unlock(blocktime_monitor);
            printf("\n");
            sleep(10);
        }
  return 0;
}
