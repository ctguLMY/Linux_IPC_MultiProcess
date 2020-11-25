/*
 * @Description: linux IPC,  multiple process, shared memory, 3 producer and 1 customer
 * @Version: 1.0
 * @Autor: Ming
 * @Date: 2020-11-23 09:55:09
 * @LastEditors: Ming
 * @LastEditTime: 2020-11-24 23:43:27
 */
#include <iostream>
using namespace std;

extern "C" {
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/ipc.h>
#include <sys/shm.h>

#include <string.h>
}

#define SHM_KEY 25
#define SEM_KEY_1 1234
#define SEM_KEY_2 1235
#define BUF_SIZE 2048

union semnum
{
    int     val;
    struct semid_ds *buf;
    unsigned short  *array;
};

struct msg_data
{
    char data[BUF_SIZE];
    int process_id;
};

/**
 * @description: create a semaphore and set its value == val
 * @param {key, val}
 * @return {semid}
 */
int sem_create(key_t key, int val)  
{
    int semid;
    semid = semget(key, 1, 0666 | IPC_CREAT);
    if(semid == -1)
    {
        perror("semget");
        exit(-1);
    }
    union semnum arg; 
    arg.val = val;       //set semaphore initial value
    if(semctl(semid, 0, SETVAL, arg) == -1)
    {
        perror("semctl");
        exit(-1);
    }
    return semid;
}

/**
 * @description: delete a semaphore
 * @param {semid}
 * @return {*}
 */
void sem_del(int semid) 
{
    union semnum arg;
    arg.val = 0;
    if(semctl(semid, 0, IPC_RMID, arg) == -1)
    {
        perror("semctl");
        exit(-1);
    }
}

/**
 * @description: make the semaphore value -1;
 * @param {int semid}
 * @return {*}
 */
int P(int semid)
{
    //P操作，使信号量的值减1
    struct sembuf sops = {0, -1, SEM_UNDO}; 
    return (semop(semid, &sops, 1));
}

/**
 * @description: make the semaphore value +1;
 * @param {int semid}
 * @return {*}
 */
int V(int semid)  //V操作，使信号量的值加一
{
    struct sembuf sops = {0, +1, SEM_UNDO};
    return (semop(semid, &sops, 1));
}

/**
 * @description: main_process create semaphores and shared memory.
 * @param {empty, full, shmid}
 * @return {*}
 */
void main_create_sem_shm(int *empty, int *full, int *shmid){
	
    *empty = sem_create(SEM_KEY_1, 1); //set semaphore empty initial value = 1;
    *full = sem_create(SEM_KEY_2, 0); //set semaphore full initial value = 0;
    //create shared memory
    *shmid = shmget(SHM_KEY, sizeof(struct msg_data), 0666 | IPC_CREAT);
    if(*shmid == -1)
    {
        perror("shmget");
        exit(-1);
    }

    printf("empty=%d\tfull=%d\tshmid=%d\n", *empty, *full, *shmid);
}
/**
 * @description: child process get semaphores and shared memory
 * @param {empty, full, shmid}
 * @return {*}
 */
void process_get_sem_shm(int *empty, int *full, int *shmid){
	 
        *empty = semget(SEM_KEY_1, 1, 0); //get semaphore 
        *full = semget(SEM_KEY_2, 1, 0);
        *shmid = shmget(SHM_KEY, 1024, 0);  //get shared memory
        if(*empty == -1 || *full == -1 || *shmid == -1)
        {
            perror("get");
            exit(-1);
        }	
}

/**
 * @description: if share memory is empty, producer write data.
 * @param {msg, pid, i}
 * @return {*}
 */
void Productor(struct msg_data *msg, int pid, int i) 
{

    sprintf(msg->data, "producer write, data= %d", i);
    msg->process_id = pid;

    printf("Productor write:%s\n", msg->data);
}

/**
 * @description:if producer write, customer get data from shared memory.
 * @param {msg_data}
 * @return {*}
 */
void Customer(struct msg_data *msg)     //消费者
{
    printf("Customer get:%s from %d \n", msg->data, msg->process_id);
}

int main()
{
    pid_t p1, p2, p3, p4;
    
    //create shared memory
    int empty, full, shmid;
    main_create_sem_shm(&empty, &full, &shmid);
  
    //create 3 proces as producer
    if((p1 = fork()) == 0)
    {
        //process 1
        printf("create pid = %d, ppid = %d\n", getpid(), getppid());

        int empty, full, shmid;
        process_get_sem_shm(&empty, &full, &shmid);

        struct msg_data *buf;
        void *tmp = shmat(shmid, (void *)0, 0); //process map the Shared memory
        printf("\n%d:Memory attached at %X\n", getpid(), tmp);
        buf = (struct msg_data *)tmp;
        int i = 0;

        while(1)
        {
            cout << "p1 tread:" << getpid() << endl;
			
            P(empty);
            Productor(buf, getpid(),i++);
            V(full);
			
            sleep(1);
        }

        return 0;
    }
    else if((p2 = fork()) == 0)
    {
        //process 2
        printf("create pid = %d, ppid = %d\n", getpid(), getppid());

        int empty, full, shmid;
        process_get_sem_shm(&empty, &full, &shmid);

        struct msg_data *buf;
        void *tmp = shmat(shmid, NULL, 0); //process map the Shared memory
        buf = (struct msg_data *)tmp;
        int i = 0;
        printf("\n%d:Memory attached at %X\n", getpid(), tmp);
        while(1)
        {
            cout << "p2 tread:" << getpid() << endl;
            P(empty);
            Productor(buf, getpid(),i++);
            V(full);
            sleep(1);
        }

        return 0;
    }
    else if((p3 = fork()) == 0)
    {
        //process 3
        printf("create pid = %d, ppid = %d\n", getpid(), getppid());

        int empty, full, shmid;
        process_get_sem_shm(&empty, &full, &shmid);

        struct msg_data *buf;
        void *tmp = shmat(shmid, NULL, 0); //process map the Shared memory
        buf = (struct msg_data *)tmp;

        printf("\n%d:Memory attached at %X\n", getpid(), tmp);
        while(1)
        {
            cout << "p3 tread:" << getpid() << endl;
            sleep(1);

            P(empty);
            Productor(buf, getpid(),1024);
            V(full);
        }
        return 0;
    }
    else if((p4 = fork()) == 0)
    {
        //process customer
        cout << "create customer thread" << endl;
        int empty, full, shmid;
        process_get_sem_shm(&empty, &full, &shmid);

        struct msg_data *buf;
        buf = (struct msg_data *)shmat(shmid, NULL, 0); //process map the Shared memory
        printf("\n%d:Memory attached at %X\n", getpid(), buf);

        while(1)
        {
            cout << "customer tread:" << getpid() << endl;

            P(full);
            Customer(buf);           
            V(empty);

            sleep(3);
        }
        return 0;
    }


    while(1)
    {
        cout << "main tread:" << getpid() << endl;
        sleep(1);
    }
    return 0;
}

