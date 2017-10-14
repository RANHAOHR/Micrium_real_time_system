#include <includes.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "BattlefieldLib.h"

/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/
#define M_PI 3.14159265358979323846

float stuck_radius = 350;
float danger_range = 300.0;

bool tank_reverse; //indicating the reverse
int tank_ID; //currentl 1

static OS_SEM done_control;
static OS_SEM done_sensing;
static OS_SEM done_danger_check;

// maybe not need the directions
typedef struct {int ID; bool status; bool reverse; Point position; float heading; float speed; int region;} tankInfo;
typedef struct {float steer; float acceleration;} controlVector;
typedef struct {bool stuck; bool missile;} dangerInfo;
typedef struct {int num; int ID[3];float relative_distance[3]; float relative_bear[3]; int missile_region; int type[3];} missileInfo; // indicate the missiles
typedef struct {tankInfo myTank; dangerInfo danger; RadarData radar; controlVector control; missileInfo missiles;} tank_data;

Tank* tank_target;
tank_data total_data; //necessary datas of the current tank

float front_limit_pos_1 = M_PI /8;
float front_limit_neg_1 = -1* M_PI /8;
float front_limit_pos_2 = M_PI /6;
float front_limit_neg_2 = -1* M_PI /6;

float back_limit_pos_1 = 7 * M_PI / 8;
float back_limit_neg_1 = -7 * M_PI / 8;
float back_limit_pos_2 = 5* M_PI /6;
float back_limit_neg_2 = -5* M_PI /6;

int mid_x = MINX + (MAXX - MINX) /2;
int mid_y = MINY + (MAXY - MINY) /2;

/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

int indicatedRegion(float dx, float dy){

  int region;
  if(dx > 0 && dy >0){
    region = 1;
  }
  else if(dx < 0 && dy >0){
    region = 2;
  }
  else if(dx < 0 && dy <0){
    region = 3;
  }
  else if(dx > 0 && dy <0){
    region = 4;
  }
  return region;
}
/*
 * contorller, control the tank to avoid stuck
 */

controlVector regionStuckAvoidance(tank_data myTank){
  controlVector command;

  int x = myTank.myTank.position.x;
  int y = myTank.myTank.position.y;
  int region = myTank.myTank.region;
  float heading = myTank.myTank.heading;

  int sensitivity = heading / (M_PI /2);

  //positive is left, negative is right
  float minimal_steer = 0.2;

    if (region == 1)  {
      if (sensitivity == 0) command.steer = -minimal_steer;
      if (sensitivity == 1) command.steer = -2*minimal_steer ;
      if (sensitivity == 2) command.steer = 2*minimal_steer;
      if (sensitivity == 3) command.steer = minimal_steer;
    }
    if (region == 2){
      if (sensitivity == 0) command.steer = -2*minimal_steer;
      if (sensitivity == 1) command.steer =  2*minimal_steer;
      if (sensitivity == 2) command.steer =    minimal_steer;
      if (sensitivity == 3) command.steer = -1*minimal_steer;
    }
    if (region == 3) {
      if (sensitivity == 0) command.steer = minimal_steer;
      if (sensitivity == 1) ;
      if (sensitivity == 2) ;
      if (sensitivity == 3) command.steer = -2*minimal_steer;
    }
     if (region == 4) {
      if (sensitivity == 0) ;
      if (sensitivity == 1) ;
      if (sensitivity == 2) command.steer = -2*minimal_steer;
      if (sensitivity == 3) command.steer = minimal_steer;
    }
  command.acceleration = 1;

  return command;

}

int getTankType(float missile_bearing){

    float missle_heading;
    int tank_type;

    if (missile_bearing > M_PI) {
      missle_heading = missile_bearing - 2*M_PI;
    }

    if ( (0 <= missle_heading <= front_limit_pos_1 )|| (-1*M_PI <= missle_heading <= back_limit_neg_1)) {
        tank_type = 1;
    }
    else if ((front_limit_neg_1 <= missle_heading < 0) || (back_limit_pos_1 <= missle_heading < M_PI)) {
        tank_type = 2;
    }
    else if ((front_limit_pos_1 < missle_heading <= front_limit_pos_2 )|| (back_limit_neg_1 < missle_heading <= back_limit_neg_2)) {
        tank_type = 3;
    }
    else if ((front_limit_neg_2 < missle_heading < front_limit_neg_1) || (back_limit_pos_2 < missle_heading < back_limit_pos_1)) {
      tank_type = 4;
    }
    else{
      tank_type = 5;
    }

   return tank_type;
}
/*
 * contorller, control the tank to avoid missile
 */
 float singleMissileSteer(float tank_type){
   float cmd_steer;
   if (tank_type == 1) {
      cmd_steer = 0.6;
   }else if (tank_type == 2) {
     cmd_steer = -0.6;
   }else if (tank_type == 3) {
     cmd_steer = 0.3;
   }else if (tank_type == 4) {
     cmd_steer = -0.3;
   }
   else if (tank_type == 5) {
     cmd_steer = 0.0;
   }
   return cmd_steer;
 }
