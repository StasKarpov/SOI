/* version where reders can read all items in a row
/* they dont wait for consumer */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <semaphore.h>
#include <pthread.h>

#define BUFFER_SIZE 5
#define NITERS 2000
#define NREADERS 3

#define RED "\x1B[31m"
#define GREEN "\x1B[32m"
#define YELLOW "\x1B[33m"
#define RESET "\x1B[0m"
#define READER_SLEEP 2
#define CONSUMER_SLEEP 5
#define PRODUCER_SLEEP 1
typedef struct
{
    int buf[BUFFER_SIZE];
    int in;//index of first empty slot
    int out;//index of last slot
    int rindex[NREADERS];//where reders are
    int nreads[BUFFER_SIZE];//how much each element have been read
    sem_t empty;//free places
    sem_t full;//non-free places
    sem_t mutex;//critical section
    sem_t readers[NREADERS];
    sem_t cons; // how much elements have readen
    //sem_t consreader; // communication between consumer and reader

}Buffer;
Buffer shared;

void print_buffer()
{
   int i;
   for(i = 0;i<BUFFER_SIZE;i++)
   {
     if(shared.buf[i] == 0) printf("_");
     else if (shared.nreads[i]==0) printf("%d",shared.buf[i]);
     else if (shared.nreads[i]==1) printf(GREEN "%d",shared.buf[i]);
     else if (shared.nreads[i]==2) printf(YELLOW "%d",shared.buf[i]);
     else if (shared.nreads[i]==3) printf(RED "%d",shared.buf[i]);

    printf(RESET);
   }
   printf("\n");
 }

 void print_semaphors()
 {
   int e,f,r1,r2,r3,c;
   sem_getvalue(&shared.empty,&e);
   sem_getvalue(&shared.full,&f);
   sem_getvalue(&shared.readers[0],&r1);
   sem_getvalue(&shared.readers[1],&r2);
   sem_getvalue(&shared.readers[2],&r3);
   sem_getvalue(&shared.cons,&c);
   printf("E|%d  F|%d  R1|%d  R2|%d  R3|%d  C|%d  ",e,f,r1,r2,r3,c);
 }


void random_sleep(int i)
{
     int x;
     x = (rand()%i)+1;
     sleep(x);
}



void *Producer()
{

    int item,i;
    for(i = 0 ; i<NITERS ; i++ )
    {
       item = (rand() % 9) + 1;

       //printf("p");
       sem_wait(&shared.empty); // dec empty
       sem_wait(&shared.mutex); // dec critical section

       shared.buf[shared.in] = item;
       shared.in = (shared.in+1)%BUFFER_SIZE;//inc in index


       sem_post(&shared.full);
       sem_post(&shared.readers[0]);
       sem_post(&shared.readers[1]);
       sem_post(&shared.readers[2]);

       print_semaphors();
       printf("[P] producing %d   ", item);
       print_buffer();

       sem_post(&shared.mutex);


       random_sleep(PRODUCER_SLEEP);
     }
}

void *Consumer()
{

     int item,i,j,error,prev;
     for(i = 0 ; i<NITERS ; i++ )
     {
       //printf("c");
       sem_wait(&shared.cons); // wait for readers will read
       sem_wait(&shared.full);//wait for producer
       sem_wait(&shared.mutex);
       print_semaphors();



       item = shared.buf[shared.out]; // get object
       shared.buf[shared.out] = 0; //erase object

       if(shared.nreads[shared.out] < NREADERS ||
          shared.nreads[shared.out] == 4 || // for special read (when double read occurs)
          shared.nreads[shared.out] == 5 ||
          shared.nreads[shared.out] == 6 )
       {//if two readers dont need to read this yet means that one read this
         //so we need to run up readers
          for(j = 0; j<NREADERS ; j++)
          {//search for this reader
            if(shared.rindex[j] == shared.out)
            {
               shared.rindex[j]=(shared.rindex[j]+1)%BUFFER_SIZE;
            }
          }
       }

       printf("[C] consume %d     ", item);
       print_buffer();
       shared.nreads[shared.out] = 0;
       sem_post(&shared.mutex);
       sem_post(&shared.empty);
       prev = shared.nreads[shared.out];
       shared.out = (shared.out+1)%BUFFER_SIZE; // inc index of new slot

       if(prev == 1 || shared.buf[shared.out] == 0)
       {//consumer sleeps only if element that it consume have been readen once
       random_sleep(CONSUMER_SLEEP);
       }else
        {//double read
         if(prev == 2 || prev == 3 || prev == 6 || prev == 7)
         {
           if(shared.nreads[shared.out] == 0)
           {
           shared.nreads[shared.out] = 4 ; //special nreads
           sem_post(&shared.cons);
           }
         }
       }
     }//for
     printf("Consumer finished!\n");
}


void *Reader(void *arg)
{

  int i,index,check;
  index = (int)arg; // reader index

  for(i = 0 ; i<NITERS ; i++ )
  {
    //printf("r");
    sem_wait(&shared.readers[index]); // wait until it can read
    sem_wait(&shared.mutex);

    check = 1;// to limit reader
    if(shared.in >= shared.out){
        if( shared.rindex[index] >= shared.in || shared.rindex[index] < shared.out ){check = 0;}
        //else if(shared.rindex[index] < shared.out){check = 0;}
    }else {
        if(shared.rindex[index] >= shared.in && shared.rindex[index] < shared.out){check = 0;}
    }
    if(shared.nreads[shared.rindex[index]]>=2) check = 0;//reader cant read more then two times

    if(check == 1)
    {
        shared.nreads[shared.rindex[index]]++;

        if(shared.nreads[shared.rindex[index]] == 1)
           sem_post(&shared.cons);//cons can cons item

        print_semaphors();
        printf("[R%d] reading %d    ", index+1,shared.buf[shared.rindex[index]]);
        print_buffer();

        shared.rindex[index]=(shared.rindex[index]+1)%BUFFER_SIZE;
    }

    sem_post(&shared.mutex);
    if(check == 1)
       random_sleep(READER_SLEEP);
  }
    printf("Reader finished!\n");
}


int main()
{
    pthread_t idP, idC, idR1,idR2,idR3;

    sem_init(&shared.full, 0, 0);
    sem_init(&shared.empty, 0, BUFFER_SIZE);
    sem_init(&shared.mutex, 0, 1);
    sem_init(&shared.readers[0],0 , 0);
    sem_init(&shared.readers[1],0 , 0);
    sem_init(&shared.readers[2],0 , 0);
    sem_init(&shared.cons,0,0);

    pthread_create(&idP, NULL, Producer, NULL);
    pthread_create(&idC, NULL, Consumer, NULL);
    pthread_create(&idR1, NULL, Reader, (void*)0);
    pthread_create(&idR2, NULL, Reader, (void*)1);
    pthread_create(&idR3, NULL, Reader, (void*)2);


    pthread_exit(NULL);
    return 0;
}
