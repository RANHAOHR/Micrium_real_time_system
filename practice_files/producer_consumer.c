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

#define BUFFER_TASK_STK_SIZE 256
#define BUFFER_SIZE 7
#define PRODUCER_PRIO 3
#define CONSUMER_PRIO 3

static  OS_TCB   AppTaskStartTCB;
static  OS_TCB   Prod1TaskTCB;
static  OS_TCB   Prod2TaskTCB;
static  OS_TCB   Prod3TaskTCB;
static  OS_TCB   Cons1TaskTCB;
static  OS_TCB   Cons2TaskTCB;
static  OS_TCB   Cons3TaskTCB;

static  CPU_STK  AppTaskStartStk[APP_TASK_START_STK_SIZE];
static  CPU_STK  Prod1TaskStk[BUFFER_TASK_STK_SIZE];
static  CPU_STK  Prod2TaskStk[BUFFER_TASK_STK_SIZE];
static  CPU_STK  Prod3TaskStk[BUFFER_TASK_STK_SIZE];
static  CPU_STK  Cons1TaskStk[BUFFER_TASK_STK_SIZE];
static  CPU_STK  Cons2TaskStk[BUFFER_TASK_STK_SIZE];
static  CPU_STK  Cons3TaskStk[BUFFER_TASK_STK_SIZE];

static int buffer[BUFFER_SIZE];
static int rdIndex = 0;
static int wrIndex = 0;

OS_SEM full;  /* the sem pointer, has to be global */
OS_SEM empty;
OS_SEM access;

/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void  AppTaskStart  (void *p_arg);
static  void  ProducerTask  (void *p_arg);
static  void  ConsumerTask  (void *p_arg);

static void produce();
static int consume();
static void display(int num);

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

    OSSemCreate(&full,"Full",BUFFER_SIZE,&err); /* counting semaphore */
    OSSemCreate(&empty,"Empty",0,&err); /* If using a semaphore as a signaling mechanism, you should set the initial value to 0. */
    OSSemCreate(&access,"Access",1,&err);

    OSTaskCreate((OS_TCB     *)&Cons1TaskTCB,
                 (CPU_CHAR   *)"Consumer #1",
                 (OS_TASK_PTR )ConsumerTask,
                 (void       *)0,
                 (OS_PRIO     )CONSUMER_PRIO,
                 (CPU_STK    *)&Cons1TaskStk[0],
                 (CPU_STK_SIZE)BUFFER_TASK_STK_SIZE / 10,
                 (CPU_STK_SIZE)BUFFER_TASK_STK_SIZE,
                 (OS_MSG_QTY  )0,
                 (OS_TICK     )0,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);
    OSTaskCreate((OS_TCB     *)&Cons2TaskTCB,
                 (CPU_CHAR   *)"Consumer #2",
                 (OS_TASK_PTR )ConsumerTask,
                 (void       *)0,
                 (OS_PRIO     )CONSUMER_PRIO,
                 (CPU_STK    *)&Cons2TaskStk[0],
                 (CPU_STK_SIZE)BUFFER_TASK_STK_SIZE / 10,
                 (CPU_STK_SIZE)BUFFER_TASK_STK_SIZE,
                 (OS_MSG_QTY  )0,
                 (OS_TICK     )0,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);
     OSTaskCreate((OS_TCB     *)&Cons3TaskTCB,
                 (CPU_CHAR   *)"Consumer #3",
                 (OS_TASK_PTR )ConsumerTask,
                 (void       *)0,
                 (OS_PRIO     )CONSUMER_PRIO,
                 (CPU_STK    *)&Cons3TaskStk[0],
                 (CPU_STK_SIZE)BUFFER_TASK_STK_SIZE / 10,
                 (CPU_STK_SIZE)BUFFER_TASK_STK_SIZE,
                 (OS_MSG_QTY  )0,
                 (OS_TICK     )0,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);
    OSTaskCreate((OS_TCB     *)&Prod1TaskTCB,
                 (CPU_CHAR   *)"Producer #1",
                 (OS_TASK_PTR )ProducerTask,
                 (void       *)0,
                 (OS_PRIO     )PRODUCER_PRIO,
                 (CPU_STK    *)&Prod1TaskStk[0],
                 (CPU_STK_SIZE)BUFFER_TASK_STK_SIZE / 10,
                 (CPU_STK_SIZE)BUFFER_TASK_STK_SIZE,
                 (OS_MSG_QTY  )0,
                 (OS_TICK     )0,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);
    OSTaskCreate((OS_TCB     *)&Prod2TaskTCB,
                 (CPU_CHAR   *)"Producer #2",
                 (OS_TASK_PTR )ProducerTask,
                 (void       *)0,
                 (OS_PRIO     )PRODUCER_PRIO,
                 (CPU_STK    *)&Prod2TaskStk[0],
                 (CPU_STK_SIZE)BUFFER_TASK_STK_SIZE / 10,
                 (CPU_STK_SIZE)BUFFER_TASK_STK_SIZE,
                 (OS_MSG_QTY  )0,
                 (OS_TICK     )0,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);
    OSTaskCreate((OS_TCB     *)&Prod3TaskTCB,
                 (CPU_CHAR   *)"Producer #3",
                 (OS_TASK_PTR )ProducerTask,
                 (void       *)0,
                 (OS_PRIO     )PRODUCER_PRIO,
                 (CPU_STK    *)&Prod3TaskStk[0],
                 (CPU_STK_SIZE)BUFFER_TASK_STK_SIZE / 10,
                 (CPU_STK_SIZE)BUFFER_TASK_STK_SIZE,
                 (OS_MSG_QTY  )0,
                 (OS_TICK     )0,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);

  OSTaskDel(0,&err);
}

