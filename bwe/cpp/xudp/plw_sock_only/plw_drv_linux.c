#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <semaphore.h>
#include <errno.h>
#include <pthread.h>

#include "plw_defs.h"
#include "plw_drv.h"

plw_drv_status plw_drv_delay(plw_u32 ms)
{
	usleep(ms * 1000);
	return PLW_DRVS_SUCCESS;
}

#if 0
extern int sem_timedwait(sem_t *, const struct timespec *);

// android ==>
#ifdef MY_ANDROID
//#include <android_runtime/AndroidRuntime.h>
#include <android/log.h> // __android_log_print
#define dlogi(...)  __android_log_print(ANDROID_LOG_ERROR,"mtv",__VA_ARGS__)  
#define dlogw(...)  __android_log_print(ANDROID_LOG_ERROR,"mtv",__VA_ARGS__) 
#define dloge(...)  __android_log_print(ANDROID_LOG_ERROR,"mtv",__VA_ARGS__)
#define ddump(...)  __android_log_print(ANDROID_LOG_ERROR,"mtv",__VA_ARGS__)

#define dbgi(...)  dlogi(__VA_ARGS__)  
#define dbgw(...)  dlogw(__VA_ARGS__)  
#define dbge(...)  dloge(__VA_ARGS__)  
#endif
// android <==

#ifdef  CONFIG_PLW_SOCK
#include "plw_drv_sock.h"

extern plw_u32 init_sock(void);
extern plw_u32 terminate_sock(void);

#endif



//define obj of this module
typedef struct 
{
    plw_u32 ref;    // reference count
}plw_drv_obj;

#define MAX_SEMAPHORE_VALUE     255

/*************************************/
/*        global variable            */
/*************************************/
static plw_drv_obj *g_pobj = 0;

/*************************************/
/*    initialization & termination   */
/*************************************/
plw_drv_status plw_drv_init(void)
{
	plw_drv_obj *pobj = 0;
    plw_drv_status ret = PLW_DRVS_SUCCESS;
    
	if(g_pobj) {
		g_pobj->ref++;
		return PLW_DRVS_SUCCESS;
	}
    
	pobj = (plw_drv_obj *) malloc( sizeof(plw_drv_obj) );
	if(!pobj) {
		return PLW_DRVS_ERROR;
	}

    #ifdef  CONFIG_PLW_SOCK
    
    ret = init_sock();

    if(ret != PLW_DRVS_SUCCESS)
    {
        goto plw_drv_init_ret;
    }
    
    #endif

    
	pobj->ref = 0;
	g_pobj = pobj;
    
	g_pobj->ref++;

plw_drv_init_ret:


    return ret;

}

plw_drv_status plw_drv_terminate(void)
{
	if(!g_pobj) {
		return PLW_DRVS_ERROR;
	}

	if(g_pobj->ref > 0) {
		g_pobj->ref--;
	}
    
	if(g_pobj->ref == 0) {

        #ifdef  CONFIG_PLW_SOCK

        terminate_sock();

        #endif
        
		free(g_pobj);
		g_pobj = 0;
	}

	return PLW_DRVS_SUCCESS;
}


/*************************************/
/*              memory               */
/*************************************/
plw_drv_status plw_drv_mem_alloc( plw_size size, void ** pptr)
{
	if(!pptr) {
		return PLW_DRVS_ERROR;
	}
	
	*pptr = malloc(size);
	if(!(*pptr)) {
		return PLW_DRVS_ERROR;
	}

	return PLW_DRVS_SUCCESS;
}

plw_drv_status plw_drv_mem_free(void * ptr)
{
	if(!ptr) {
		return PLW_DRVS_ERROR;
	}

	free(ptr);

	return PLW_DRVS_SUCCESS;
}

plw_drv_status plw_drv_mem_copy(void * dst, const void *src, const plw_size size)
{
	if((!dst) || (!src)) {
		return PLW_DRVS_ERROR;
	}

	if(size == 0)
		return PLW_DRVS_SUCCESS;

	memcpy(dst, src, size);

	return PLW_DRVS_SUCCESS;
}

