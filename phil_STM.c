#include <assert.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

#ifdef DEBUG
# define IO_FLUSH                       fflush(NULL)
/* Note: stdio is thread-safe */
#endif

#define RO                              1
#define RW                              0

#if defined(TM_GCC)
# include "../../abi/gcc/tm_macros.h"
#elif defined(TM_DTMC)
# include "../../abi/dtmc/tm_macros.h"
#elif defined(TM_INTEL)
# include "../../abi/intel/tm_macros.h"
#elif defined(TM_ABI)
# include "../../abi/tm_macros.h"
#endif /* defined(TM_ABI) */

#if defined(TM_GCC) || defined(TM_DTMC) || defined(TM_INTEL) || defined(TM_ABI)
# define TM_COMPILER
/* Add some attributes to library function */
TM_PURE
void exit(int status);
TM_PURE
void perror(const char *s);
#else /* Compile with explicit calls to tinySTM */

#include "stm.h"
#include "mod_ab.h"

/*
 *  * Useful macros to work with transactions. Note that, to use nested
 *   * transactions, one should check the environment returned by
 *    * stm_get_env() and only call sigsetjmp() if it is not null.
 *     */

#define TM_START(tid, ro)               { stm_tx_attr_t _a = {{.id = tid, .read_only = ro}}; sigjmp_buf *_e = stm_start(_a); if (_e != NULL) sigsetjmp(*_e, 0)
#define TM_LOAD(addr)                   stm_load((stm_word_t *)addr)
#define TM_STORE(addr, value)           stm_store((stm_word_t *)addr, (stm_word_t)value)
#define TM_COMMIT                       stm_commit(); }

#define TM_INIT                         stm_init(); mod_ab_init(0, NULL)
#define TM_EXIT                         stm_exit()
#define TM_INIT_THREAD                  stm_init_thread()
#define TM_EXIT_THREAD                  stm_exit_thread()

#endif /* Compile with explicit calls to tinySTM */

#define DEFAULT_DURATION                10000
#define DEFAULT_NB_ACCOUNTS             1024
#define DEFAULT_NB_THREADS              1
#define DEFAULT_READ_ALL                20
#define DEFAULT_SEED                    0
#define DEFAULT_WRITE_ALL               0
#define DEFAULT_READ_THREADS            0
#define DEFAULT_WRITE_THREADS           0
#define DEFAULT_DISJOINT                0

#define XSTR(s)                         STR(s)
#define STR(s)                          #s


#define THINKING 0
#define HUNGRY 1
#define EATING 2


typedef struct
{
int id,flag;
long program_starting_time,maximum_sleeping_time;
int *total_blocking_time,*block_starting_time;
void *v;
int philosopher_count;
//pthread_mutex_t *blocktime_monitor;
int* blocktime_flag;
} philosopher_structure;

typedef struct {
  //pthread_mutex_t *monitor;
  //pthread_cond_t **checker;
  int *state;
  int* flag;
  int *tblock;
 int *blocking_time_count;
  int philosopher_count;
} philosopher;

/*
int can_I_eat(philosopher *pp, int n, int ms)
{
  int t;
  int philosopher_count;

  struct timeval tv;
  philosopher_count = pp->philosopher_count;

  if (pp->state[(n+(philosopher_count-1))%philosopher_count] == EATING ||
      pp->state[(n+1)%philosopher_count] == EATING) return 0;
  gettimeofday(&tv,NULL);
  t=time(0);
  if (pp->state[(n+(philosopher_count-1))%philosopher_count] == HUNGRY &&
      pp->tblock[(n+(philosopher_count-1))%philosopher_count] < pp->tblock[n] &&
      (t - pp->tblock[(n+(philosopher_count-1))%philosopher_count]) >= ms*16) return 0;
  if (pp->state[(n+1)%philosopher_count] == HUNGRY &&
      pp->tblock[(n+1)%philosopher_count] < pp->tblock[n] &&
      (t - pp->tblock[(n+1)%philosopher_count]) >= ms*16) return 0;
  return 1;
}*/
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
  int* state_temp=(int*)calloc(pp->philosopher_count,sizeof(int));
  int* flag_temp=(int*)calloc(pp->philosopher_count,sizeof(int));
  int* tblock_temp=(int*)calloc(pp->philosopher_count,sizeof(int));
  TM_START(0,RW);
  state_temp=(int*)TM_LOAD(&pp->state);
  flag_temp=(int*)TM_LOAD(&pp->flag);
  tblock_temp=(int*)TM_LOAD(&pp->tblock);
  //pthread_mutex_lock(pp->monitor);
  state_temp[ps->id] = HUNGRY;
