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

float danger_range = 300.0;

int x_Uplimit = 700;
int x_Downlimit =50;
int y_Uplimit =550;
int y_Downlimit = 150;

bool tank_reverse; //indicating the reverse
int tank_ID; //currentl 1
int reverse_count;

static OS_SEM done_control;
static OS_SEM done_sensing;
static OS_SEM done_alarm_check;

typedef struct {int ID; bool status; Point position; float heading; float speed; int region;} tankInfo;
typedef struct {float steer; float acceleration;} controlVector;
typedef struct {bool stuck; bool missile;} dangerInfo;
typedef struct {int num; int ID[3]; float relative_distance[3]; float relative_bear[3]; int missile_region; int type[3];} missileInfo; // indicate the missiles
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

float back_danger_pos_limit = M_PI /10;
float back_danger_neg_limit = -1*M_PI /10;

/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

int getMissileType(float missile_bearing, float missile_distance){

    float missle_heading;
    int missile_type;

    if (missile_bearing > M_PI) {
      missle_heading = missile_bearing - 2*M_PI;
    }

    if (missile_distance <= 270) {
      if ( (-0.1 <= missle_heading <= front_limit_pos_1 )|| (-1*M_PI <= missle_heading <= back_limit_neg_1)) {
          missile_type = 1;
      }
      else if ((front_limit_neg_1 <= missle_heading < 0) || (back_limit_pos_1 <= missle_heading <=( M_PI+0.1) )) {
          missile_type = 2;
      }
      else if ((front_limit_pos_1 < missle_heading <= front_limit_pos_2 )|| (back_limit_neg_1 < missle_heading <= back_limit_neg_2)) {
          missile_type = 3;
      }
      else if ((front_limit_neg_2 < missle_heading < front_limit_neg_1) || (back_limit_pos_2 < missle_heading < back_limit_pos_1)) {
        missile_type = 4;
      }
      else{
        missile_type = 5;
      }
    }else{
      if ( (0 <= missle_heading <= front_limit_pos_2 )|| (-1*M_PI <= missle_heading <= back_limit_neg_2)) {
          missile_type = 6;
      }
      else if ((front_limit_neg_2 <= missle_heading < 0) || (back_limit_pos_2 <= missle_heading <= M_PI )) {
          missile_type = 7;
      }
      else{
        missile_type = 5;
      }
    }

   return missile_type;
}
/*
 * contorller, control the tank to avoid missile
 */
 float singleMissileSteer(float missile_type, tank_data myTank){
   float cmd_steer;
   if (missile_type == 1) {
      cmd_steer = 0.6;
   }else if (missile_type == 2) {
     cmd_steer = -0.6;
   }else if (missile_type == 3) {
     cmd_steer = 0.3;
   }else if (missile_type == 4) {
     cmd_steer = -0.3;
   }
   else if (missile_type == 6) {
     cmd_steer = -0.3;
   }
   else if (missile_type == 7) {
     cmd_steer = 0.3;
   }
   else if (missile_type == 5) {
     cmd_steer = 0.0;
   }

   return cmd_steer;
 }