plw_drv_status plw_drv_mem_set(void * dst, const plw_u8 value, const plw_size size)
{
    int c;
    
    if( !dst )
    {
        return PLW_DRVS_ERROR;
    }

    if(size == 0)
        return PLW_DRVS_SUCCESS;

    c = (int)   value;
    
    memset(dst, c, size);

    return PLW_DRVS_SUCCESS;

}

plw_drv_status plw_drv_mem_equ(const void * dst, const void * src, const plw_size size)
{
    if((!dst) || (!src))
    {
        //printf("[error]plw_drv_mem_cmp: dst or src  0!\r\n");
        return PLW_DRVS_ERROR;
    }

    if(size == 0)
        return PLW_DRVS_SUCCESS;

    if(memcmp(dst, src, size))
        return PLW_DRVS_ERROR;

    return PLW_DRVS_SUCCESS;
}

/*************************************/
/*          semaphore                */
/*************************************/

#if 1

// use sem_timedwait

struct plw_drv_sem_st {
    char name[32];
    sem_t * drv_sem;
	sem_t   drv_sem_v;
};

plw_drv_status plw_drv_sem_create(plw_u32 init_value, plw_drv_sem_t *psem)
{
//	int ret;
	struct plw_drv_sem_st *hsem;

	if(!psem) {
		return PLW_DRVS_ERROR;
	}

	hsem = (struct plw_drv_sem_st *)malloc(sizeof(struct plw_drv_sem_st));
	if(hsem == NULL) {
		return PLW_DRVS_ERROR;
	}

    sprintf(hsem->name, "plw_sem_%p", hsem);
    hsem->drv_sem = sem_open( hsem->name, O_CREAT, 0644, init_value );
    if(hsem->drv_sem == SEM_FAILED){
        return PLW_DRVS_ERROR;
    }
    
//	ret = sem_init(&hsem->drv_sem_v, 0, init_value);
//	if(ret != 0) {
//		return PLW_DRVS_ERROR;
//	}
//    hsem->drv_sem = &hsem->drv_sem_v;

	*psem = hsem;

	return PLW_DRVS_SUCCESS;
}

plw_drv_status plw_drv_sem_delete(plw_drv_sem_t sem)
{
	int ret;
	if(!sem) {
		return PLW_DRVS_ERROR;
	}
    
    ret = sem_close( sem->drv_sem );
    sem_unlink(sem->name);
    
//	ret = sem_destroy(sem->drv_sem);
//	if(ret != 0) {
//		return PLW_DRVS_ERROR;
//	}

	free(sem);
	
	return PLW_DRVS_SUCCESS;
}

plw_drv_status plw_drv_sem_wait(plw_drv_sem_t sem)
{
	int ret;
	if(!sem) {
		return PLW_DRVS_ERROR;
	}

	ret = sem_wait(sem->drv_sem);
	if(ret != 0) {
		return PLW_DRVS_ERROR;
	}

	return PLW_DRVS_SUCCESS;
}