//  tblock_temp[ps->id] = time(0);
gettimeofday(&tv,NULL);
  tblock_temp[ps->id] = tv.tv_sec*(int)1e6+tv.tv_usec;
  while (pp->flag[ps->id]==1 || !can_I_eat(pp, ps->id, ps->maximum_sleeping_time)) {
    //pthread_cond_wait(pp->checker[ps->id], pp->monitor);
    pp->flag[ps->id]=1;
  }
  state_temp[ps->id] = EATING;
  //pthread_mutex_unlock(pp->monitor);
  TM_STORE(&pp->state,state_temp);
  TM_STORE(&pp->flag,flag_temp);
  TM_STORE(&pp->tblock,tblock_temp);
  TM_COMMIT;
}

void put_fork(philosopher_structure *ps)
{
  stm_init_thread();
  philosopher *pp;
  int philosopher_count;
  TM_START(0,RW);
  pp = (philosopher *) ps->v;
  int* state_temp=(int*)calloc(pp->philosopher_count,sizeof(int));
  int* flag_temp=(int*)calloc(pp->philosopher_count,sizeof(int));
  philosopher_count = pp->philosopher_count;

  //pthread_mutex_lock(pp->monitor);

  state_temp=(int*)TM_LOAD(&pp->state);
  flag_temp=(int*)TM_LOAD(&pp->flag);
  state_temp[ps->id] = THINKING;
  if (state_temp[(ps->id+(philosopher_count-1))%philosopher_count] == HUNGRY) {
    //pthread_cond_signal(pp->checker[(ps->id+(philosopher_count-1))%philosopher_count]);
    flag_temp[(ps->id+(philosopher_count-1))%philosopher_count]=0;
  }
  if (state_temp[(ps->id+1)%philosopher_count] == HUNGRY) {
    flag_temp[(ps->id+1)%philosopher_count]=0;
  }
  //pthread_mutex_unlock(pp->monitor);

  TM_STORE(&pp->state,state_temp);
  TM_STORE(&pp->flag,flag_temp);
  TM_COMMIT;
  stm_init_thread();
}

void *initialize_v(int philosopher_count)
{
  philosopher *pp;
  int i;
  pp = (philosopher *) malloc(sizeof(philosopher));
  pp->flag=(int*)calloc(philosopher_count,sizeof(int));
  pp->philosopher_count = philosopher_count;
  //pp->monitor = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
  //pp->checker = (pthread_cond_t **) malloc(sizeof(pthread_cond_t *)*philosopher_count);
  //if (pp->checker == NULL) { perror("malloc"); exit(1); }

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
  //pthread_mutex_init(pp->monitor, NULL);
  for (i = 0; i < philosopher_count; i++) {
    //pp->checker[i] = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
    //pthread_cond_init(pp->checker[i], NULL);
    pp->state[i] = THINKING;
    pp->tblock[i] = 0;
  }

  return (void *) pp;
}





