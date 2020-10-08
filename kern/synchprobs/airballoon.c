/*
 * Driver code for airballoon problem
 */
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <test.h>
#include <synch.h>
#define N_LORD_FLOWERKILLER 8
#define NROPES 16
static int ropes_left = NROPES;
//Initialize flags which determine the order of threads
static int balloon_flag = 0;
static int dandelion_flag = 0;
static int marigold_flag = 0;
/* Data structures for rope mappings */

/* Implement this! */
struct stake{
  int srope; //rope to which stake is connected (rope moves not stakes)
  volatile bool is_staked;
};
struct hook{
  int hrope; //rope to which hook is connected (always same as hook index)
  volatile bool is_hooked;
};
struct rope{
  volatile bool is_severed;
};
/* Synchronization primitives */

/* Implement this! */
struct lock *lock1;
struct lock *lock2;
struct lock *lock3;
struct lock *lock4;
struct lock *lock5;
struct lock *lock6;
struct lock *lock7;
struct semaphore *sem1;

static void init_primitives()
{

sem1 = sem_create("sem1", 0);
lock1 = lock_create("lock1");

lock2 = lock_create("lock2");

lock3 = lock_create("lock3");
lock4 = lock_create("lock4");


lock5 = lock_create("lock5");
lock6 = lock_create("lock6");
lock7 = lock_create("lock7");
}

static void free_primitives()
{
lock_destroy(lock1);
lock_destroy(lock2);
lock_destroy(lock3);
lock_destroy(lock4);
lock_destroy(lock5);
lock_destroy(lock6);
lock_destroy(lock7);
sem_destroy(sem1);

}
/*static void decrement()
{lock_acquire(lock1);
 ropes_left--;
 lock_release(lock1);
}*/
/*
 * Describe your design and any invariants or locking protocols
 * that must be maintained. Explain the exit conditions. How
 * do all threads know when they are done?
 */
struct rope ropes[NROPES]; //where ropes[n] is the nth rope
struct stake stakes[NROPES]; //where stakes[n] is the nth stake
struct hook hooks[NROPES]; //where hooks[n] is the nth hook

static void initialize()
{
for (int i=0; i<NROPES; i++){
	  stakes[i].srope = i;
	  stakes[i].is_staked = true;
	}
for (int i=0; i<NROPES; i++){
	  hooks[i].hrope = i;
	  hooks[i].is_hooked = true;
	}
for (int i=0; i<NROPES; i++){
	  ropes[i].is_severed = false;
	}
}
static
void
dandelion(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;
    
	kprintf("Dandelion thread starting\n");

	/* Implement this function */
    lock_acquire(lock2);
    lock_acquire(lock7);
    while(true){
	  //int randomhook = random() % NROPES;
	  if(ropes_left == 0){
	    lock_release(lock2);
        lock_release(lock7);
	    dandelion_flag = 1;
	    break;
	  }
	  lock_release(lock2);
      lock_release(lock7);
      //thread_yield();
      int randomhook = random() % NROPES;
      lock_acquire(lock2);
      lock_acquire(lock7);
	  if (hooks[randomhook].is_hooked == true && ropes[hooks[randomhook].hrope].is_severed == false){// && ropes[hooks[randomhook].hrope].is_severed != true){
	    //unhook and severe rope
	    ropes[hooks[randomhook].hrope].is_severed = true;
	    hooks[randomhook].is_hooked = false;
        ropes_left--;
        lock_release(lock2);
        lock_release(lock7);
	    thread_yield();
        //lock_release(lock2);
	    kprintf("Dandelion severed rope %d from hook %d\n", randomhook, randomhook);
	    //thread_yield();
	  }
	  else{
	  lock_release(lock2);
      lock_release(lock7);}

	  }
	 
	 //thread_yield();	
	 kprintf("Dandelion thread done\n");

}

static
void
marigold(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	kprintf("Marigold thread starting\n");

	/* Implement this function */

	

	lock_acquire(lock3);
	lock_acquire(lock6);
	while(true){
	  //int randomstake = random() % NROPES;
	  if(ropes_left == 0){
	    lock_release(lock3);
        lock_release(lock6);
	    marigold_flag = 1;
	    break;
	  }
	  lock_release(lock3);
      lock_release(lock6);
      //thread_yield();
      int randomstake = random() % NROPES;
      lock_acquire(lock3);
	  lock_acquire(lock6);
	  if (stakes[randomstake].is_staked == true && ropes[stakes[randomstake].srope].is_severed == false){// && ropes[stakes[randomstake].srope].is_severed != true){
	    //unstake and severe rope
	    ropes[stakes[randomstake].srope].is_severed = true;
	    stakes[randomstake].is_staked = false;
        ropes_left--;
        //thread_yield();
        lock_release(lock3);
        lock_release(lock6);
        thread_yield();
	    kprintf("Marigold severed rope %d from stake %d\n", stakes[randomstake].srope, randomstake);
	    //thread_yield();
	    }else{
	  lock_release(lock3);
      lock_release(lock6);}
	  
	}
     //thread_yield();	
	 kprintf("Marigold thread done\n");

}