plw_drv_status plw_drv_sem_timedwait(plw_drv_sem_t sem, plw_u32 ms)
{
	int ret;
	struct timeval time;
	struct timespec timedout;

	gettimeofday(&time, NULL);

	timedout.tv_sec = time.tv_sec+ms/1000;
	timedout.tv_nsec = (time.tv_usec+(ms%1000) * 1000) * 1000;
       if (timedout.tv_nsec > 999999999){
	    timedout.tv_sec = timedout.tv_sec + 1;
	    timedout.tv_nsec = timedout.tv_nsec - 1000000000;
	}

	//dlogi("plw_drv_sem_timedwait: start wait \n");
	while((ret = sem_timedwait(sem->drv_sem, &timedout) != 0) \
           && (errno == EINTR)){
           //dlogi("plw_drv_sem_timedwait: wake up \n");
           // printf("errno = %d\n", errno);
            gettimeofday(&time, NULL);
            if(time.tv_sec > timedout.tv_sec){
                //dlogi("plw_drv_sem_timedwait: wait sec timeout \n");
                return PLW_DRVS_TIMEOUT;
            } else if((time.tv_sec == timedout.tv_sec) \
                   && ((time.tv_usec * 1000) > timedout.tv_nsec)){

                //dlogi("plw_drv_sem_timedwait: wait usec timeout \n");
                return PLW_DRVS_TIMEOUT;
            }

            //dlogi("plw_drv_sem_timedwait: wait again \n");
        }

    //dlogi("plw_drv_sem_timedwait: wait done\n");
    
	if(ret != 0) {
		if(errno == ETIMEDOUT) {
			return PLW_DRVS_TIMEOUT;
		}
		return PLW_DRVS_ERROR;
	}

	return PLW_DRVS_SUCCESS;
}

plw_drv_status plw_drv_sem_signal(plw_drv_sem_t sem)
{
	int ret;
	if(!sem) {
		return PLW_DRVS_ERROR;
	}
	
	ret = sem_post(sem->drv_sem);
	if(ret != 0) {
		return PLW_DRVS_ERROR;
	}

	return PLW_DRVS_SUCCESS;
}
#else
// use timer for time wait

struct plw_drv_sem_st {
	sem_t           drv_sem;
	timer_t         timer_obj;

	int             req_timeout;
	int             is_timeout;
};


static 
void _sem_time_handle(sigval_t v)
{
    plw_drv_sem_t sem = (plw_drv_sem_t) v.sival_ptr;

    if(sem->req_timeout)
    {
        sem->is_timeout = 1;
        plw_drv_sem_signal(sem);
    }
    
    return;
}


plw_drv_status plw_drv_sem_create(plw_u32 init_value, plw_drv_sem_t *psem)
{
	int ret;

	struct plw_drv_sem_st *hsem;

    struct sigevent se;


    
	if(!psem) {
		return PLW_DRVS_ERROR;
	}

	hsem = (struct plw_drv_sem_st *)malloc(sizeof(struct plw_drv_sem_st));
	if(hsem == NULL) {
		return PLW_DRVS_ERROR;
	}

	memset(hsem, 0, sizeof(struct plw_drv_sem_st));

	ret = sem_init(&hsem->drv_sem, 0, init_value);
	if(ret != 0) {
		return PLW_DRVS_ERROR;
	}



    memset(&se, 0, sizeof (se));
    se.sigev_notify = SIGEV_THREAD;
    se.sigev_notify_function = _sem_time_handle;
    se.sigev_value.sival_int = 0;
    se.sigev_value.sival_ptr = hsem;

    if (timer_create (CLOCK_REALTIME, &se, &hsem->timer_obj) < 0)
    {
        dloge ("sem: timer_creat fail !!!!!!!!!!!!!!!!\n");
        return PLW_DRVS_ERROR;
    }
    

	*psem = hsem;

	return PLW_DRVS_SUCCESS;
}

plw_drv_status plw_drv_sem_delete(plw_drv_sem_t sem)
{
	int ret;
	if(!sem) {
		return PLW_DRVS_ERROR;
	}

    timer_delete(sem->timer_obj);
    
	ret = sem_destroy(&sem->drv_sem);
	if(ret != 0) {
		return PLW_DRVS_ERROR;
	}

	free(sem);
	
	return PLW_DRVS_SUCCESS;
}

plw_drv_status plw_drv_sem_wait(plw_drv_sem_t sem)
{
	int ret;
	if(!sem) {
		return PLW_DRVS_ERROR;
	}

	ret = sem_wait(&sem->drv_sem);
	if(ret != 0) {
		return PLW_DRVS_ERROR;
	}

	return PLW_DRVS_SUCCESS;
}

