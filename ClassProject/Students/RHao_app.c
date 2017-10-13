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
#define PI 3.14159265358979323846

 int x_Left = MINX + 250;
 int y_Left = MINY + 250;
 int x_Right = MAXX - 200;
 int y_Right = MAXY -  200;
//int x_Left = 350;
//int y_Left = 350;
//int x_Right = 550;
//int y_Right = 450;


float delta_t = 10;
float danger_range = 100.0;

bool tank_reverse; //indicating the reverse
int tank_ID; //currentl 1

static OS_SEM done_control;
static OS_SEM done_sensing;
static OS_SEM done_danger_check;

// maybe not need the directions
typedef struct {int ID; bool status; bool reverse; Point position; float heading; float speed; int region;} tankInfo;
typedef struct {float steer; float acceleration;} controlVector;
typedef struct {bool stuck; bool missile;} dangerInfo;
typedef struct {int num; int ID[3]; float relative_distance[3]; float relative_bear[3];} missileInfo; // indicate the missiles
typedef struct {tankInfo myTank; dangerInfo danger; RadarData radar; controlVector control; missileInfo missiles;} tank_data;

Tank* tank_target;
tank_data total_data; //necessary datas of the current tank

/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

int indicatedRegion(float angle){

  int region = 2*angle/PI;

  return region;
}

float getRelativeDirection(float missile_bearing){

  float relative_angle;
  relative_angle = missile_bearing;
  if (PI/2 < missile_bearing <= PI)
  {
    relative_angle = PI - missile_bearing;
  }
  else if(PI< missile_bearing <=3*PI/2){
    relative_angle = missile_bearing - PI;
  }
  else if(missile_bearing > 3*PI/2){
    relative_angle = 2*PI - missile_bearing;
  } //for -2pi because
  else if( -1* PI/2 <missile_bearing < 0 ){
    relative_angle = fabs(missile_bearing);
  }
  else if( -3* PI / 2 <missile_bearing < -1* PI/2 ){
    relative_angle = fabs(missile_bearing + PI);  //either way with the abs
  }
  else if( -2* PI <missile_bearing < -3* PI/2 ){
    relative_angle = fabs(missile_bearing + 2*PI);
  }

  return relative_angle;
}

/*
 * contorller, control the tank to avoid stuck
 */
controlVector stuckAvoidance(tank_data myTank){
  controlVector command;

  int x = myTank.myTank.position.x;
  int y = myTank.myTank.position.y;
  float direction = myTank.myTank.heading;

  float sensitive = 1;

  float delta_direction;
  //positive is left, negative is right
    if (x < x_Left)  {
      if (direction > PI) {
        delta_direction = direction -2* PI;
      }
      else{
        delta_direction = direction;
      }
      command.steer = -1* delta_direction / sensitive;
    }
    if (x > x_Right){
      command.steer = (PI - direction)  / sensitive;
    }

   if (y < y_Left) {
     command.steer = (3*PI/2 - direction)  / sensitive;
   }
   if (y > y_Right) {
     command.steer = (PI/2 - direction)  / sensitive;
   }

  command.acceleration = 0.3;

  return command;

}

controlVector regionStuckAvoidance(tank_data myTank){
  controlVector command;

  int x = myTank.myTank.position.x;
  int y = myTank.myTank.position.y;
  float region = myTank.myTank.region;

  //positive is left, negative is right
  float minimal_steer = 0.3;

    if (x < x_Left)  {
      if (region == 0) command.steer = -minimal_steer;
      if (region == 1) command.steer = -2*minimal_steer ;
      if (region == 2) command.steer = 2*minimal_steer;
      if (region == 3) command.steer = minimal_steer;
    }
    if (x > x_Right){
      if (region == 0) command.steer = 2*minimal_steer;
      if (region == 1) command.steer = minimal_steer;
      if (region == 2) command.steer = -minimal_steer;
      if (region == 3) command.steer = -2*minimal_steer;
    }
    if (y < y_Left) {
      if (region == 0) command.steer = -minimal_steer;
      if (region == 1) command.steer = -2*minimal_steer;
      if (region == 2) ;
      if (region == 3) ;
    }
     if (y > y_Right) {
      if (region == 0) ;
      if (region == 1) ;
      if (region == 2) command.steer = -1*minimal_steer;
      if (region == 3) command.steer = -2*minimal_steer;
    }
  command.acceleration = 0.3;

  return command;

}

