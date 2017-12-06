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

#define DEFAULT_TASK_STK_SIZE 256
#define NUM_TASKS 3
#define TASK_PRIO 3

static  OS_TCB   AppTaskStartTCB;
static  OS_TCB   TaskTCB[NUM_TASKS];
static  OS_TCB   FlagTaskTCB;

static  CPU_STK  AppTaskStartStk[APP_TASK_START_STK_SIZE];
static  CPU_STK  TaskStks[NUM_TASKS][DEFAULT_TASK_STK_SIZE];
static  CPU_STK  FlagTaskStk[DEFAULT_TASK_STK_SIZE];

static char Names[NUM_TASKS][16];
int count = 0;

OS_SEM access;
OS_FLAG_GRP readDone;
/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void  AppTaskStart  (void *p_arg);
static  void  Task  (void *p_arg);
static  void  FlagTask  (void *p_arg);

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

 
    OSSemCreate(&access,"Access",1,&err);
    OSFlagCreate(&readDone,"Ready",0,&err);

    for (int i = 0; i < NUM_TASKS; i++) {
      sprintf(Names[i],"Task %d",i);
      OSTaskCreate((OS_TCB   *)&TaskTCB[i],
                 (CPU_CHAR   *)Names[i],
                 (OS_TASK_PTR )Task,
                 (void       *)(i+1),
                 (OS_PRIO     )TASK_PRIO,
                 (CPU_STK    *)&TaskStks[i][0],
                 (CPU_STK_SIZE)DEFAULT_TASK_STK_SIZE / 10,
                 (CPU_STK_SIZE)DEFAULT_TASK_STK_SIZE,
                 (OS_MSG_QTY  )0,
                 (OS_TICK     )0,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);

    }

    OSTaskCreate((OS_TCB   *)&FlagTaskTCB,
               (CPU_CHAR   *)"flag task",
               (OS_TASK_PTR )FlagTask,
               (void       *)0,
               (OS_PRIO     )TASK_PRIO,
               (CPU_STK    *)&FlagTaskStk[0],
               (CPU_STK_SIZE)DEFAULT_TASK_STK_SIZE / 10,
               (CPU_STK_SIZE)DEFAULT_TASK_STK_SIZE,
               (OS_MSG_QTY  )0,
               (OS_TICK     )0,
               (void       *)0,
               (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
               (OS_ERR     *)&err);


    /* code to check trace below */

    BSP_LED_Off(0);
    OSTaskSuspend(0,&err);

}

void delay()
{
  OS_ERR err;
  float delay = ((float)rand()) * 500 / RAND_MAX + 200;
  OSTimeDlyHMSM(0, 0, 0, (int)delay,
                OS_OPT_TIME_HMSM_STRICT,
                &err);
}

void fixeddelay(int d)
{
    OS_ERR err;
    OSTimeDlyHMSM(0, 0, 0, d,
                      OS_OPT_TIME_HMSM_STRICT,
                      &err);
}

static  void  Task (void *p_arg)
{

    CPU_TS ts;
    OS_ERR err;
    
    int taskno = (int)p_arg;
    OS_FLAGS flag;

    while (DEF_TRUE) {
       delay();
       BSP_LED_On(taskno);
       OSSemPend(&access,0,OS_OPT_PEND_BLOCKING,&ts,&err);
//       count++;
//       if(count == NUM_TASKS){
//          OSFlagPost(&readDone,0x07,OS_OPT_POST_FLAG_CLR,&err);
//          count= 0;
//       }
       flag = pow(2,taskno) -1;
       printf("get flag %d\n", flag);
       OSFlagPost(&readDone,flag,OS_OPT_POST_FLAG_SET,&err);
       printf("get taskno %d\n", taskno);
       OSSemPost(&access,OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED,&err);

    }
}

static  void  FlagTask (void *p_arg){
  
  CPU_TS ts;
  OS_ERR err;
  (void)p_arg;

  while (DEF_TRUE) {

    OSFlagPend(&readDone,0x07,0,OS_OPT_PEND_FLAG_SET_ALL + OS_OPT_PEND_BLOCKING, &ts,&err);
    printf("get after pend\n");
    fixeddelay(500);
    BSP_LED_Off(0);
    OSFlagPost(&readDone,0x07,OS_OPT_POST_FLAG_CLR,&err);
  }


}
