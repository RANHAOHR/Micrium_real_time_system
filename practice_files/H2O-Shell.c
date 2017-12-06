/*
*********************************************************************************************************
*                                              EXAMPLE CODE
*
*                             (c) Copyright 2009; Micrium, Inc.; Weston, FL
*
*               All rights reserved.  Protected by international copyright laws.
*               Knowledge of the source code may NOT be used to develop a similar product.
*               Please help us continue to provide the Embedded community with the finest
*               software available.  Your honesty is greatly appreciated.
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*
*                                            EXAMPLE CODE
*
*                                     ST Microelectronics STM32
*                                              on the
*
*                                     Micrium uC-Eval-STM32F107
*                                        Evaluation Board
*
* Filename      : app.c
* Version       : V1.00
* Programmer(s) : JJL
                  EHS
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                             INCLUDE FILES
*********************************************************************************************************
*/

#include <includes.h>


/*
*********************************************************************************************************
*                                             LOCAL DEFINES
*********************************************************************************************************
*/

/*
*********************************************************************************************************
*                                            LOCAL VARIABLES
*********************************************************************************************************
*/

#define TASK_STK_SIZE 256
#define NUM_O 3
#define NUM_H 5
#define O_PRIO 3
#define H_PRIO 3

#define HYDRO_NUM 2

static  OS_TCB   AppTaskStartTCB; 
static  OS_TCB   OTaskTCB[NUM_O]; 
static  OS_TCB   HTaskTCB[NUM_H];

static  CPU_STK  AppTaskStartStk[APP_TASK_START_STK_SIZE];
static  CPU_STK  OTaskStks[NUM_O][TASK_STK_SIZE];
static  CPU_STK  HTaskStks[NUM_H][TASK_STK_SIZE];


int count = 1;
/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void  AppTaskStart  (void *p_arg);
static  void  OTask  (void *p_arg);
static  void  HTask  (void *p_arg);

OS_MUTEX access;
OS_MUTEX access1;

OS_SEM oxygen;
OS_SEM hydro;

static  void delayRandom(int maxi);

/*
*********************************************************************************************************
*                                                main()
*
* Description : This is the standard entry point for C code.  It is assumed that your code will call
*               main() once you have performed all necessary initialization.
*
* Arguments   : none
*
* Returns     : none
*********************************************************************************************************
*/


int  main (void)
{
    OS_ERR  err;


    BSP_IntDisAll();                                            /* Disable all interrupts.                              */

    OSInit(&err);                                               /* Init uC/OS-III.                                      */

    OSTaskCreate((OS_TCB     *)&AppTaskStartTCB,                /* Create the start task                                */
                 (CPU_CHAR   *)"App Task Start",
                 (OS_TASK_PTR )AppTaskStart, 
                 (void       *)0,
                 (OS_PRIO     )2,
                 (CPU_STK    *)&AppTaskStartStk[0],
                 (CPU_STK_SIZE)APP_TASK_START_STK_SIZE / 10,
                 (CPU_STK_SIZE)APP_TASK_START_STK_SIZE,
                 (OS_MSG_QTY  )0,
                 (OS_TICK     )0,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);

      
  
    OSStart(&err);                                              /* Start multitasking (i.e. give control to uC/OS-III). */
}


/*
*********************************************************************************************************
*                                          STARTUP TASK
*
* Description : This is an example of a startup task.  As mentioned in the book's text, you MUST
*               initialize the ticker only once multitasking has started.
*
* Arguments   : p_arg   is the argument passed to 'AppTaskStart()' by 'OSTaskCreate()'.
*
* Returns     : none
*
* Notes       : 1) The first line of code is used to prevent a compiler warning because 'p_arg' is not
*                  used.  The compiler should not generate any code for this statement.
*********************************************************************************************************
*/

