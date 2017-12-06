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
#define NUM_BUDDIES 6
#define BUDDY_PRIO 3
#define BARTENDER_PRIO 3
#define KEG_SIZE 7


static  OS_TCB   AppTaskStartTCB; 
static  OS_TCB   BuddyTaskTCB[NUM_BUDDIES]; 
static  OS_TCB   BartenderTaskTCB;

static  CPU_STK  AppTaskStartStk[APP_TASK_START_STK_SIZE];
static  CPU_STK  BuddyTaskStks[NUM_BUDDIES][DEFAULT_TASK_STK_SIZE];
static  CPU_STK  BartenderTaskStk[DEFAULT_TASK_STK_SIZE];

static char BuddyName[NUM_BUDDIES][16]={'a','b','c','d','e','f'};

static int level; /* only for display purposes */


OS_SEM is_empty;
OS_SEM ready_drink;

OS_MUTEX access;
OS_MUTEX access1;

/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

static  void  AppTaskStart  (void *p_arg);
static  void  BartenderTask  (void *p_arg);
static  void  BuddyTask  (void *p_arg);


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

static void display()
{
  if (level > 7) level = 7;
  (level & 0x1) ? BSP_LED_On(1) : BSP_LED_Off(1);
  (level & 0x2) ? BSP_LED_On(2) : BSP_LED_Off(2);
  (level & 0x4) ? BSP_LED_On(3) : BSP_LED_Off(3);
}


static void delay()
{
  OS_ERR err;
  float delay = ((float)rand()) * 500 / RAND_MAX;
  OSTimeDlyHMSM(0, 0, 0, (int)delay, 
                OS_OPT_TIME_HMSM_STRICT, 
                &err);
}

static void fixeddelay(int d)
{
    OS_ERR err;
    OSTimeDlyHMSM(0, 0, 0, d, 
                      OS_OPT_TIME_HMSM_STRICT, 
                      &err);
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

    level = KEG_SIZE;
    display();

    OSSemCreate(&is_empty,"empty",0,&err);
    OSSemCreate(&ready_drink,"ready to drink",1,&err);
    
    OSMutexCreate(&access,"Access",&err);
    OSMutexCreate(&access1,"Access1",&err);

    
    for (int i = 0; i < NUM_BUDDIES; i++) {
      sprintf(BuddyName[i],"Buddy %d",i);
      OSTaskCreate((OS_TCB   *)&BuddyTaskTCB[i],
                 (CPU_CHAR   *)BuddyName[i],
                 (OS_TASK_PTR )BuddyTask, 
                 (void       *)i,
                 (OS_PRIO     )BUDDY_PRIO, 
                 (CPU_STK    *)&BuddyTaskStks[i][0],
                 (CPU_STK_SIZE)DEFAULT_TASK_STK_SIZE / 10,
                 (CPU_STK_SIZE)DEFAULT_TASK_STK_SIZE,
                 (OS_MSG_QTY  )0,
                 (OS_TICK     )0,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);
    }
    OSTaskCreate((OS_TCB     *)&BartenderTaskTCB,
                 (CPU_CHAR   *)"Bartender",
                 (OS_TASK_PTR )BartenderTask, 
                 (void       *)0,
                 (OS_PRIO     )BARTENDER_PRIO,
                 (CPU_STK    *)&BartenderTaskStk[0],
                 (CPU_STK_SIZE)DEFAULT_TASK_STK_SIZE / 10,
                 (CPU_STK_SIZE)DEFAULT_TASK_STK_SIZE,
                 (OS_MSG_QTY  )0,
                 (OS_TICK     )0,
                 (void       *)0,
                 (OS_OPT      )(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
                 (OS_ERR     *)&err);

  


    OSTaskSuspend(0,&err);  

}


static  void  BartenderTask (void *p_arg)
{
    OS_ERR err;
    CPU_TS ts;
    
    while (DEF_TRUE) {   

      OSMutexPend(&access,0,OS_OPT_PEND_BLOCKING,&ts,&err);
      OSSemPend(&is_empty,0,OS_OPT_PEND_BLOCKING,&ts,&err); //the keg is empty
      level = KEG_SIZE;     ///fill the keg
      OSSemPost(&ready_drink, OS_OPT_POST_1,&err);  //tell anyone who is waiting is ok to drink now
      printf("show level in bartender side: %d\n",level );
      display();   //should play a 7 here
      fixeddelay(500);                                              /* in a real program never wait in a critical section! */
        
      OSMutexPost(&access,OS_OPT_POST_1,&err);
      }
      

}


static  void  BuddyTask (void *p_arg)
{
    OS_ERR err;
    CPU_TS ts;
    //int buddyno = (int)p_arg;   //what is this for

    while (DEF_TRUE) {

      OSSemPend(&ready_drink,0,OS_OPT_PEND_BLOCKING,&ts,&err); //so 1 at a time
      OSMutexPend(&access1,0,OS_OPT_PEND_BLOCKING,&ts,&err);

      display();
      delay();                                                        /* has the keg to himself and drinks */
                                                                      /* in a real program never wait in a critical section! */
      level--;      //one person eat 1 pint
      printf("show level: %d\n",level );
      if(level == 0){   //so if it is empty
        OSSemPost(&is_empty, OS_OPT_POST_1,&err);    //tell bartender it is empty
      }
      
      delay();                                                        /* not thirsty for a while */
      OSMutexPost(&access1,OS_OPT_POST_1,&err);
      OSSemPost(&ready_drink, OS_OPT_POST_1,&err);
      
    }
}

