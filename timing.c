#define _POSIX_C_SOURCE 199309
#define NEW_TIME_US {0.0,0.0}
#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h>
#include <stdint.h> 
#include <time.h>
#include <sys/time.h> 
#include <sys/resource.h> 
#include <pthread.h> 
#include <semaphore.h> 
#include <fcntl.h> 
#include <string.h>

// not declared in standard headers
extern void *sbrk(intptr_t); 

// get the resolution of the real time clock
//---------------------------------------------------------------
static double seconds_per_tick()
{
    struct timespec res; 
    clock_getres(CLOCK_REALTIME, &res);
    double resolution = res.tv_sec + (((double)res.tv_nsec)/1.0e9);
    return resolution; 
}

// store user and system time in one variable
//---------------------------------------------------------------
struct time_us {
  double u;
  double s;
};

// take an rusage struct and get user/system time
//---------------------------------------------------------------
struct time_us getsec(struct rusage ru){
  struct time_us sec;
  sec.u = (double)ru.ru_utime.tv_sec 
          + ((double)ru.ru_utime.tv_usec)/1e6;
  sec.s = (double)ru.ru_stime.tv_sec 
          + ((double)ru.ru_stime.tv_usec)/1e6;
  return sec;
}

// time a for loop of n iterations
//---------------------------------------------------------------
double for_t(int for_iters){
  struct rusage in_fr;
  struct rusage out_fr;
  struct time_us ft = NEW_TIME_US;
  struct time_us in_f = NEW_TIME_US;
  struct time_us out_f = NEW_TIME_US;
  // measure and loop
  getrusage(RUSAGE_SELF, &in_fr);
  for (int i=0;i<=for_iters;i++){
  }
  getrusage(RUSAGE_SELF, &out_fr);
  // extract time and get mean
  in_f = getsec(in_fr);
  out_f = getsec(out_fr);
  ft.u += (out_f.u - in_f.u);
  return ft.u;
}

// get average time of sbrk for size and iterations
//---------------------------------------------------------------
struct time_us sbrk_timer(int size, int sbrk_iters){
  struct rusage in_sr;
  struct rusage out_sr;
  struct time_us t = NEW_TIME_US;
  struct time_us in_s = NEW_TIME_US;
  struct time_us out_s = NEW_TIME_US;
  int unsize = -1*size; // avoid calculation in the loop
  void *p;
  // measure and loop
  getrusage(RUSAGE_SELF, &in_sr);
  for (int i=0;i<sbrk_iters;i++){
    p=sbrk(size);
    p=sbrk(unsize);
  }
  getrusage(RUSAGE_SELF, &out_sr);
  in_s = getsec(in_sr);
  out_s = getsec(out_sr);
  t.u += (out_s.u - in_s.u)/2;
  t.s += (out_s.s - in_s.s)/2;
  // subtract loop
  t.u -= for_t(sbrk_iters);
  // turn sums into means
  t.u /= (sbrk_iters);
  t.s /= (sbrk_iters);
  return t;
}

// generate path of arbitrary depth
//--------------------------------------------------------------
char *getpath(char* fill, int depth){
  strcat(fill,"~");
  char k[] = "/a";
  char *end = "/";
  for (int i=0;i<depth;i++){
    strcat(fill,k);     
  }
  strcat(fill,end);
  return fill;     
}