void *philosopher_create(void *v)
{
        stm_init_thread();
          struct timeval tv1,tv2;
        philosopher_structure *ps;
        long int st,t;

        ps=(philosopher_structure*)v;
        philosopher* pp=(philosopher*)ps->v;
        TM_START(0,RW);
        int* block_starting_time_temp=(int*)calloc(ps->philosopher_count,sizeof(int));
        int* blocking_time_count_temp=(int*)calloc(ps->philosopher_count,sizeof(int));
        while(1)
        {
                gettimeofday(&tv1,NULL);
                block_starting_time_temp=(int*)TM_LOAD(&ps->block_starting_time);
                while(*ps->blocktime_flag==1);
                *ps->blocktime_flag=1;
                //pthread_mutex_lock(ps->blocktime_monitor);
                block_starting_time_temp[ps->id] = tv1.tv_sec*(int)1e6+tv1.tv_usec;//block time not added yet
                *ps->blocktime_flag=0;
                //pthread_mutex_unlock(ps->blocktime_monitor);
                TM_STORE(&ps->block_starting_time,block_starting_time_temp);
                //TM_COMMIT;
                take_fork(ps);

                //calculate block time
                blocking_time_count_temp=(int*)TM_LOAD(&pp->blocking_time_count);
                //block_starting_time_temp=(int*)TM_LOAD(&ps->block_starting_time);
                while(*ps->blocktime_flag==1);
                *ps->blocktime_flag=1;
                //pthread_mutex_lock(ps->blocktime_monitor);
                gettimeofday(&tv2,NULL);
                if((tv2.tv_sec-tv1.tv_sec)*(int)1e6+(tv2.tv_usec-tv1.tv_usec)>=16*ps->maximum_sleeping_time)
                        blocking_time_count_temp[ps->id]++;
                block_starting_time_temp[ps->id] = -1; // indicates that blocktime has been added
                *ps->blocktime_flag=0;
               //pthread_mutex_unlock(ps->blocktime_monitor);
                TM_STORE(&pp->blocking_time_count,blocking_time_count_temp);
                TM_STORE(&ps->block_starting_time,block_starting_time_temp);
                TM_COMMIT;
            usleep(ps->maximum_sleeping_time);

                put_fork(ps);
        }
        stm_exit_thread();
}

void func(int argc,char** argv)
{
        stm_init_thread();
    struct timeval timev;
        pthread_t *threads;//store all threads
        philosopher_structure *ps;
        philosopher* pp;
        int i,maximum_sleeping_time=atoi(argv[2]);
        void *v;
        long int t_temp1,t_temp2;
        int *total_blocking_time,*block_starting_time;
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
        total_blocking_time=(int*)malloc(philosopher_count*sizeof(int));
        if(total_blocking_time==NULL)
        {
                perror("malloc total_blocking_time not defined");
                exit(1);
        }

        block_starting_time=(int*)malloc(philosopher_count*sizeof(int));
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
        int* blocktime_flag=(int*)malloc(sizeof(int));
        *blocktime_flag=0;
        //initialize philosophers
        for (i=0;i<philosopher_count;i++)
        {
                ps[i].id = i;
                ps[i].v = v;
                ps[i].maximum_sleeping_time = maximum_sleeping_time;
                ps[i].total_blocking_time=total_blocking_time;
                ps[i].block_starting_time=block_starting_time;
               // ps[i].blocktime_monitor=blocktime_monitor;
                ps[i].blocktime_flag=blocktime_flag;
                ps[i].philosopher_count=philosopher_count;
                pthread_create(threads+i,NULL,philosopher_create,(void *)(ps+i));
        }

        //calculating blocktime

//dboutful
        int* blocking_time_count_temp=(int*)malloc(philosopher_count*sizeof(int));
        for(i=0;i<philosopher_count;i++)
                blocking_time_count_temp[i]=0;
        while(1)
        {
        TM_START(0,RW);
            blocking_time_count_temp=(int*)TM_LOAD(&pp->blocking_time_count);
            //pthread_mutex_lock(blocktime_monitor);
            while(*blocktime_flag==1);
            *blocktime_flag=1;
            gettimeofday(&timev,NULL);
            t_temp1 = timev.tv_sec*(int)1e6+timev.tv_usec;
                printf("Starvation Count :  ");

            for(i=0; i < philosopher_count; i++)
            {
              t_temp2 = total_blocking_time[i];
              if (block_starting_time[i]!=-1 && t_temp1-block_starting_time[i]>=16*maximum_sleeping_time)
                 blocking_time_count_temp[i]++;
          printf("(%d)",blocking_time_count_temp[i]);
            }

            //pthread_mutex_unlock(blocktime_monitor);
            *blocktime_flag=0;
            printf("\n");
            sleep(10);
            TM_STORE(&pp->blocking_time_count,blocking_time_count_temp);
            TM_COMMIT;
        }

        stm_exit_thread();

}
int main(int argc,char **argv)
{
        stm_init();
        func(argc,argv);
                stm_exit();
  return 0;
}