/*
 * contorller, control the tank to avoid missile
 */
controlVector dwaMissileAvoid(tank_data myTank){
    controlVector command;

    int numSample = 30;

    float heading[30];
    float expectation[30];
    float acc_scale[30];
    float distance[30];
    float dealta_heading[30];

    float new_heading;
    float resolution_theta = 2*PI / numSample;
    float current_heading = myTank.myTank.heading;

    float direct_total = 0;
    float distance_total = 0;

    float vel = myTank.myTank.speed;

    float delta_distance;
    float current_distance;

    //initialize
    for (int i = 0; i < numSample; i++) {
      acc_scale[i] = 2.0;
      float delta_theta = i * resolution_theta;
      if( i > 24){
         dealta_heading[i] = delta_theta - 2*PI;
      }else{
        dealta_heading[i] = delta_theta;
      }
    }

    //starting the dwa
    for (int i = 0; i < numSample; i++) {
      // float delta_theta = i *  resolution_theta;
      // new_heading = current_heading - dealta_heading[i];
      // if (new_heading < 0.0) {
      //     new_heading = new_heading + 2*PI;
      // }

      float update_direction = 0.0;
      float update_distance = 0.0;

          for (int j = 0; j < myTank.missiles.num; j++) {
              float temp_direction;
              //update heading values
              if(i < 25){
                temp_direction = myTank.missiles.relative_bear[j] - dealta_heading[i];
              }
              else{
                temp_direction = myTank.missiles.relative_bear[j] - (dealta_heading[i] + 2*PI);
              }

              if (temp_direction > 2*PI) {
                  temp_direction = temp_direction - 2*PI;
              }
              float delta_direction = getRelativeDirection(temp_direction);
              update_direction += fabs(delta_direction);

              //update distance values
              delta_distance = (vel + acc_scale[i]) * delta_t;
              current_distance = myTank.missiles.relative_distance[j];
              float new_distance = pow(delta_distance, 2) + pow(current_distance,2) - 2*delta_distance*current_distance*cos(temp_direction);
              new_distance = sqrt(new_distance);
              update_distance += new_distance;

          }

      heading[i] = update_direction;
      direct_total += heading[i];

      distance[i] = update_distance;
      distance_total += distance[i];

    }

    float alpha = 0.3;
    float beta = 0.7;
    float maxExpect = -1.0;
    //normalize the vectors

    int max_index = 0;
    for (int i = 0; i < numSample; i++) {
        heading[i] =  heading[i] / direct_total;
        distance[i] = distance[i] / distance_total;
        expectation[i] = alpha *heading[i] + beta * distance[i];

        if (expectation[i] > maxExpect) {
              maxExpect = expectation[i];
              max_index = i; //corresponding to the former
        }
    }

    float delta_steer =  dealta_heading[max_index] /delta_t;
    float delta_acceleration = acc_scale[max_index];

    if(delta_steer > STEER_LIMIT){
      delta_steer = STEER_LIMIT;
    }
    if(delta_steer < -1*STEER_LIMIT){
      delta_steer = -1*STEER_LIMIT;
    }

    command.steer = delta_steer;
    command.acceleration = delta_acceleration;
    printf("command.steer %f\n", command.steer);
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

  sensing_info.myTank.region = indicatedRegion(sensing_info.myTank.heading);
  // sensing_info.danger.stuck = is_stuck(myTank);
  sensing_info.missiles.num = sensing_info.radar.numMissiles;  //detected number of missiles
  //printf("sensing_info.radar.numMissiles is : %d \n", sensing_info.radar.numMissiles);
   for (int i = 0; i < sensing_info.missiles.num; i++){
      sensing_info.danger.missile = TRUE;
   }

  /* first check the risk of getting stuck */
  if ((sensing_info.myTank.position.x < x_Left) || (sensing_info.myTank.position.x > x_Right) ||  (sensing_info.myTank.position.y < y_Left) || (sensing_info.myTank.position.y > y_Right))  {
     sensing_info.danger.stuck = TRUE;
  }
  else sensing_info.danger.stuck = FALSE;

   /* read missile direction and bearing */
   for (int i = 0; i< sensing_info.missiles.num; i++){
     sensing_info.missiles.relative_distance[i] = sensing_info.radar.missileDistances[i];
     sensing_info.missiles.relative_bear[i] = sensing_info.radar.missileBearings[i];
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
