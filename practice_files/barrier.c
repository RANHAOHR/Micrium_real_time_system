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

#define STK_SIZE 256
#define TASK_PRIO 3
#define TASK_PRIO 3

static  OS_TCB   AppTaskStartTCB;
static  OS_TCB   Task1TCB;
static  OS_TCB   Task2TCB;


static  CPU_STK  AppTaskStartStk[APP_TASK_START_STK_SIZE];
static  CPU_STK  Task1Stk[STK_SIZE];
static  CPU_STK  Task2Stk[STK_SIZE];

OS_SEM sem1;
OS_SEM sem2;


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void  AppTaskStart  (void *p_arg);
static  void  Task1  (void *p_arg);
static  void  Task2  (void *p_arg);
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
                 (OS_PRIO     )3,
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
    BSP_LED_Off(0);

    OSSemCreate(&sem1,"1",0,&err);
    OSSemCreate(&sem2,"2",0,&err);

    OSTaskCreate((OS_TCB     *)&Task1TCB,
                 (CPU_CHAR   *)"Task #1",
                 (OS_TASK_PTR )Task1,
                 (void       *)0,
                 (OS_PRIO     )TASK_PRIO,
                 (CPU_STK    *)&Task1Stk[0],
                 (CPU_STK_SIZE)STK_SIZE / 10,
                 (CPU_STK_SIZE)STK_SIZE,
                 (OS_MSG_QTY  )0,
                 (OS_TICK     )0,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);
    OSTaskCreate((OS_TCB     *)&Task2TCB,
                 (CPU_CHAR   *)"Task #2",
                 (OS_TASK_PTR )Task2,
                 (void       *)0,
                 (OS_PRIO     )TASK_PRIO,
                 (CPU_STK    *)&Task2Stk[0],
                 (CPU_STK_SIZE)STK_SIZE / 10,
                 (CPU_STK_SIZE)STK_SIZE,
                 (OS_MSG_QTY  )0,
                 (OS_TICK     )0,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);

  OSTaskDel(0,&err);
}

void delay()
{
    OS_ERR err;

    float dly = ((float)rand()) * 500 / RAND_MAX + 200;
    OSTimeDlyHMSM(0, 0, 0, (int)dly,
                      OS_OPT_TIME_HMSM_STRICT,
                      &err);
}

void fixeddelay()
{
    OS_ERR err;
    OSTimeDlyHMSM(0, 0, 0, 500,
                      OS_OPT_TIME_HMSM_STRICT,
                      &err);
}

static  void  Task1 (void *p_arg)
{
  OS_ERR err;
  CPU_TS ts;

    while (DEF_TRUE) {
      delay();                                    /* do some work */
      BSP_LED_On(1);                              /* rendezvous */
      OSSemPost(&sem1,OS_OPT_POST_1,&err);        /* notify the other guy */
      OSSemPend(&sem2,0,
                OS_OPT_PEND_BLOCKING,&ts,&err);   /* wait for the other guy */
      fixeddelay();                               /* for visualization only */
      BSP_LED_Off(1);                             /* just after rendezvous */
      delay();                                    /* do something */
    }
}


static  void  Task2 (void *p_arg)
{
  OS_ERR err;
  CPU_TS ts;

    while (DEF_TRUE) {
      delay();
      BSP_LED_On(2);
      OSSemPost(&sem2,OS_OPT_POST_1,&err);
      OSSemPend(&sem1,0,OS_OPT_PEND_BLOCKING,&ts,&err);
      fixeddelay();
      BSP_LED_Off(2);
      delay();
    }
}
