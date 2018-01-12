# Multiple Tank Missile-Avoidance and Shooting

# This package is for Real-Time System Project II.
# Dependencies: IAR Workbench, µC/OS-III(µC-Eval-STM32F107)

This project controls over two tanks which will be under fire from at most six missiles at any given time. The tanks are equipped with the same sensors as in single tank project; however, you are now responsible for avoiding missiles and up to two enemy tanks.

Instead of using a hit counter to measure how well your tanks are performing, the tanks will have health meters, initially set to 100%. Additionally, the tanks may be equipped with turrets, allowing you to fire at enemy tanks.

The objective is to ensure that the tanks remain undamaged and undestroyed longer than the enemy tanks which are controlled by the enemy's algorithm (other students' algorithm).

- ClassProject.eww and associated directories: These files comprise the IAR workspace and environment needed to implement your avoidance system.
- ClassProject/Students/RHao_app.c and enemy_app: These files contain the main control and sensing functions. Add these files to the ‘ClassProject→APP→Students’ group.
- BattlefieldGUI.jar: This Java program is used to provide a visual reference of what is happening during the execution of your program. It requires that you connect your computer to the evaluation board using a serial (RS-232) port or a USB-to-Serial converter. (This is necessary for your program to run, so try to
locate such a cable as soon as possible.)
- A single tank cannot fire more quickly than 1 missile every 1.5 seconds, and the tank can have no more than two active missiles on the battlefield at any time. (Note that attempting to violate these rules will not cause blocking, but rather will fail and cause the fire() function to return FALSE).
