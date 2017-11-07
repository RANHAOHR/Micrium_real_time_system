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

bool tank_reverse1; //indicating the reverse
bool tank_reverse2; //indicating the reverse

int RHao_tankID1;
int RHao_tankID2;
int RHao_enemyID1;
int RHao_enemyID2;

static OS_SEM RHao_done_control;
static OS_SEM RHao_done_sensing;
static OS_SEM RHao_done_alarmCheck;

typedef struct {int ID; bool radarStatus; Point position; float heading; float speed; int region; int teamMateCode;float turretDirect;} tankInfo;
typedef struct {float steer; float acceleration;} controlVector;
typedef struct {bool stuck; bool missile;bool tank_reverse;} dangerInfo;
typedef struct {int num; float relative_distance[3]; float relative_bear[3]; int missile_region; int type[3];} missileInfo; // indicate the missiles
typedef struct {tankInfo myTank; dangerInfo danger; RadarData radar; controlVector control; missileInfo missiles;} tank_data;

Tank* RHao_tank1;
Tank* RHao_tank2;

tank_data RHao_tankData1; //necessary datas of the current tank
tank_data RHao_tankData2;

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
bool safeReverseStuck(Tank* tankName, tank_data myTank){

    bool reverse_danger = FALSE;

    if (is_stuck(tankName)){  //if the tank is stuck
      float temp_dist;
      int num = myTank.missiles.num;
      for (int k = 0; k < num; k++) {
        temp_dist = myTank.missiles.relative_distance[k];
        if (temp_dist < 90) {
          reverse_danger = TRUE;
        }else{reverse_danger = FALSE;}
      }
    }
    return reverse_danger;
}
tank_data controlling(Tank* tankName, tank_data myTank){
  controlVector command;

    if (myTank.danger.missile){
    //printf("in missle command\n");
     command = dwaMissileAvoid(myTank);
    }
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
    sensing_info.myTank.radarStatus = poll_radar(myTank, &sensing_info.radar); //get the radar
    //get all the tankInfo
    sensing_info.myTank.position = get_position(myTank);
    sensing_info.myTank.heading = get_heading (myTank);
    sensing_info.myTank.speed = get_speed(myTank);

    //each time reset the missiles info
    for (int i = 0; i< 3; i++){
      sensing_info.missiles.relative_distance[i] = 10000;  //starting without detecting
      sensing_info.missiles.relative_bear[i] = 10000;
      sensing_info.missiles.type[i] = 5; //pretend it is not there
    }

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
static void RHao_sensing_task(void* p_arg){
    CPU_TS ts;
    OS_ERR err;

    while (DEF_TRUE) {

       OSSemPend(&RHao_done_control,0,OS_OPT_PEND_BLOCKING,&ts,&err);

       RHao_tankData1.myTank.ID = RHao_tankID1;  //first set the ID
       RHao_tankData1.myTank.teamMateCode = RHao_tankID2;
       RHao_tankData1 = sensing(RHao_tank1);

       RHao_tankData2.myTank.ID = RHao_tankID2;  //second tank
       RHao_tankData2.myTank.teamMateCode = RHao_tankID1;
       RHao_tankData2 = sensing(RHao_tank2);

       BSP_LED_Off(0);

       OSSemPost(&RHao_done_sensing, OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED,&err);
    }

}

//after at the sensing info, make sure if there is no danger
static void RHao_alarm_task(void* p_arg){
  CPU_TS ts;
  OS_ERR err;

  while (DEF_TRUE) {

       OSSemPend(&RHao_done_sensing,0,OS_OPT_PEND_BLOCKING,&ts,&err);

       if (RHao_tankData1.danger.missile){
          BSP_LED_On(1);
       }
       else BSP_LED_Off(1);

       if (RHao_tankData2.danger.missile){
          BSP_LED_On(2);
       }
       else BSP_LED_Off(2);

       OSSemPost(&RHao_done_alarmCheck, OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED,&err);
    }
}

// Student initialization function
static void RHao_control_task(void* p_arg){
  CPU_TS ts;
  OS_ERR err;

  while (DEF_TRUE) {
      OSSemPend(&RHao_done_alarmCheck,0,OS_OPT_PEND_BLOCKING,&ts,&err);

      bool reverse1 = safeReverseStuck(RHao_tank1,RHao_tankData1);
      bool reverse2 = safeReverseStuck(RHao_tank2,RHao_tankData2);
      if (!reverse1){  //if the tank is stuck
          tank_reverse1 = !tank_reverse1;
          set_reverse(RHao_tank1,tank_reverse1);
      }
      if (!reverse2){  //if the tank is stuck
          tank_reverse2 = !tank_reverse2;
          set_reverse(RHao_tank2,tank_reverse2);
      }
    //  if (is_stuck(RHao_tank1)){  //if the tank is stuck
    //      tank_reverse1 = !tank_reverse1;
    //      set_reverse(RHao_tank1,tank_reverse1);
    //  }
    //  if (is_stuck(RHao_tank2)){  //if the tank is stuck
    //      tank_reverse2 = !tank_reverse2;
    //      set_reverse(RHao_tank2,tank_reverse2);
    //  }

       RHao_tankData1 = controlling(RHao_tank1, RHao_tankData1);
       RHao_tankData2 = controlling(RHao_tank2, RHao_tankData2);
       OSSemPost(&RHao_done_control, OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED,&err);
    }

}

void RHao_init(char teamNumber)
{
      // Initialize any system constructs (such as mutexes) here
      OS_ERR err;
      BSP_Init();

      //enableExternalMissiles(FALSE);

      OSSemCreate(&RHao_done_control,"RHao_done_control",1,&err);  // start from a control task
      OSSemCreate(&RHao_done_sensing,"RHao_done_sensing",0,&err); //get the sensing info
      OSSemCreate(&RHao_done_alarmCheck,"RHao_done_alarmCheck",0,&err); //check the high level danger
      // Perform any user-required initializations here
      RHao_tank1 = create_tank(teamNumber,"hao");
      create_turret(RHao_tank1);

      RHao_tank2 = create_tank(teamNumber,"hao");
      create_turret(RHao_tank2);

      RHao_tankID1 = initialize_tank(RHao_tank1);
      RHao_tankID2 = initialize_tank(RHao_tank2);
      // Register user functions here
      register_user(RHao_control_task,5,0);
      register_user(RHao_sensing_task,5,0);
      register_user(RHao_alarm_task,5,0);
}
