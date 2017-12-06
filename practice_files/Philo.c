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
#define NUM_PHILO 15
#define TASK_PRIO 3

static  OS_TCB   AppTaskStartTCB;
static  OS_TCB   TaskTCB[NUM_PHILO];

static  CPU_STK  AppTaskStartStk[APP_TASK_START_STK_SIZE];
static  CPU_STK  TaskStks[NUM_PHILO][DEFAULT_TASK_STK_SIZE];

static char Names[NUM_PHILO][16];
int count = 0;
int numfeasts[NUM_PHILO];

OS_SEM forks[NUM_PHILO];

/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void  AppTaskStart  (void *p_arg);
static  void  Task  (void *p_arg);

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

    for(int i = 0; i < NUM_PHILO; i++) {
      OSSemCreate(forks+i,"Fork",1,&err);
      numfeasts[i] = 0;
    }
    count = 0;

    for (int i = 0; i < NUM_PHILO; i++) {
      sprintf(Names[i],"Philo %d",i);
      OSTaskCreate((OS_TCB   *)&TaskTCB[i],
                 (CPU_CHAR   *)Names[i],
                 (OS_TASK_PTR )Task,
                 (void       *)(i),
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


    /* code to check trace below */

    BSP_LED_Off(0);
    OSTaskSuspend(0,&err);

}

void delay(int s,int ms)
{
  OS_ERR err;
  float delay = ((float)rand()) * ms / RAND_MAX;
  OSTimeDlyHMSM(0, 0, s, (int)delay,
                OS_OPT_TIME_HMSM_STRICT,
                &err);
}

static void display(int level)
{
  if (level > 7) level = 7;
  (level & 0x1) ? BSP_LED_On(1) : BSP_LED_Off(1);
  (level & 0x2) ? BSP_LED_On(2) : BSP_LED_Off(2);
  (level & 0x4) ? BSP_LED_On(3) : BSP_LED_Off(3);
}

static  void  Task (void *p_arg)
{
    CPU_TS ts;
    OS_ERR err;
    int philo = (int)p_arg;

    OS_SEM *fork1 = (philo == NUM_PHILO - 1) ? &forks[philo] : &forks[philo+1];
    OS_SEM *fork2 = (philo == NUM_PHILO - 1) ? &forks[0] : &forks[philo];

    while (DEF_TRUE) {

       delay(1,200);
       OSSemPend(fork1,0,OS_OPT_PEND_BLOCKING,&ts,&err);
       OSSemPend(fork2,0,OS_OPT_PEND_BLOCKING,&ts,&err);
       count++;
       display(count);
       numfeasts[philo]++;
       delay(0,800);
       count--;
       display(count);
       OSSemPost(fork2,OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED,&err);
       OSSemPost(fork1,OS_OPT_POST_1,&err);  //the trick?

    }
}
