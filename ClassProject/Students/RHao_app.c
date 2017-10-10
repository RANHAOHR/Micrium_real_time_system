#include <includes.h>
#include <string.h>
#include "BattlefieldLib.h"

/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/
#define PI 3.14159265358979323846

int x_Low = MINX + 200;
int y_Low = MINY + 200;
int x_High = MAXX - 200;
int y_High = MAXY -  200;

float danger_range = 20.0;

bool tank_reverse; //indicating the reverse
int tank_ID; //currentl 1

static OS_SEM done_control;
static OS_SEM done_sensing;
static OS_SEM done_danger_check;

typedef struct {int ID; bool status; bool reverse; Point position; float heading; float speed; int direction;} tank_info;
typedef struct {float steer; float acceleration;} control_info;
typedef struct {bool stuck; bool danger;} avoid_danger;
typedef struct {int num; int ID[3]; int abs_bear[3]; int direction[3]; int danger_distance[3]; int danger_bear[3]; float delta_bear[3]; float old_bear[3];} missile_info; // indicate the missiles

typedef struct {tank_info myTank; avoid_danger danger; RadarData radar; control_info control; missile_info missiles;} tank_data;

Tank* tank_target;
tank_data total_data;

/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

/*
 * direction_determine function
 */
int direction_determine(float angle){

  int direction = 2*angle/PI;

  return direction;
}

/*
 * compute the bearing angle
 */
float abs_bearing(float angle){
  float bearing;

  bearing = angle;

  // if ( PI/2 < bearing <= PI){
  //     bearing = PI - angle;
  // }
   if (bearing > 3*PI/2){
     bearing = 2*PI - angle;
   }
   else if (PI< bearing <=3*PI/2){
     bearing = angle - PI;
   }

  return bearing;
}

/*
 * chek the distance danger, return the level of distance danger
 */
int distance_danger_check(float missile_distance){
 int level;

 if(missile_distance > 30*danger_range){
    level = 0; // no dangerous
 }
 if ( 20*danger_range < missile_distance <= 30*danger_range){
    level = 1; // low dangerous
 }
 if (10*danger_range < missile_distance <= 20*danger_range){
    level = 2; // medium dangerous
 }
 if (missile_distance <= 10*danger_range){
    level = 3; // high dangerous
 }

  return level;
}

/*
 * chek the bearing danger, return the level of bearing danger
 */
int bearing_danger_check (float missile_abs_bearing){
 int level;

 if (missile_abs_bearing > STEER_LIMIT){
  level = 0; // no dangerous
 }
 if ( 0.5 < missile_abs_bearing <= STEER_LIMIT){
  level = 1; // low dangerous
 }
 if (0.2 < missile_abs_bearing <= 0.5) {
  level = 2; // medium dangerous
 }
 if (missile_abs_bearing <= 0.2){
  level = 3; // high dangerous
 }
 return level;
}

/*
 * contorller, control the tank to avoid stuck
 */
control_info avoid_stuck(tank_data INFO){
  control_info command;
  int x = INFO.myTank.position.x;
  int y = INFO.myTank.position.y;
  int d = INFO.myTank.direction;


  command.acceleration = 1;

  return command;

}

/*
 * contorller, control the tank to avoid missile
 */
control_info avoid_missile (tank_data myTank){
    control_info command;

    command.steer = 0;
    command.acceleration = 1;
  return command;
 }

/**************************
 * Control task function
 ***************************/
tank_data controlling(Tank* tankname, tank_data myTank){
  control_info command_missle, command_stuck, command;

  command_missle = avoid_missile(myTank);
  command_stuck = avoid_stuck(myTank);

  if (myTank.danger.danger){
     command = command_missle;
  }
  else if (myTank.danger.stuck){
     command = command_stuck;
  }

  myTank.control.acceleration = command.acceleration;
  myTank.control.steer = command.steer;

  set_steering(tankname, myTank.control.steer);
  accelerate(tankname, myTank.control.acceleration);

  return myTank;
 }

/**************************
 * Sensing task function
 ***************************/