static
void
flowerkiller(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	kprintf("Lord FlowerKiller thread starting\n");

	/* Implement this function */
	
	while(true){
	  lock_acquire(lock1);
	  lock_acquire(lock4);
	  if(ropes_left < 2){
	    lock_release(lock1);
	    lock_release(lock4);
	    break;
	  }
	  int stake1;
	  int stake2;
      
	  stake1 = random() % NROPES; //maybe add a while condition
	  stake2 = random() % NROPES;
      lock_release(lock1);
	  lock_release(lock4);
	  thread_yield();
	  lock_acquire(lock1);
	  lock_acquire(lock4);
	  if(stake1 != stakes[stake1].srope && stake2 != stakes[stake2].srope){ //Only swap stakes not already swapped
	      lock_release(lock1);
	      lock_release(lock4);
	      thread_yield();
          continue;
       }
      lock_release(lock1);
	  lock_release(lock4);
	  thread_yield();
	 /* if (ropes[stake1].is_severed == false && ropes[stake2].is_severed == false){
      lock_acquire(lock1);
      lock_acquire(lock4);}
      else{
        lock_release(lock1);
	    lock_release(lock4);
	    continue;
      }*/
      lock_acquire(lock1);
      lock_acquire(lock4);
	  if(ropes[stake1].is_severed == false && ropes[stake2].is_severed == false ){// && stakes[stake1].is_staked && stakes[stake2].is_staked){
        if(ropes[stake1].is_severed == true && ropes[stake2].is_severed == true ){
        lock_release(lock1);
	    lock_release(lock4);
	    continue;
        }
        
        int rope1;
        rope1 = stakes[stake1].srope;
        stakes[stake1].srope = stakes[stake2].srope;
	    stakes[stake2].srope = rope1;
	    
	    lock_release(lock1);
	    lock_release(lock4);
	    //thread_yield();
	    kprintf("Lord FlowerKiller switched rope %d from stake %d to stake %d\n", stakes[stake1].srope, stake2, stake1); //stake1, stake2
	    kprintf("Lord FlowerKiller switched rope %d from stake %d to stake %d\n", stakes[stake2].srope, stake1, stake2); // stake2, stake1
	    thread_yield();
        break;
	    }else{
	    lock_release(lock1);
	    lock_release(lock4);
	    }
	   
	  
	}
	 	
	 	kprintf("Lord FlowerKiller thread done\n");

}

static
void
balloon(void *p, unsigned long arg)
{
	(void)p;
	(void)arg;

	kprintf("Balloon thread starting\n");

	/* Implement this function */
	
	while(true){
	   if(marigold_flag && dandelion_flag){
	     balloon_flag = 1;
	     thread_yield();
	     break;
	   }
	   //thread_yield();
	}
	thread_yield();
	kprintf("Balloon freed and Prince Dandelion escapes!\n");
	kprintf("Balloon thread done\n");


	
}


// Change this function as necessary
int
airballoon(int nargs, char **args)
{

	int err = 0, i;

	(void)nargs;
	(void)args;
	(void)ropes_left;
	
	initialize();
	init_primitives();

	err = thread_fork("Marigold Thread",
			  NULL, marigold, NULL, 0);
	if(err)
		goto panic;

	err = thread_fork("Dandelion Thread",
			  NULL, dandelion, NULL, 0);
	if(err)
		goto panic;

	for (i = 0; i < N_LORD_FLOWERKILLER; i++) {
		err = thread_fork("Lord FlowerKiller Thread",
				  NULL, flowerkiller, NULL, 0);
		if(err)
			goto panic;
	}

	err = thread_fork("Air Balloon",
			  NULL, balloon, NULL, 0);
	if(err)
		goto panic;

	goto done;
panic:
	panic("airballoon: thread_fork failed: %s)\n",
	      strerror(err));

done:
    
    while(true){
      
      if(balloon_flag == 1){
        thread_yield();
        break;
      }
      //thread_yield();
      
    }
    thread_yield();
    free_primitives();
    thread_yield();
    kprintf("Main thread done\n");
	return 0;
}
