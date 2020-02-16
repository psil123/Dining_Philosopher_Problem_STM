#include<stdio.h>
#include<stdlib.h>
#include<string.h>
//#include<math.h>
#define _GNU_SOURCE
#define THINKING 0
#define HUNGRY 1
#define EATING 2
//stores control info for each philosopher
typedef struct
{
        int id;
        long program_starting_time,maximum_sleeping_time;
        int *total_blocking_time,*block_starting_time;
        void *v; //stores philosopher var to be passed to each process
        int philosopher_count;
        pthread_mutex_t *blocktime_monitor;
} philosopher_structure;

//stores all control info for all philosophers
typedef struct
{
        int *state;
        int philosopher_count;
        pthread_mutex_t *monitor;//general mutex for CS
        pthread_cond_t **checker;//mutex for each fork 
} philosopher;


void take_fork(philosopher_structure *ps)
{
        philosopher *pp;
        int philosopher_count;

        pp=(philosopher*)ps->v;
        philosopher_count=pp->philosopher_count;

        pthread_mutex_lock(pp->monitor);
        pp->state[ps->id]=HUNGRY;
        while (pp->state[(ps->id+(philosopher_count-1))%philosopher_count]==EATING || pp->state[(ps->id + 1)%philosopher_count]==EATING)
        {
                pthread_cond_wait(pp->checker[ps->id],pp->monitor);
        }
        pp->state[ps->id]=EATING;
        pthread_mutex_unlock(pp->monitor);
}

void put_fork(philosopher_structure *ps)
{
        philosopher *pp;
        int philosopher_count;

        pp=(philosopher*)ps->v;
        philosopher_count=pp->philosopher_count;

        pthread_mutex_lock(pp->monitor);
        pp->state[ps->id]=THINKING;
        if(pp->state[(ps->id+(philosopher_count-1))%philosopher_count]==HUNGRY)
        {
                pthread_cond_signal(pp->checker[(ps->id+(philosopher_count-1))%philosopher_count]);
        }
        if(pp->state[(ps->id+1)%philosopher_count]==HUNGRY)
        {
                pthread_cond_signal(pp->checker[(ps->id+1)%philosopher_count]);
        }
        pthread_mutex_unlock(pp->monitor);
}

void *initialize_v(int philosopher_count)
{
        philosopher *pp;
        int i;

        pp = (philosopher*) malloc(sizeof(philosopher));
        pp->philosopher_count=philosopher_count;
        pp->monitor=(pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
        pp->checker=(pthread_cond_t**)malloc(philosopher_count*sizeof(pthread_cond_t
*));
        if(pp->checker==NULL)
        {
                perror("malloc");
                exit(1);
        }

        pp->state=(int*)malloc(philosopher_count*sizeof(int));
        if(pp->state==NULL)
        {
                perror("malloc");
                exit(1);
        }

        //initialize mutex
        pthread_mutex_init(pp->monitor, NULL);

        //initialize conditional variables
        for(i=0;i<philosopher_count;i++)
        {
                pp->checker[i]=(pthread_cond_t*)malloc(sizeof(pthread_cond_t));
                if(pp->checker[i]==NULL)
                {
                        perror("malloc");
                        exit(1);
                }
                pthread_cond_init(pp->checker[i], NULL);
                pp->state[i] = THINKING;
        }
        return (void*)pp;
}

void *philosopher_main(void *v)
{
        philosopher_structure *ps;
        long st,t;

        ps=(philosopher_structure*)v;

        while(1)
        {
                st = (random()%ps->maximum_sleeping_time) + 1;
                sleep(st);

                t = time(0);
                pthread_mutex_lock(ps->blocktime_monitor);
                ps->block_starting_time[ps->id] = t;//block time not added yet
                pthread_mutex_unlock(ps->blocktime_monitor);

                take_fork(ps);

                //calculate block time
                pthread_mutex_lock(ps->blocktime_monitor);
                ps->total_blocking_time[ps->id] += (time(0) - t);
                ps->block_starting_time[ps->id] = -1; // indicates that blocktime has been added
                pthread_mutex_unlock(ps->blocktime_monitor);

                st = (random()%ps->maximum_sleeping_time) + 1;
                sleep(st);

                put_fork(ps);
        }
}

int main(int argc,char **argv)
{
        pthread_t *threads;//store all threads
        philosopher_structure *ps;
        int i,maximum_sleeping_time=atoi(argv[2]);
        void *v;
        long program_starting_time,t_temp1,t_temp2;
        int *total_blocking_time,*block_starting_time,*blocking_time_count;
        int philosopher_count;
        pthread_mutex_t *blocktime_monitor;//mutex to block all process while calc and printing block_time
        char s[500];
        int total;
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
        program_starting_time=time(0);
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

        blocking_time_count=(int*)malloc(philosopher_count*sizeof(int));
        if(blocking_time_count==NULL)
        {
                perror("malloc total_blocking_time not defined");
                exit(1);
        }

        //initialize philosophers
        for (i=0;i<philosopher_count;i++)
        {
                ps[i].id = i;
                ps[i].program_starting_time=program_starting_time;
                ps[i].v = v;
                ps[i].maximum_sleeping_time = maximum_sleeping_time;
                ps[i].total_blocking_time=total_blocking_time;
                ps[i].block_starting_time=block_starting_time;
                ps[i].blocktime_monitor=blocktime_monitor;
                ps[i].philosopher_count=philosopher_count;
                pthread_create(threads+i,NULL,philosopher_main,(void *)(ps+i));
        }

        //calculating blocktime
        while(1)
        {
            pthread_mutex_lock(blocktime_monitor);
            t_temp1 = time(0);
            current = s;
            total = 0;
            for(i=0; i < philosopher_count; i++)
            {
              total += total_blocking_time[i];
              if (block_starting_time[i] != -1)
                        total += (t_temp1 - block_starting_time[i]);
            }
            sprintf(current,"Starvation Count :  ");

            current = s + strlen(s);

            for(i=0; i < philosopher_count; i++)
            {
              t_temp2 = total_blocking_time[i];
              if (block_starting_time[i] != -1)
                        t_temp2 += (t_temp1 - block_starting_time[i]);
	      if(t_temp2>=16*maximum_sleeping_time)
		blocking_time_count[i]++;
              sprintf(current, "%5d ", blocking_time_count[i]);
              current = s + strlen(s);
            }
            pthread_mutex_unlock(blocktime_monitor);
            printf("%s\n", s);
            fflush(stdout);
            sleep(10);
        }
  return 0;
}