tank_data sensing(Tank* tankname){

  tank_data sensing_info;

  //tank_info
  sensing_info.myTank.status = poll_radar(tankname, &sensing_info.radar);
  sensing_info.myTank.position = get_position(tankname);
  sensing_info.myTank.heading = get_heading (tankname);
  sensing_info.myTank.speed = get_speed(tankname);
  sensing_info.myTank.direction = direction_determine(sensing_info.myTank.heading);

  sensing_info.danger.stuck = is_stuck(tankname);
  sensing_info.missiles.num = sensing_info.radar.numMissiles;  //detected number of missiles

  if (tankname == tank_target){
    sensing_info.myTank.ID = tank_ID;
  }

  //reset missile and other tanks direction and absolute bearing
  for (int i = 0; i< 3; i++){
    sensing_info.missiles.direction[i] = 0;
    sensing_info.missiles.abs_bear[i] = 0;
  }

  // read missile direction and absolute bearing
   for (int i = 0; i< sensing_info.missiles.num ; i++){
     sensing_info.missiles.direction[i] = direction_determine(sensing_info.radar.missileBearings[i]);
     sensing_info.missiles.abs_bear[i] = abs_bearing(sensing_info.radar.missileBearings[i]);
   }

  // check the risk of getting stuck
  if ((sensing_info.myTank.position.x < x_Low) || (sensing_info.myTank.position.x > x_High) ||  (sensing_info.myTank.position.y < y_Low) || (sensing_info.myTank.position.y > y_High))  {
     sensing_info.danger.stuck = TRUE;
  }
  else sensing_info.danger.stuck = FALSE;

  /*
   * check danger, reset danger check
   */
  for(int i = 0; i<3;i++){
    sensing_info.missiles.danger_distance[i] = 0;
    sensing_info.missiles.danger_bear[i] = 0;
  }

  // check danger from missiles
  for (int i = 0; i <sensing_info.missiles.num;i++){
  sensing_info.missiles.danger_distance[i] = distance_danger_check(sensing_info.radar.missileDistances[i]);
  sensing_info.missiles.danger_bear[i] = bearing_danger_check(sensing_info.missiles.abs_bear[i]);

  }

 return sensing_info;
}

/****************
 * The task functions, including the sensing information receiving, and danger check and control task
 ****************/

static void sensing_task(void* p_arg){
    CPU_TS ts;
    OS_ERR err;

    while (DEF_TRUE) {

       OSSemPend(&done_control,0,OS_OPT_PEND_BLOCKING,&ts,&err);

       total_data = sensing(tank_target);
       BSP_LED_Off(0);

       OSSemPost(&done_sensing,OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED,&err);
    }

}

//after et the sensing info, make sure if there is no danger
static void dangeralarm_task(void* p_arg){
  CPU_TS ts;
  OS_ERR err;

  while (DEF_TRUE) {

       OSSemPend(&done_sensing,0,OS_OPT_PEND_BLOCKING,&ts,&err);

       if (total_data.danger.danger){
         int danger_num = total_data.missiles.num;
         BSP_LED_On(danger_num);
       }
       else BSP_LED_Off(0);

       OSSemPost(&done_danger_check, OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED,&err);
    }
}

// Student initialization function
static void control_task(void* p_arg){
  CPU_TS ts;
  OS_ERR err;

  while (DEF_TRUE) {
       OSSemPend(&done_danger_check,0,OS_OPT_PEND_BLOCKING,&ts,&err);  //when there are no danger

        if (is_stuck(tank_target)){  //if the tank is stuck
           tank_reverse = TRUE;
           set_reverse(tank_target,tank_reverse);       //reverse the tank
       }
      total_data.myTank.reverse = tank_reverse;
      tank_reverse = FALSE;

       total_data = controlling(tank_target, total_data);

       OSSemPost(&done_control,OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED,&err);
    }

}

void RHao_init(char teamNumber)
{
    // Initialize any system constructs (such as mutexes) here
     OS_ERR err;
     OSSemCreate(&done_control,"done_control",1,&err);
     OSSemCreate(&done_danger_check,"done_danger_check",0,&err);
     OSSemCreate(&done_sensing,"done_sensing",0,&err);

     BSP_Init();
    // Perform any user-required initializations here
    tank_target = create_tank(1,"target");
    create_turret(tank_target);
    tank_ID = initialize_tank(tank_target);
    // Register user functions here

    register_user(control_task,5,0);
    register_user(sensing_task,5,0);
    register_user(dangeralarm_task,5,0);
}