//===========================================================================//
//================================= MAIN ====================================//
//===========================================================================//
int main() {
    struct time_us in = NEW_TIME_US;
    struct time_us out = NEW_TIME_US;
    struct rusage in_r;
    struct rusage out_r;
    int success;
    
// MUTEX LOCK MEASUREMENT
//---------------------------------------------------------------   
    struct time_us mutex_t = NEW_TIME_US;
    int mutex_iters = 100000;
    
    // 10 reps of mutex_iters many calls
    int reps = 10;
    for (int j = 0; j <= reps;j++){
      //make a bunch of mutexes and initialize them
      pthread_mutex_t locks[mutex_iters];
      for (int i = 0; i <= mutex_iters-1; i++){
        pthread_mutex_init(&locks[i], NULL);   
      }    
      // turn them on and measure
      getrusage(RUSAGE_SELF, &in_r);
      for (int i = 0; i <= mutex_iters-1; i++){
        pthread_mutex_lock(&locks[i]); 
      }
      getrusage(RUSAGE_SELF, &out_r);
      // clean up
      for (int i = 0; i <= mutex_iters-1; i++){
        pthread_mutex_destroy(&locks[i]);   
      }
      // extract the time 
      in = getsec(in_r);
      out = getsec(out_r);
      mutex_t.u += (out.u - in.u);
      mutex_t.s += (out.s - in.s);
    }
    // subtract total loop time from total time
    mutex_t.u -= (for_t(mutex_iters)*reps);
    // turn sums into means
    mutex_t.u /= (double)(mutex_iters*reps);
    mutex_t.s /= (double)(mutex_iters*reps);
    // print results
    printf("------------------------------------------------------\n");
    printf("mutex\t | \t user=%e system=%e\n",mutex_t.u,mutex_t.s);
    printf("------------------------------------------------------\n");

// SEMAPHORE MEASUREMENT
//---------------------------------------------------------------
    int sem_iters = 100000;
    struct time_us semaphore_t = NEW_TIME_US;
    // only so much room, so repeat
    reps = 10;
    for (int j = 0; j <= reps;j++){
      // declare and initialize semaphores
      sem_t sems[sem_iters];
      for (int i = 0; i <= sem_iters-1; i++){
        success = sem_init(&sems[i],0,0);
        if (success<0) perror("sem_init"); 
      }
      // loop semaphore operations and measure
      getrusage(RUSAGE_SELF, &in_r);
      for (int i = 0; i <= sem_iters; i++){
        success = sem_post(&sems[i]); 
      }
      getrusage(RUSAGE_SELF, &out_r);
      if (success<0) perror("sem_post"); 
      // extract times and increment sums
      in = getsec(in_r);
      out = getsec(out_r);
      semaphore_t.u += (out.u - in.u);
      semaphore_t.s += (out.s - in.s);
    }
    // subtract loop time
    semaphore_t.u -= for_t(sem_iters)*reps;
    // turn sums into means
    semaphore_t.u /= sem_iters;
    semaphore_t.s /= sem_iters;
    //print results
    printf("sem\t | \t user=%e system=%e\n",semaphore_t.u,semaphore_t.s);
    printf("------------------------------------------------------\n");

// OPEN MEASUREMENT     getting random blips of user time...
//---------------------------------------------------------------
    int fd[1000];
    int open_iters = 900;
    int open_depth[3] = {1,500,1000};
    char path[3][2100] = {0};
    char cmd[2200] = {0};
    char nums[1000][4] = {0};
    getpath(path[0],open_depth[0]);
    getpath(path[1],open_depth[1]);
    getpath(path[2],open_depth[2]);
    struct time_us opens_n[3] = {NEW_TIME_US,NEW_TIME_US,NEW_TIME_US};
    struct time_us opens_o[3] = {NEW_TIME_US,NEW_TIME_US,NEW_TIME_US};
    // create numbers for file names
    for(int i=1;i<1000;i++){
      snprintf(nums[i],4,"%i",i);
    }
    // make max-depth directory
    strcat(cmd,"mkdir -p ");
    strcat(cmd,path[2]);
    system(cmd);
    // generate full paths for open to use
    char temp[1000][2200] = {0};
    for (int i=0;i<3;i++){
      printf("open depth %i\n", open_depth[i]);
      for (int j=0;j<1000;j++){
        strcpy(temp[j],path[i]);
        strcat(temp[j],nums[j]);
      }
      // open/create files that don't exist
      getrusage(RUSAGE_SELF, &in_r);
      for (int i=0;i<=open_iters;i++){
          fd[i] = open(temp[i], O_RDONLY, O_CREAT); 
      }
      getrusage(RUSAGE_SELF, &out_r);
      // extract time
      in = getsec(in_r);
      out = getsec(out_r);
      opens_n[i].u += (out.u - in.u);
      opens_n[i].s += (out.s - in.s);
      // subtract loop
      opens_n[i].u -= for_t(open_iters);
      // turn sums into means
      opens_n[i].u /= open_iters;
      opens_n[i].s /= open_iters;
      // print results
      printf("create\t | \t user=%e system=%e\n",opens_n[i].u,opens_n[i].s);
      // close all except first file to free table space
      for (int i=1;i<=open_iters;i++){
          close(fd[i]); 
      }
      // open first file still open
      getrusage(RUSAGE_SELF, &in_r);
      for (int i=1;i<=open_iters;i++){
          fd[i] = open(temp[0],O_RDONLY); 
      }
      getrusage(RUSAGE_SELF, &out_r);
      // extract time
      in = getsec(in_r);
      out = getsec(out_r);
      opens_o[i].u += (out.u - in.u);
      opens_o[i].s += (out.s - in.s);
      // subtract loop
      opens_o[i].u -= for_t(open_iters);
      // turn sums into means
      opens_o[i].u /= open_iters;
      opens_o[i].s /= open_iters;
      // print results
      printf("reopen\t | \t user=%e system=%e\n",opens_o[i].u,opens_o[i].s);
      close(fd[0]);
    }
    // clean up files
    system("rm -rf ~/a");
    printf("------------------------------------------------------\n");

// SBRK MEASUREMENT
//---------------------------------------------------------------  
    int s_iters = 1000000;
    struct time_us sbrk_t[5] = {NEW_TIME_US, NEW_TIME_US,
                                NEW_TIME_US, NEW_TIME_US,
                                NEW_TIME_US};
    
    sbrk_t[0]=sbrk_timer(1,s_iters);
    printf("sbrk 1b\t | \t user=%e system=%e\n",sbrk_t[0].u,sbrk_t[0].s); 
    sbrk_t[1]=sbrk_timer(1024,s_iters);
    printf("     1k\t | \t user=%e system=%e\n",sbrk_t[1].u,sbrk_t[1].s); 
    sbrk_t[2]=sbrk_timer(102400,s_iters);
    printf("   100k\t | \t user=%e system=%e\n",sbrk_t[2].u,sbrk_t[2].s); 
    sbrk_t[3]=sbrk_timer(1048576,s_iters);
    printf("     1M\t | \t user=%e system=%e\n",sbrk_t[3].u,sbrk_t[3].s); 
    sbrk_t[4]=sbrk_timer(104857600,s_iters);
    printf("   100M\t | \t user=%e system=%e\n",sbrk_t[4].u,sbrk_t[4].s);  
    printf("------------------------------------------------------\n");
    
// PER TICK AND TOTAL MEASUREMENT
//---------------------------------------------------------------
    struct rusage buf; 
    getrusage(RUSAGE_SELF, &buf);  
    struct time_us total_t = getsec(buf);
    //print results
    printf("total\t | \t user=%e system=%e\n",total_t.u,total_t.s); 
    printf("------------------------------------------------------\n");
    printf("seconds per tick = %e\n", seconds_per_tick()); 
    printf("------------------------------------------------------\n");
} 

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
