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
#define NUM_SLAVES 3
#define SLAVE_PRIO 4
#define MASTER_PRIO 3

static  OS_TCB   AppTaskStartTCB;
static  OS_TCB   MasterTCB;
static  OS_TCB   SlaveTCB[NUM_SLAVES];

static  CPU_STK  AppTaskStartStk[APP_TASK_START_STK_SIZE];
static  CPU_STK  MasterStk[DEFAULT_TASK_STK_SIZE];
static  CPU_STK  SlaveStks[NUM_SLAVES][DEFAULT_TASK_STK_SIZE];

static char Names[NUM_SLAVES][16];

/* global variables for synchronization */
static OS_SEM master;
static OS_MUTEX mutex;
int newest;

/* function declarations */
void free_a_slave();
void submit_to_master(int taskno, int *infoPtr);

/* for checking */
#define TRACE_SIZE 10000
#define MASK 0x100

int trace[TRACE_SIZE];
int index = 0;
int passed = 0;


/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void  AppTaskStart  (void *p_arg);
static  void  Slave  (void *p_arg);
static  void  Master  (void *p_arg);
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


    OSSemCreate(&master,"Master",0,&err);

     OSTaskCreate((OS_TCB   *)&MasterTCB,
                 (CPU_CHAR   *)"Master Task",
                 (OS_TASK_PTR )Master,
                 (void       *)0,
                 (OS_PRIO     )MASTER_PRIO,
                 (CPU_STK    *)MasterStk,
                 (CPU_STK_SIZE)DEFAULT_TASK_STK_SIZE / 10,
                 (CPU_STK_SIZE)DEFAULT_TASK_STK_SIZE,
                 (OS_MSG_QTY  )0,
                 (OS_TICK     )0,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);

    for (int i = 0; i < NUM_SLAVES; i++) {
      sprintf(Names[i],"Slave %d",i+1);
      OSTaskCreate((OS_TCB   *)&SlaveTCB[i],
                 (CPU_CHAR   *)Names[i],
                 (OS_TASK_PTR )Slave,
                 (void       *)(i+1),
                 (OS_PRIO     )SLAVE_PRIO,
                 (CPU_STK    *)&SlaveStks[i][0],
                 (CPU_STK_SIZE)DEFAULT_TASK_STK_SIZE / 10,
                 (CPU_STK_SIZE)DEFAULT_TASK_STK_SIZE,
                 (OS_MSG_QTY  )0,
                 (OS_TICK     )0,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);
      OSTaskSemSet((OS_TCB   *)&SlaveTCB[i],0,(OS_ERR     *)&err);

    }


    /* code to check the execution trace below */

    int stack[NUM_SLAVES];
    int sp = 0;
    int rd = 0;

    while (DEF_TRUE) {
        OSTimeDlyHMSM(0, 0, 0, 300,
                      OS_OPT_TIME_HMSM_STRICT,
                      &err);
        if(index >= TRACE_SIZE) {
          while (rd < index) {
            BSP_LED_On(0);
            if ((trace[rd] & MASK) == 0) {
              stack[sp++] = trace[rd++];
            }
            else {
              if(stack[--sp] != (trace[rd++] - MASK)) {
                BSP_LED_On(0);
                OSTaskSuspend(&MasterTCB,&err);
                OSTaskSuspend(0,&err);
              }
            }
            if( sp < 0 || sp > NUM_SLAVES) {
                BSP_LED_Off(0);
                OSTaskSuspend(&MasterTCB,&err);
                OSTaskSuspend(0,&err);
            }
          }
          passed = 1;
          BSP_LED_Toggle(0);
        }
    }
}

void delay(int random,int fixed)
{
  OS_ERR err;
  float delay = ((float)rand()) * random / RAND_MAX + fixed;
  OSTimeDlyHMSM(0, 0, 0, (int)delay,
                OS_OPT_TIME_HMSM_NON_STRICT,
                &err);
}

void fixeddelay(int d)
{
    OS_ERR err;
    OSTimeDlyHMSM(0, 0, 0, d,
                      OS_OPT_TIME_HMSM_NON_STRICT,
                      &err);
}

/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/*                 DO NOT MODIFY THE TASK BODIES BELOW                       */
/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */

static  void  Slave (void *p_arg)
{
    int taskno = (int)p_arg;
    int info;

    while (DEF_TRUE) {
       if(index <TRACE_SIZE)
          trace[index++] = taskno;

       if(!passed)
         BSP_LED_On(taskno);
       submit_to_master(taskno,&info);
       if(!passed)
         BSP_LED_Off(taskno);

       if(index <TRACE_SIZE)
          trace[index++] = MASK + taskno;

       delay(5,1);  /* you can play with the timing */
    }
}

static  void  Master (void *p_arg)
{
    while (DEF_TRUE) {
      free_a_slave();
      fixeddelay(2);  /* you can play with the timing */
    }
}

/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/*                 DO NOT MODIFY THE TASK BODIES ABOVE                       */
/* XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */

void free_a_slave()
{
    CPU_TS ts;
    OS_ERR err;

    OSSemPend(&master,0,OS_OPT_PEND_BLOCKING,&ts,&err);
    OSTaskSemPost(&SlaveTCB[newest-1], OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED,&err);
}

void submit_to_master(int taskno, int *infoPtr)
{
    CPU_TS ts;
    OS_ERR err;

    OSMutexPend(&mutex,0,OS_OPT_PEND_BLOCKING,&ts,&err);
      *infoPtr = newest; // get the value of : newest
      newest = taskno;
    OSMutexPost(&mutex,OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED,&err);
    //when task is ready tell master, excute that specifc task
    OSSemPost(&master,OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED,&err);
    OSTaskSemPend(0,OS_OPT_PEND_BLOCKING,&ts,&err);

    OSMutexPend(&mutex,0,OS_OPT_PEND_BLOCKING,&ts,&err);
      newest = *infoPtr; //go to the last one
    OSMutexPost(&mutex,OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED,&err);
}