static  void  AppTaskStart (void *p_arg)
{
    CPU_INT32U  cpu_clk_freq;
    CPU_INT32U  cnts;
    OS_ERR      err;

   (void)p_arg;
    
    BSP_Init();                                                   /* Initialize BSP functions                         */
    CPU_Init();                                                   /* Initialize the uC/CPU services                   */

    cpu_clk_freq = BSP_CPU_ClkFreq();                             /* Determine SysTick reference freq.                */                                                                        
    cnts         = cpu_clk_freq / (CPU_INT32U)OSCfg_TickRate_Hz;  /* Determine nbr SysTick increments                 */
    OS_CPU_SysTickInit(cnts);                                     /* Init uC/OS periodic time src (SysTick).          */

    OSSemCreate(&oxygen,"oxygen sem",0,&err);
    OSSemCreate(&hydro,"hydro sem",0,&err);

    OSMutexCreate(&access,"Access",&err);
    OSMutexCreate(&access1,"Access1",&err);
    
    for (int i = 0; i < NUM_O; i++) {   
      OSTaskCreate((OS_TCB   *)&OTaskTCB[i],
                 (CPU_CHAR   *)"Oxygen",
                 (OS_TASK_PTR )OTask, 
                 (void       *)0,
                 (OS_PRIO     )O_PRIO,                   
                 (CPU_STK    *)&OTaskStks[i][0],
                 (CPU_STK_SIZE)TASK_STK_SIZE / 10,
                 (CPU_STK_SIZE)TASK_STK_SIZE,
                 (OS_MSG_QTY  )0,
                 (OS_TICK     )0,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);
      
    }
    for (int i = 0; i < NUM_H; i++) {   
      OSTaskCreate((OS_TCB   *)&HTaskTCB[i],
                 (CPU_CHAR   *)"Hydrogen",
                 (OS_TASK_PTR )HTask, 
                 (void       *)0,
                 (OS_PRIO     )H_PRIO,                   
                 (CPU_STK    *)&HTaskStks[i][0],
                 (CPU_STK_SIZE)TASK_STK_SIZE / 10,
                 (CPU_STK_SIZE)TASK_STK_SIZE,
                 (OS_MSG_QTY  )0,
                 (OS_TICK     )0,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);
      
    }
    
    /* code to check trace below */
	
    BSP_LED_Off(0);
    OSTaskSuspend(0,&err);       

}

static void makewater()
{ 
    OS_ERR err;
    OSTimeDlyHMSM(0, 0, 0, 300, 
                      OS_OPT_TIME_HMSM_STRICT, 
                      &err);    
    BSP_LED_Off(0);
    OSTimeDlyHMSM(0, 0, 0, 300, 
                      OS_OPT_TIME_HMSM_STRICT, 
                      &err); 
}


static  void  OTask (void *p_arg)
{
    OS_ERR      err;
    CPU_TS ts;

    while (DEF_TRUE) {   
        delayRandom(500);                                                  
		
	/* PUT YOUR CODE HERE!! */
        OSMutexPend(&access,0,OS_OPT_PEND_BLOCKING,&ts,&err);
        BSP_LED_On(1); //light up the  green one
        for(int i = 0; i < HYDRO_NUM; i++)
          OSSemPost(&hydro,OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED,&err); //tell hydro oxygen is ready

        for (int i = 0; i < HYDRO_NUM; i++){
          OSSemPend(&oxygen,0,OS_OPT_PEND_BLOCKING,&ts,&err);
        }
         makewater();

        OSMutexPost(&access,OS_OPT_POST_1,&err);

    }
}

static  void  HTask (void *p_arg)
{
    OS_ERR      err;
    CPU_TS ts;

    while (DEF_TRUE) {
        delayRandom(500);
		
	/* PUT YOUR CODE HERE!! */
        OSSemPend(&hydro,0,OS_OPT_PEND_BLOCKING,&ts,&err);
        OSMutexPend(&access1,0,OS_OPT_PEND_BLOCKING,&ts,&err); 
        count++;
        BSP_LED_On(count);
        if (count == (HYDRO_NUM + 1)) {
          for (int i = 0; i < HYDRO_NUM; i++){
            OSSemPost(&oxygen, OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED,&err);
          }
          count = 1;
        }
        
        OSMutexPost(&access1,OS_OPT_POST_1,&err);				
    }    
}



void delayRandom(int maxi)
{
    OS_ERR err;
    float delay = ((float)rand()) * maxi / RAND_MAX;
    OSTimeDlyHMSM(0, 0, 1, (int)delay, 
                      OS_OPT_TIME_HMSM_STRICT, 
                      &err);
}