controlVector avoid_stuck(tank_data INFO){
  controlVector command;
  int x = INFO.myTank.position.x;
  int y = INFO.myTank.position.y;

  int direction = 2*INFO.myTank.heading/M_PI;

  int dx = x - 450;
  int dy = y - 350;
  if (reverse_count%2 ==0) {
    if (x > x_Uplimit) {
      if (direction == 0){command.steer = -0.27;}
      if(direction == 3){command.steer = 0.27;}
      if (dy < 0) {if (direction == 1){command.steer = -0.15;}}
      if (dy > 0) {if(direction == 2){command.steer = 0.15;}}
    }
    if (x < x_Downlimit) {
      if (direction == 1){command.steer = 0.27;}
      if(direction == 2){command.steer = -0.27;}
      if (dy < 0) {if(direction == 0){command.steer = 0.15;}}
      if (dy > 0) {if(direction == 3){command.steer = -0.15;}}
    }
    if (y > y_Uplimit) {
      if (direction == 2){command.steer = 0.27;}
      if(direction == 3){command.steer = -0.27;}
      if (dx < 0) {if (direction == 1){command.steer = 0.15;}}
      if (dx > 0) {if(direction == 0){command.steer = -0.15;}}
    }
    if (y < y_Downlimit) {
      if (direction == 0){command.steer = 0.27;}
      if(direction == 1){command.steer = -0.27;}
      if (dx < 0) {if(direction == 2){command.steer = -0.15;}}
      if (dx > 0) {if(direction == 3){command.steer = 0.15;}}
    }
  }else if (reverse_count%2 == 1) {
    if (x > x_Uplimit) {
      if (direction == 1){command.steer = 0.27;}
      if(direction == 2){command.steer = -0.27;}
      if (dy < 0) {if(direction == 3){command.steer = -0.15;}}
      if (dy > 0) {if(direction == 0){command.steer = 0.15;}}
    }
    if (x < x_Downlimit) {
      if (direction == 0){command.steer = 0.3;}
      if(direction == 3){command.steer = -0.3;}
      if (dy < 0) {if (direction == 2){command.steer = 0.15;}}
      if (dy > 0) {if (direction == 1){command.steer = -0.15;}}
    }
    if (y > y_Uplimit) {
      if (direction == 0){command.steer = 0.3;}
      if(direction == 1){command.steer = -0.3;}
      if (dx < 0) {if (direction == 3){command.steer = 0.15;}}
      if (dx > 0) {if (direction == 2){command.steer = -0.15;}}
    }
    if (y < y_Downlimit) {
      if (direction == 2){command.steer = 0.3;}
      if(direction == 3){command.steer = -0.3;}
      if (dx < 0) {if (direction == 0){command.steer = -0.15;}}
      if (dx > 0) {if (direction == 1){command.steer = 0.15;}}
    }
  }

  command.acceleration = 1;
  return command;

}

