#include<time.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include<stdlib.h>
#include <unistd.h>
#define THINKING 2
#define HUNGRY 1
#define EATING 0
int *state,N,*starve;
sem_t mutex;
sem_t **S;
void test(int phnum)
{
        if (state[phnum]==HUNGRY && state[(phnum+N-1)%N]!=EATING && state[(phnum+1)%N]!=EATING)
        {
                state[phnum] = EATING;
                sleep(2);
                //printf("Philosopher %d takes fork %d and %d\n", phnum+1,(phnum+N-1)%N+1,phnum+1);
                //printf("Philosopher %d is Eating\n", phnum+1);
                sem_post(S[phnum]);
        }
}
void take_fork(int phnum)
{
        sem_wait(&mutex);
        state[phnum]=HUNGRY;
        //printf("Philosopher %d is Hungry\n",phnum+1);
        test(phnum);
        sem_post(&mutex);
        //sem_wait(S[phnum]);
        if(sem_trywait(S[phnum])!=0)
        {
                //printf("Philosopher %d has rejected %d\n",phnum+1,sem_wait(S[phnum]));
                sem_wait(S[phnum]);
                starve[phnum]++;
        }
        sleep(1);
}
void put_fork(int phnum)
{
        sem_wait(&mutex);
        state[phnum]=THINKING;
        //printf("Philosopher %d putting fork %d and %d down\n",phnum+1,(phnum+N-1)%N+1,phnum+1);
        //printf("Philosopher %d is thinking\n",phnum+1);
        test((phnum+N-1)%N);
        test((phnum+1)%N);
        sem_post(&mutex);
}
void* philospher(void* num)
{
        //struct timeval start,end;
        int i=*((int*)num);
        int j;
        for(j=0;j<30;j++)
        {
                //gettimeofday(&start,NULL);
                //sleep(1);
                take_fork(i);
                sleep(4);
                put_fork(i);
                //gettimeofday(&end,NULL);
        }
        //double t=(double)((end.tv_sec-start.tv_sec)/1000000.0);
        //printf("time : %lf",t);
}
int main()
{
        int i,k;
        printf(" Enter number of philosophers : ");
        scanf("%d",&N);
        if(N==0)
                exit(1);
        pthread_t thread_id[N];
        int phil[N];
        sem_init(&mutex,0,1);
        starve=(int*)malloc(N*sizeof(int));
        S=(sem_t**)malloc(N*sizeof(sem_t*));
        state=(int*)calloc(N,sizeof(int));
        for (i=0;i<N;i++)
        {
                S[i]=(sem_t*)malloc(sizeof(sem_t));
                sem_init(S[i],0,0);
        }
        for (i=0;i<N;i++)
        {
                phil[i]=i;
                pthread_create(&thread_id[i], NULL,philospher,&phil[i]);
                printf("Philosopher %d is thinking\n",i+1);
        }
        for (i=0;i<N;i++)
                pthread_join(thread_id[i], NULL);
        //for(i=0;i<N;i++)
        //      printf("Philosopher %d ret -------\n",pthread_cancel(thread_id[i]));
        for(i=0;i<N;i++)
                printf("Philosopher %d starved %d\n",i+1,starve[i]);

        return 0;
}