plw_drv_status plw_drv_sem_timedwait(plw_drv_sem_t sem, plw_u32 ms)
{
	int ret;
	struct timeval time;
	struct timespec timedout;
    struct itimerspec ts, ots;
    
	gettimeofday(&time, NULL);

	timedout.tv_sec = time.tv_sec+ms/1000;
	timedout.tv_nsec = (time.tv_usec+(ms%1000) * 1000) * 1000;
       if (timedout.tv_nsec > 999999999){
	    timedout.tv_sec = timedout.tv_sec + 1;
	    timedout.tv_nsec = timedout.tv_nsec - 1000000000;
	}


    // set timer
    sem->is_timeout = 0;  // be careful , should lock hear
    sem->req_timeout = 1;
    
    memset(&ts, 0, sizeof (ts));
    ts.it_value = timedout;
    ts.it_interval.tv_sec = 0;
    ts.it_interval.tv_nsec = 0;
    if (timer_settime (sem->timer_obj, TIMER_ABSTIME, &ts, &ots) < 0)
    {
        dbgi ("sem: timer_settime fail 1 !!!!!!!!!!!!!!!!\n");
        return PLW_DRVS_ERROR;
    }

    
	dlogi("plw_drv_sem_timedwait: start wait ++\n");
	while((ret = sem_wait(&sem->drv_sem) != 0) && (errno == EINTR))
	{
        dlogi("plw_drv_sem_timedwait: wake up \n");
        dlogi("errno = %d\n", errno);
        gettimeofday(&time, NULL);
        if(time.tv_sec > timedout.tv_sec)
        {
            dlogi("plw_drv_sem_timedwait: wait sec timeout \n");
            return PLW_DRVS_TIMEOUT;
        }
        else if((time.tv_sec == timedout.tv_sec) && ((time.tv_usec * 1000) > timedout.tv_nsec))
        {

            dlogi("plw_drv_sem_timedwait: wait usec timeout \n");
            return PLW_DRVS_TIMEOUT;
        }

        dlogi("plw_drv_sem_timedwait: wait again \n");
    }

    dlogi("plw_drv_sem_timedwait: wait done--\n");

    sem->req_timeout = 0;
    
    memset(&ts, 0, sizeof (ts));
    ts.it_value.tv_sec = 2147483647;
    ts.it_value.tv_nsec = 0;
    if (timer_settime (sem->timer_obj, TIMER_ABSTIME, &ts, &ots) < 0)
    {
        dbgi ("sem: timer_settime fail 2 !!!!!!!!!!!!!!!!\n");
        return PLW_DRVS_ERROR;
    }

    if(sem->is_timeout)
    {
        sem->is_timeout = 0;
        return PLW_DRVS_TIMEOUT;
    }

    if(ret != 0) 
    {
        return PLW_DRVS_ERROR;
    }
    

	return PLW_DRVS_SUCCESS;
}

plw_drv_status plw_drv_sem_signal(plw_drv_sem_t sem)
{
	int ret;
	if(!sem) {
		return PLW_DRVS_ERROR;
	}
	
	ret = sem_post(&sem->drv_sem);
	if(ret != 0) {
		return PLW_DRVS_ERROR;
	}

	return PLW_DRVS_SUCCESS;
}


#endif

/*************************************/
/*             task                  */
/*************************************/
struct plw_drv_task_st {
	pthread_t drv_task;
	
	plw_drv_task_entry_t task_func;

	void * task_param;

	//sem_t resume_sem;
    plw_drv_sem_t resume_sem;
};


void *_thread_proc(void *param)
{

	struct plw_drv_task_st  *threadparam = (struct plw_drv_task_st *)param;

	plw_drv_task_entry_t task_func = threadparam->task_func ;
	
	void *task_param = threadparam->task_param;

//       sem_wait(&threadparam->resume_sem);
    plw_drv_sem_wait(threadparam->resume_sem);
	 
	
	task_func(task_param);

	param = NULL;
//       sem_destroy(&threadparam->resume_sem);
    plw_drv_sem_delete(threadparam->resume_sem);

	pthread_detach(pthread_self());
	
	return 0;

}