controlVector dwaMissileAvoid(tank_data myTank){
    controlVector command;

    float cmd_steer = 0.0;
    float cmd_vel = 1.0;
    int num_missles = myTank.missiles.num;

    float missile_type;
    float missle_1;
    float missle_2;
    float missle_3;

    float dist_1;
    float dist_2;
    float dist_3;

    float ave_diret;
    float ave_dist;
    int ave_type;

    switch(num_missles){
      case 1:
          missile_type = myTank.missiles.type[0];
          if(myTank.missiles.relative_bear[0] ==0.0){
            cmd_steer = 0.7;
          }
          else cmd_steer = singleMissileSteer(missile_type, myTank);
          cmd_vel = 1.2;
          break;
      case 2:
          missle_1 = myTank.missiles.relative_bear[0];
          missle_2 = myTank.missiles.relative_bear[1];
          dist_1 = myTank.missiles.relative_distance[0];
          dist_2 = myTank.missiles.relative_distance[1];

          ave_diret = (missle_1 + missle_2) / 2;
          ave_dist = (dist_1 + dist_2) / 2;
          ave_type =  getMissileType(ave_diret,ave_dist);
          cmd_steer = singleMissileSteer(ave_type, myTank);
          cmd_vel = 1.0;
          break;
      case 3:
          missle_1 = myTank.missiles.relative_bear[0];
          missle_2 = myTank.missiles.relative_bear[1];
          missle_3 = myTank.missiles.relative_bear[2];

          dist_1 = myTank.missiles.relative_distance[0];
          dist_2 = myTank.missiles.relative_distance[1];
          dist_3 = myTank.missiles.relative_distance[2];
          ave_diret = (missle_1 + missle_2 + missle_3) / 3;
          ave_dist = (dist_1 + dist_2 + dist_3) / 3;
          ave_type =  getMissileType(ave_diret, ave_dist);
          cmd_steer = singleMissileSteer(ave_type, myTank);
          cmd_vel = 1.5;
          break;
       case 0:
          cmd_steer = 0.0;
          cmd_vel = 1.0;
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

    bool reverse_danger = FALSE;

     if (is_stuck(tank_target)){  //if the tank is stuck
       tank_reverse = !tank_reverse;
       float temp_dist;
       float temp_direct;
       int num = total_data.missiles.num;
       for (int k = 0; k < num; k++) {
         temp_dist = total_data.missiles.relative_distance[k];
         temp_direct = total_data.missiles.relative_bear[k] - M_PI;
         if (temp_dist < 100) {
           reverse_danger = TRUE;
         }else{reverse_danger = FALSE;}
       }

        if (reverse_danger) {
           command = dwaMissileAvoid(myTank);
       }
       else{set_reverse(tank_target,tank_reverse);
         reverse_count +=1;
       }
      }
//      else  if (total_data.danger.stuck && total_data.control.acceleration == 0){  //may be want to reduce the velocity to 0 first
//          set_reverse(tank_target, stuck_reverse);
//          reverse_count +=1;
//      }

    if (myTank.danger.missile){
    //printf("in missle command\n");
     command = dwaMissileAvoid(myTank);
    }
    // else if (myTank.danger.stuck){
    // //printf("in missle command\n");
    //  command = avoid_stuck(myTank);
    // }
    else{
      command.acceleration = 1.5;
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
  //printf("sensing_info.radar.numMissiles is : %d \n", sensing_info.radar.numMissiles);
 int current_x = sensing_info.myTank.position.x;
 int current_y = sensing_info.myTank.position.y;
  if ((current_x < x_Downlimit) || (current_x > x_Uplimit) ||  (current_y < y_Downlimit) || (current_y> y_Uplimit) )  {
     sensing_info.danger.stuck = TRUE;
  }
  else {
    sensing_info.danger.stuck = FALSE ;
  }

  sensing_info.missiles.num = 0;
   /* read missile direction and bearing */
   for (int i = 0; i< sensing_info.radar.numMissiles; i++){
     if(sensing_info.radar.missileDistances[i] < danger_range){
        sensing_info.missiles.num += 1;
        sensing_info.missiles.relative_distance[i] = sensing_info.radar.missileDistances[i];
        sensing_info.missiles.relative_bear[i] = sensing_info.radar.missileBearings[i];
        sensing_info.missiles.type[i] = getMissileType(sensing_info.missiles.relative_bear[i], sensing_info.missiles.relative_distance[i]);
     }
   }
    if (sensing_info.missiles.num > 0) {
    sensing_info.danger.missile = TRUE;
    }else {
      sensing_info.danger.missile = FALSE;
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
static void alarm_task(void* p_arg){
  CPU_TS ts;
  OS_ERR err;

  while (DEF_TRUE) {

       OSSemPend(&done_sensing,0,OS_OPT_PEND_BLOCKING,&ts,&err);

       if (total_data.danger.missile){
          int danger_num = total_data.missiles.num;
         BSP_LED_On(danger_num);
       }
       else BSP_LED_Off(0);

       OSSemPost(&done_alarm_check, OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED,&err);
    }
}

// Student initialization function
static void control_task(void* p_arg){
  CPU_TS ts;
  OS_ERR err;

  while (DEF_TRUE) {
       OSSemPend(&done_alarm_check,0,OS_OPT_PEND_BLOCKING,&ts,&err);

       total_data = controlling(tank_target, total_data);
       OSSemPost(&done_control, OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED,&err);
    }

}

void RHao_init(char teamNumber)
{
    // Initialize any system constructs (such as mutexes) here
     OS_ERR err;
     BSP_Init();

     reverse_count = 0;
     OSSemCreate(&done_control,"done_control",1,&err);  // start from a control task
     OSSemCreate(&done_sensing,"done_sensing",0,&err); //get the sensing info
     OSSemCreate(&done_alarm_check,"done_alarm_check",0,&err); //check the high level danger
    // Perform any user-required initializations here
    tank_target = create_tank(1,"target");

    tank_ID = initialize_tank(tank_target);
    // Register user functions here
    register_user(control_task,5,0);
    register_user(sensing_task,5,0);
    register_user(alarm_task,5,0);
}