static  void  ProducerTask (void *p_arg)
{
    OS_ERR  err;
    float delay;

    while (DEF_TRUE) {
        produce();
        delay =  500; //((float)rand()) * 500 / RAND_MAX;
        OSTimeDlyHMSM(0, 0, 0, (int)delay,
                      OS_OPT_TIME_HMSM_STRICT,
                      &err);
    }
}

static  void  ConsumerTask (void *p_arg)
{
    OS_ERR  err;
    float delay;

    while (DEF_TRUE) {
        consume();
        delay = 500; //((float)rand()) * 500 / RAND_MAX;
        OSTimeDlyHMSM(0, 0, 0, (int)delay,
                      OS_OPT_TIME_HMSM_STRICT,
                      &err);
    }
}

void produce()
{
  OS_ERR err;
  CPU_TS ts;
  int item = (int)(((float)rand() * 9 /(RAND_MAX+1))) + 1;

  OSSemPend(&full,0,OS_OPT_PEND_BLOCKING,&ts,&err);
  OSSemPend(&access,0,OS_OPT_PEND_BLOCKING,&ts,&err);

  buffer[wrIndex++] = item;
  if (wrIndex >= BUFFER_SIZE)
	wrIndex = 0;

  OSSemPost(&access,OS_OPT_POST_NONE,&err);
  OSSemPost(&empty,OS_OPT_POST_1,&err);

  display(empty.Ctr);
}

int consume()
{
  OS_ERR err;
  CPU_TS ts;
  int item;

  OSSemPend(&empty,0,OS_OPT_PEND_BLOCKING,&ts,&err);
  OSSemPend(&access,0,OS_OPT_PEND_BLOCKING,&ts,&err);

  item = buffer[rdIndex++];
  if (rdIndex >= BUFFER_SIZE)
	rdIndex = 0;

  OSSemPost(&access,OS_OPT_POST_NONE,&err);
  OSSemPost(&full,OS_OPT_POST_1,&err);

  display(empty.Ctr);
  return item;
}

void display(int num)
{
  if (num > 7) num = 7;
  (num & 0x1) ? BSP_LED_On(1) : BSP_LED_Off(1);
  (num & 0x2) ? BSP_LED_On(2) : BSP_LED_Off(2);
  (num & 0x4) ? BSP_LED_On(3) : BSP_LED_Off(3);
}