plw_drv_status plw_drv_task_create
				(
					plw_drv_task_entry_t    entry,
					void *			task_param,
					plw_size			stack_size,
					plw_drv_priority	prior,
					plw_drv_task_t*	ptask 
				)
{
	int ret;
	pthread_attr_t attr;
	struct sched_param param;
	struct plw_drv_task_st *task = 0;
	plw_drv_status vret = PLW_DRVS_ERROR;

	task = (struct plw_drv_task_st *)malloc(sizeof(struct plw_drv_task_st));
	if(!task) {
		return PLW_DRVS_ERROR;
	}
    
	
	task->task_func = entry;
	task->task_param = task_param;
	
//	ret = sem_init(&task->resume_sem, 0, 0);
    ret = plw_drv_sem_create(0, &task->resume_sem);
	if(ret != 0) {
		vret = PLW_DRVS_ERROR;
		goto plw_drv_task_create_end;
	}
	  
        ret = pthread_attr_init(&attr);
	if(ret != 0) {
		vret = PLW_DRVS_ERROR;
		goto plw_drv_task_create_end;
	}
	
	ret = pthread_attr_getschedparam(&attr, &param);
	if(ret != 0) {
		vret = PLW_DRVS_ERROR;
		goto plw_drv_task_create_end;
	}
	
	//  android do NOT support schedule priority
	#if 0
	if(	(prior == PLW_DRV_PRIOR_VERY_LOW) || 
		(prior == PLW_DRV_PRIOR_LOW) || 
		(prior == PLW_DRV_PRIOR_NORMAL)
	) {
		param.__sched_priority = 0;
	} else if(prior == PLW_DRV_PRIOR_HIGH) {
		param.__sched_priority = 5;
	} else if (prior == PLW_DRV_PRIOR_VERY_HIGH){
		param.__sched_priority = 25;
	} else {
		vret = PLW_DRVS_ERROR;
		goto plw_drv_task_create_end;
	}
	#endif

	
	
	ret = pthread_attr_setschedparam(&attr, &param);
	if(ret != 0) {
		vret = PLW_DRVS_ERROR;
		goto plw_drv_task_create_end;
	}
	
	ret = pthread_attr_setstacksize(&attr, stack_size);
	
	ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if(ret != 0) {
		vret = PLW_DRVS_ERROR;
              goto plw_drv_task_create_end;
	}

	ret = pthread_create(&task->drv_task, &attr, _thread_proc, (void*)task);
	if(ret != 0) {
		vret = PLW_DRVS_ERROR;
              goto plw_drv_task_create_end;
	}

	*ptask = (plw_drv_task_t) task;
	
	vret =  PLW_DRVS_SUCCESS;

       
//       sem_post(&task->resume_sem);
    plw_drv_sem_signal(task->resume_sem);
	   
plw_drv_task_create_end:

    if(vret != PLW_DRVS_SUCCESS)
    {
        if(task)
        {
            free(task);
            task = 0;
        }
    }
    
    return vret;
	
}


plw_drv_status plw_drv_task_delete
				(
					plw_drv_task_t	task
				)
{
//	int ret;
//	ret = pthread_cancel(task->drv_task);
        free(task);
	return PLW_DRVS_SUCCESS;
}

plw_drv_status plw_drv_task_is_self
				(
					plw_drv_task_t      task,
					plw_bool            *is_self
				)
{
	if(task->drv_task == pthread_self()) {
		*is_self = PLW_TRUE;
	} else {
		*is_self = PLW_FALSE;
	}
    
	return PLW_DRVS_SUCCESS;
}



#if 0
/*************************************/
/*            timer                  */
/*************************************/

plw_drv_status plw_drv_timer_create(plw_drv_timer_t *ptimer);

plw_drv_status plw_drv_timer_delete(plw_drv_timer_t timer);

