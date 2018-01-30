

#ifndef __PLW_DRV_H__
#define __PLW_DRV_H__

#include "plw_defs.h"

/* --> simon */
#ifndef __FUNCTION__
#define __FUNCTION__ "unknown"
#endif

#ifndef CONFIG_PLW_SOCK
#define CONFIG_PLW_SOCK
#endif

#ifndef CONFIG_PLW_FILE
#define CONFIG_PLW_FILE
#endif

#ifndef CONFIG_PLW_TIME
#define CONFIG_PLW_TIME
#endif

#ifndef CONFIG_PLW_CFG_FILE
#define CONFIG_PLW_CFG_FILE
#endif

#ifndef CONFIG_PLW_CFG_DIR
#define CONFIG_PLW_CFG_DIR
#endif

/* <-- simon */

#ifdef __cplusplus  
extern "C" {
#endif  

typedef enum
{
    PLW_DRVS_SUCCESS = 0,

    PLW_DRVS_TIMEOUT,
    
    PLW_DRVS_ERROR
    
}plw_drv_status;



/*************************************/
/*    initialization & termination   */
/*************************************/

plw_drv_status plw_drv_init(void);

plw_drv_status plw_drv_terminate(void);




/*************************************/
/*              memory               */
/*************************************/

plw_drv_status plw_drv_mem_alloc( plw_size size, void ** pptr);

plw_drv_status plw_drv_mem_free(void * ptr);

plw_drv_status plw_drv_mem_copy(void * dst, const void *src, const plw_size size);

plw_drv_status plw_drv_mem_set(void * dst, const plw_u8 value, const plw_size size);

plw_drv_status plw_drv_mem_equ(const void * dst, const void * src, const plw_size size);


/*************************************/
/*          semaphore                */
/*************************************/

typedef struct plw_drv_sem_st * plw_drv_sem_t ;

plw_drv_status plw_drv_sem_create(plw_u32 init_value, plw_drv_sem_t *psem);

plw_drv_status plw_drv_sem_delete(plw_drv_sem_t sem);

plw_drv_status plw_drv_sem_wait(plw_drv_sem_t sem);

plw_drv_status plw_drv_sem_timedwait(plw_drv_sem_t sem, plw_u32 ms);

plw_drv_status plw_drv_sem_signal(plw_drv_sem_t sem);




/*************************************/
/*             task                  */
/*************************************/

typedef struct plw_drv_task_st * plw_drv_task_t;

typedef void (*plw_drv_task_entry_t)(void *param);

typedef enum
{

    PLW_DRV_PRIOR_VERY_LOW,

    PLW_DRV_PRIOR_LOW,
    
    PLW_DRV_PRIOR_NORMAL,
    
    PLW_DRV_PRIOR_HIGH,
    
    PLW_DRV_PRIOR_VERY_HIGH

} plw_drv_priority;

plw_drv_status plw_drv_task_create
                        (
                        plw_drv_task_entry_t    entry,
                        void *                  task_param,
					    plw_size                stack_size,
					    plw_drv_priority        prior,
					    plw_drv_task_t*         ptask 
					    );



plw_drv_status plw_drv_task_delete
                        (
                        plw_drv_task_t          task
                        );



plw_drv_status plw_drv_task_is_self
                        (
                            plw_drv_task_t      task,
                            plw_bool            *is_self
                        );






/*************************************/
/*              storage              */
/*************************************/

typedef struct plw_drv_storage_st * plw_drv_storage_t;

plw_drv_status plw_drv_storage_open
                            (
                            plw_size size, 
                            plw_drv_storage_t * pstroage
                            );


plw_drv_status plw_drv_storage_close
                            (
                            plw_drv_storage_t  stroage
                            );

plw_drv_status plw_drv_storage_read
                            (
                            plw_drv_storage_t  stroage,
                            plw_size    offset,
                            plw_size    length,
                            plw_u8      *buffer
                            );
                            
plw_drv_status plw_drv_storage_write
                            (
                            plw_drv_storage_t  stroage,
                            plw_size    offset,
                            plw_size    length,
                            const plw_u8      *buffer
                            );
                            

/*************************************/
/*          miscellaneous            */
/*************************************/

plw_u32 plw_drv_get_tick(void);

void plw_drv_print( const plw_char *  pstr );

plw_char plw_drv_get_char(void);

plw_drv_status plw_drv_delay(plw_u32 ms);    



plw_drv_status  plw_drv_console_in
                (
                plw_char *          pc
                );
               
void            plw_drv_console_in_close
                (
                void
                );


#endif

#ifdef __cplusplus  
}
#endif  

/* end of file */

