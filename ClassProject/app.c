/*
*********************************************************************************************************
*
*                                          APPLICATION CODE
*
*                                      ST Microelectronics STM32
*                                               on the
*                                      Micrium uC-Eval-STM32F107
*                                          Evaluation Board
*
* Filename      : app.c
* Version       : V2.00
* Programmer(s) : Will Hedgecock
*********************************************************************************************************
* Note(s)       : (1) Assumes the following versions (or more recent) of software modules are included in
*                     the project build :
*
*                     (a) uC/OS-III V3.01.3
*                     (b) uC/LIB    V1.31
*                     (c) uC/CPU    V1.25
*********************************************************************************************************
*/

#include <includes.h>
#include "BattlefieldLib.h"

extern void RHao_init(char teamNumber);

// Entry point - main function
int main(void)
{
    OS_ERR err;

    BSP_IntDisAll();      // Disable all interrupts
    OSInit(&err);         // Initialize uC/OS-III

    // Initialize student functions here
    RHao_init(1);

    // After initializing user functions, initialize framework
    initialize(FALSE);

    // Start multitasking and give control to uC/OS-III
    OSStart(&err);
}