plw_drv_status plw_drv_timer_wait(plw_drv_timer_t timer, plw_u32 ms);

plw_drv_status plw_drv_timer_stop(plw_drv_timer_t timer);

#endif

/*************************************/
/*              storage              */
/*************************************/


#if 0 // simon


#define StorageFileName "stor.dat"


struct plw_drv_storage_st {
	int  drv_storage;
};



plw_drv_status plw_drv_storage_open(plw_size size, plw_drv_storage_t * pstroage)
{
	char buffer= 0xff;
	unsigned int filesize;

	*pstroage = (struct plw_drv_storage_st*)malloc(sizeof(struct plw_drv_storage_st));

	if (!(*pstroage))
		return PLW_DRVS_ERROR;
	

	
	(*pstroage)->drv_storage = open(StorageFileName, O_CREAT|O_RDWR);

	if ((*pstroage)->drv_storage == -1)
	{
		return PLW_DRVS_ERROR;
	}

	filesize = lseek((*pstroage)->drv_storage, 0, SEEK_END);

	if (filesize != size)
		 plw_drv_storage_write(*pstroage ,size-1,1, (plw_u8 *) &buffer);



	 return PLW_DRVS_SUCCESS;


}


plw_drv_status plw_drv_storage_close(plw_drv_storage_t  stroage)
{

	close(stroage->drv_storage);

	free(stroage);

	return PLW_DRVS_SUCCESS;

}

plw_drv_status plw_drv_storage_read
                            (
                            plw_drv_storage_t  stroage,
                            plw_size    offset,
                            plw_size    length,
                            plw_u8      *buffer
                            )
{
	unsigned int hasread;

	if (!stroage)
		return PLW_DRVS_ERROR;

	
	lseek(stroage->drv_storage, offset, SEEK_SET);

	hasread = read(stroage->drv_storage, buffer, length);


	if (hasread != length)
	{
		return PLW_DRVS_ERROR;
	}

	return PLW_DRVS_SUCCESS;

}
                            
plw_drv_status plw_drv_storage_write
(
 plw_drv_storage_t  stroage,
 plw_size    offset,
 plw_size    length,
 const plw_u8      *buffer
 )
{
	
	unsigned int haswrite;
	
	if (!stroage)
		return PLW_DRVS_ERROR;
	
	
	lseek(stroage->drv_storage, offset, SEEK_SET);
	
	haswrite = write(stroage->drv_storage, buffer, length);
	
	if (haswrite != length)
	{
		
		return PLW_DRVS_ERROR;
		
	}
	
	return PLW_DRVS_SUCCESS;
	
}

#endif

/*************************************/
/*          miscellaneous            */
/*************************************/
/*
plw_u32 plw_drv_get_tick(void)
{
	struct timespec tp;
	
	clock_gettime(CLOCK_REALTIME, &tp);

	return (tp.tv_sec*1000 + tp.tv_nsec/1000000);
}
*/

plw_u32 plw_drv_get_tick(void)
{
	struct timeval time;
	gettimeofday(&time, NULL);
	return (time.tv_sec*1000 + time.tv_usec/1000);
}

void plw_drv_print(const plw_char *  pstr)
{
	printf("%s", pstr);
	return;
}

plw_char plw_drv_get_char(void)
{
	return (plw_char)getchar();
}

plw_drv_status plw_drv_delay(plw_u32 ms)
{
	usleep(ms * 1000);
	return PLW_DRVS_SUCCESS;
}

plw_drv_status  plw_drv_console_in
                (
                plw_char *          pc
                )
{
    int                 ic;
    plw_u32             ret = 0;

    do
    {
        ic = getchar();
        if(ic == EOF)
        {
            ret = PLW_DRVS_ERROR;
            break;
        }

        *pc = (plw_char) ic;

        ret = 0;
            
    }while(0);

    return ret;
    
}
               
void            plw_drv_console_in_close
                (
                void
                )
{
    fclose(stdin);
}


#endif
