# Single Tank Missile-Avoidance

# This package is for Real-Time System Project I.
# Dependencies: IAR Workbench, µC/OS-III(µC-Eval-STM32F107)

This project deals with the task of developing an automated missile-avoidance system. You will have control over one tank which will be under fire from at most three missiles at any given time. The tank is equipped with a GPS unit, allowing you to know the global position, as well as radar, enabling you to locate other objects (missiles in this part of the project) within the sensing range. The radar can inform
you of the number of missiles within its sensing range, as well as the distance and direction of each missile with respect to the center of the tank and its current orientation. The objective is to ensure that the tank is hit by as few missiles as possible.

- ClassProject.eww and associated directories: These files comprise the IAR workspace and environment needed to implement your avoidance system.
- ClassProject/Students/RHao_with_avoidStuck_app.c: This file contains the main control and sensing functions. This is the only file that need to be changed.
- BattlefieldGUI.jar: This Java program is used to provide a visual reference of what is happening during the execution of your program. It requires that you connect your computer to the evaluation board using a serial (RS-232) port or a USB-to-Serial converter. (This is necessary for your program to run, so try to
locate such a cable as soon as possible.)