controlVector dwaMissileAvoid(tank_data myTank){
    controlVector command;

    float cmd_steer = 0.0;
    float cmd_vel = 1.0;
    int num_missles = myTank.missiles.num;

    float tank_type;
    float missle_1;
    float missle_2;
    float missle_3;
    float ave_diret;
    int ave_type;

    switch(num_missles){
      case 1:
          tank_type = myTank.missiles.type[0];
          cmd_steer = singleMissileSteer(tank_type);
          cmd_vel = 1.0;
          break;
      case 2:
          missle_1 = myTank.missiles.relative_bear[0];
          missle_2 = myTank.missiles.relative_bear[1];
          ave_diret = (missle_1 + missle_2) / 2;
          ave_type =  getTankType(ave_diret);
          cmd_steer = singleMissileSteer(ave_type);
          cmd_vel = 1.0;
          break;
      case 3:
          missle_1 = myTank.missiles.relative_bear[0];
          missle_2 = myTank.missiles.relative_bear[1];
          missle_3 = myTank.missiles.relative_bear[2];
          ave_diret = (missle_1 + missle_2 + missle_3) / 3;
          ave_type =  getTankType(ave_diret);
          cmd_steer = singleMissileSteer(ave_type);
          cmd_vel = 1.5;
          break;
    }

    if(cmd_steer > STEER_LIMIT){
      cmd_steer = STEER_LIMIT;
    }
    if(cmd_steer < -1*STEER_LIMIT){
      cmd_steer = -1*STEER_LIMIT;
    }

    command.steer = cmd_steer;
    command.acceleration = cmd_vel;
    //printf("command.steer %f\n", command.steer);
  return command;
 }

/**************************
 * Control task function
 ***************************/
tank_data controlling(Tank* tankName, tank_data myTank){
  controlVector command;
  if (myTank.danger.missile){
    //printf("in missle command\n");
     command = dwaMissileAvoid(myTank);
  }
 else if (myTank.danger.stuck){
     //printf("in stuck command\n");
    command = regionStuckAvoidance(myTank);
 }
 else {
    command.acceleration = 1.0;
    command.steer = 0.0;
 }

  myTank.control.acceleration = command.acceleration;
  myTank.control.steer = command.steer;

  set_steering(tankName, myTank.control.steer);
  accelerate(tankName, myTank.control.acceleration);

  return myTank;
 }

/*****************************************************
 * Sensing task function
 *****************************************************/
tank_data sensing(Tank* myTank){

  tank_data sensing_info;
  sensing_info.myTank.status = poll_radar(myTank, &sensing_info.radar); //get the radar
  //get all the tankInfo
  sensing_info.myTank.position = get_position(myTank);
  sensing_info.myTank.heading = get_heading (myTank);
  sensing_info.myTank.speed = get_speed(myTank);
  // sensing_info.danger.stuck = is_stuck(myTank);
  sensing_info.missiles.num = sensing_info.radar.numMissiles;  //detected number of missiles
  //printf("sensing_info.radar.numMissiles is : %d \n", sensing_info.radar.numMissiles);

  /* first check the risk of getting stuck */
  float dx = mid_x - sensing_info.myTank.position.x;
  float dy = mid_y - sensing_info.myTank.position.y;
  float radius = sqrt(pow(dx,2) + pow(dy,2));
  if(radius > stuck_radius ){
    sensing_info.danger.stuck = TRUE;
  }
  else{
    sensing_info.danger.stuck = FALSE;
  }

  sensing_info.myTank.region = indicatedRegion(dx,dy);

  if (sensing_info.missiles.num > 0) {
    sensing_info.danger.missile = TRUE;
  }else {
    sensing_info.danger.missile = FALSE;
  }

   /* read missile direction and bearing */
   for (int i = 0; i< sensing_info.missiles.num; i++){
     if(sensing_info.radar.missileDistances[i] < danger_range){
        sensing_info.missiles.relative_distance[i] = sensing_info.radar.missileDistances[i];
        sensing_info.missiles.relative_bear[i] = sensing_info.radar.missileBearings[i];
        sensing_info.missiles.type[i] = getTankType(sensing_info.missiles.relative_bear[i]);
     }

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

       total_data.myTank.ID = tank_ID;  //first set the ID
       //each time reset the missiles info
       for (int i = 0; i< 3; i++){
         total_data.missiles.relative_distance[i] = 10000;  //starting without detecting
         total_data.missiles.relative_bear[i] = 10000;
         total_data.missiles.type[i] = 5; //pretend it is not there
       }

       total_data = sensing(tank_target);
       BSP_LED_Off(0);

       OSSemPost(&done_sensing, OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED,&err);
    }

}

//after et the sensing info, make sure if there is no danger
static void dangeralarm_task(void* p_arg){
  CPU_TS ts;
  OS_ERR err;

  while (DEF_TRUE) {

       OSSemPend(&done_sensing,0,OS_OPT_PEND_BLOCKING,&ts,&err);

       if (total_data.danger.missile){
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
           tank_reverse = !tank_reverse;
           set_reverse(tank_target,tank_reverse);       //reverse the tank
       }
        else{
          total_data.myTank.reverse = tank_reverse;
        }
      total_data = controlling(tank_target, total_data);

       OSSemPost(&done_control, OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED,&err);
    }

}

void RHao_init(char teamNumber)
{
    // Initialize any system constructs (such as mutexes) here
     OS_ERR err;
     OSSemCreate(&done_control,"done_control",1,&err);  // start from a control task
     OSSemCreate(&done_sensing,"done_sensing",0,&err); //get the sensing info
     OSSemCreate(&done_danger_check,"done_danger_check",0,&err); //check the dnager then decide

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
