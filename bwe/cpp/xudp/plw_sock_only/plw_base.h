
/* plw_base.h */

#ifndef __PLW_BASE_H__
#define __PLW_BASE_H__

#include <stdarg.h>

#include "plw_defs.h"
#include "plw_drv.h"



typedef enum
{

    PLWS_SUCCESS = PLW_DRVS_SUCCESS,

    PLWS_TIMEOUT = PLW_DRVS_TIMEOUT,
    
    PLWS_ERROR = PLW_DRVS_ERROR
    
}plw_status;






/*************************************/
/*    initialization & termination   */
/*************************************/

plw_status plw_init(void);

plw_status plw_terminate(void);






/*************************************/
/*              memory               */
/*************************************/

#define plw_mem_alloc(xsize, xpptr)         plw_drv_mem_alloc(xsize, (void **)(xpptr))

#define plw_mem_free(xptr)                  plw_drv_mem_free(xptr)

#define plw_mem_copy(xdst, xsrc, xsize)     plw_drv_mem_copy(xdst, xsrc, xsize) 

#define plw_mem_set(xdst, xvalue, xsize)    plw_drv_mem_set(xdst, xvalue, xsize) 

#define plw_mem_equ(xdst, xsrc, xsize)    plw_drv_mem_equ(xdst, xsrc, xsize)

plw_status plw_mem_zero_alloc_( plw_size size, void ** pptr);

#define plw_mem_zero_alloc(xsize, xpptr) plw_mem_zero_alloc_((xsize), (void **)(xpptr))

void plw_mem_reverse_copy(plw_u8 *dst, const plw_u8 *src, const plw_u32 len);


/*************************************/
/*          semaphore                */
/*************************************/

typedef plw_drv_sem_t plw_sem_t ;

#define plw_sem_create(xvalue, xpsem)           plw_drv_sem_create(xvalue, xpsem)

#define plw_sem_delete(xsem)                    plw_drv_sem_delete(xsem)

#define plw_sem_wait(xsem)                      plw_drv_sem_wait(xsem)

#define plw_sem_timedwait(xsem, xms)            plw_drv_sem_timedwait( xsem, xms)

#define plw_sem_signal(xsem)                    plw_drv_sem_signal(xsem)




/*************************************/
/*            mutex                  */
/*************************************/

typedef plw_drv_sem_t plw_mutex_t ;

#define plw_mutex_create(xpmutex)               plw_drv_sem_create(1, xpmutex)

#define plw_mutex_delete(xmutex)                plw_drv_sem_delete(xmutex)

#define plw_mutex_lock(xmutex)                  plw_drv_sem_wait(xmutex)

#define plw_mutex_unlock(xmutex)                plw_drv_sem_signal(xmutex)




/*************************************/
/*            event                  */
/*************************************/

typedef struct plw_event_tag * plw_event_t;

plw_status plw_event_create(plw_event_t * pevent);

plw_status plw_event_delete(plw_event_t event);

plw_status plw_event_wait(plw_event_t event);

plw_status plw_event_timedwait(plw_event_t event, plw_u32 ms);

plw_status plw_event_set(plw_event_t event);




/*************************************/
/*             task                  */
/*************************************/



typedef enum
{

    PLW_PRIOR_VERY_LOW =    PLW_DRV_PRIOR_VERY_LOW,

    PLW_PRIOR_LOW =         PLW_DRV_PRIOR_LOW,
    
    PLW_PRIOR_NORMAL =      PLW_DRV_PRIOR_NORMAL,
    
    PLW_PRIOR_HIGH =        PLW_DRV_PRIOR_HIGH,
    
    PLW_PRIOR_VERY_HIGH =   PLW_DRV_PRIOR_VERY_HIGH

} plw_priority_t;

typedef plw_drv_task_entry_t    plw_task_entry_t;

#if 0
typedef plw_drv_task_t plw_task_t;

#define plw_task_create(xentry, xparam, xstack_size, xprior, xptask ) \
                plw_drv_task_create(xentry, xparam, xstack_size, (plw_drv_priority)(xprior), xptask )


#define plw_task_delete(xtask)      plw_drv_task_delete(xtask)

#define plw_task_is_self(xtask, xpself)       plw_drv_task_is_self(xtask, xpself)

#else

typedef struct plw_task_st * plw_task_t;


plw_status plw_task_create
                        (
                        plw_task_entry_t        entry,
                        void *                  task_param,
					    plw_size                stack_size,
					    plw_priority_t          prior,
					    plw_task_t*             ptask 
					    );

plw_status plw_task_delete
                        (
                        plw_task_t              task
                        );



plw_status plw_task_is_self
                        (
                            plw_task_t      task,
                            plw_bool            *is_self
                        );

typedef void (*plw_task_hook_t)(plw_task_t task, void * param);

void plw_task_set_global_hook(plw_task_hook_t hook_start, plw_task_hook_t hook_end);

#endif
                           

/*************************************/
/*          miscellaneous            */
/*************************************/

#define plw_get_tick()      plw_drv_get_tick()

#define plw_get_char()      plw_drv_get_char()

#define plw_delay(xms)      plw_drv_delay(xms)

// #define plw_print_str(x)    plw_drv_print(x)
typedef void (* plw_print_func_t)( const plw_char *  pstr );
void plw_print_str( const plw_char *  pstr );
void plw_set_print_func(plw_print_func_t func);

#define plw_console_in(xpc) plw_drv_console_in(xpc)

#define plw_console_in_close() plw_drv_console_in_close()


plw_s32 plw_vsprintf(plw_char *buf, const plw_char *fmt, va_list args);

plw_s32 plw_sprintf(plw_char *buf, const plw_char *fmt, ...);

void plw_printf(const plw_char *fmt, ...);

plw_u32 plw_input_line(plw_char *buff, plw_u32 buff_len);

plw_status plw_console_in_line(plw_char *buff, plw_u32 buff_len, plw_u32 * plen);


plw_s32 plw_atoi(const plw_char * data_ptr);
plw_u32 plw_atou(const plw_char * data_ptr);

/*************************************/
/*          debug macro              */
/*************************************/
#ifndef __FILE__
#define __FILE__    "0"
#endif

#ifndef __LINE__
#define __LINE__    0
#endif


#define plw_error() plw_printf("Err on file %s, line %u\r\n", __FILE__, __LINE__);

#define plw_verify(x)   \
    do  \
    {   \
        if(!(x))\
        {       \
            plw_error();\
            while(1){};\
        }\
    }while(0)\


void plw_get_version(plw_u32 * pmajor, plw_u32 * pminor, plw_u32 *prev);

const plw_char*  plw_get_version_str();

#endif


