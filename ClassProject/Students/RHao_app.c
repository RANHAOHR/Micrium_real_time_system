#include <includes.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "BattlefieldLib.h"

/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/
#define M_PI 3.14159265358

float danger_range = 300.0;
bool tank_reverse1; //indicating the reverse
bool tank_reverse2; //indicating the reverse

int RHao_tankID1;
int RHao_tankID2;

bool RHao_reverseInd1 = FALSE;
bool RHao_reverseInd2 = FALSE;

int reverseCnt1 = 0;
int reverseCnt2 = 0;
int lastCnt1 = 0;
int lastCnt2 = 0;

static OS_SEM RHao_done_control;
static OS_SEM RHao_done_sensing;
static OS_SEM RHao_done_alarmCheck;

static OS_MUTEX RHao_MControl1;
static OS_MUTEX RHao_MControl2;
static OS_MUTEX RHao_MSensing1;
static OS_MUTEX RHao_MSensing2;

static OS_MUTEX RHao_MReverse1;
static OS_MUTEX RHao_MReverse2;

typedef struct {int ID; bool radarStatus; Point position; float heading; float speed; int region; int teamMateCode;float turretDirect; float friendDirect;} tankInfo;
typedef struct {float steer; float acceleration; float turretAngle; bool fire;} controlVector;
typedef struct {bool missile; bool tankHits;} dangerInfo;
typedef struct {int dangerTankNum;int tankIDs[3]; float potentialHits_Direct[3];float potentialHits_Dist[3]; float lastDirect[3];float deltaDirect[3];} detectTanksInfo;
typedef struct {int num; float relative_distance[3]; float relative_bear[3]; int missile_region; int type[3];} missileInfo; // indicate the missiles
typedef struct {tankInfo myTank; detectTanksInfo detectedTanks; dangerInfo danger; RadarData radar; controlVector control; missileInfo missiles;} tank_data;

Tank* RHao_tank1;
Tank* RHao_tank2;

tank_data RHao_tankData1; //necessary datas of the current tank
tank_data RHao_tankData2;


int RHao_enemyID1;
int RHao_enemyID2;

Tank* RHao_enemy1;
Tank* RHao_enemy2;

tank_data RHao_enemyData1; //necessary datas of the current tank
tank_data RHao_enemyData2;
bool enemy_reverse;
bool enemy_reverse2;

bool enemy_reverseCount;
bool enemy_reverseCount2;

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
      if ( (-0.0 <= missle_heading <= front_limit_pos_1 )|| (-1*M_PI <= missle_heading <= back_limit_neg_1)) {
          missile_type = 1;
      }
      else if ((front_limit_neg_1 <= missle_heading < 0) || (back_limit_pos_1 <= missle_heading <=(M_PI) )) {
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
   //float steer_avoidMissle;

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

//    if(!reverseInd){
//      cmd_steer = cmd_steer;
//    }else{
//        cmd_steer = -cmd_steer;
//    }
//   //steer_avoidMissle = cmd_steer;
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

  return command;
 }

bool safeReverseStuck(Tank* tankName, tank_data myTank){

    bool safe_reverse = FALSE;

    if (is_stuck(tankName)){  //if the tank is stuck
      safe_reverse = TRUE;
      float temp_dist;
      int num = myTank.missiles.num;
      for (int k = 0; k < num; k++) {
        temp_dist = myTank.missiles.relative_distance[k];
        if (temp_dist < 90) {
          safe_reverse = FALSE;
          break;
        }else{safe_reverse = TRUE;}
      }
    }

    return safe_reverse;
}


controlVector avoidTankHits(tank_data myTank, bool reverseInd){

    controlVector command;
    command.steer = 0.0;
    float steer_avoidHit = 0.0;
    int total_num = 0;
    float total_direct = 0;
    float current_direction = 0.0;

    float thresh = 1.67;
    for (int j = 0; j < myTank.detectedTanks.dangerTankNum; j++) {
      current_direction = myTank.detectedTanks.potentialHits_Direct[j];
      if (( current_direction> 0.0 && current_direction < thresh) || (current_direction > -1*thresh && current_direction < 0.0)) {
        total_direct += current_direction;
        total_num += 1;
      }
    }

    if (myTank.detectedTanks.dangerTankNum > 0) {
      float direction = total_direct / total_num;
      if ( direction > 0.0 && direction <= thresh) {
          steer_avoidHit = -0.6;
      }
      else if( direction > -1*thresh && direction < 0.0 ){
        steer_avoidHit = 0.6;
      }
    }

  if(!reverseInd){
      command.steer = steer_avoidHit;
  }else{
      command.steer = -steer_avoidHit;
  }

  command.acceleration = 0.8;

  return command;

}

float singleEnemyShoot(float relative_direct,float relative_dist){
  float delta_angle = 0.0;
  float sensitiveRate = -0.08;

  float dist_factor = 0.0003;
  if(fabs(relative_direct - 0.00001) < 0.001){
    delta_angle = 0.0;
  }else{
     if (relative_direct > 0) {
      delta_angle = sensitiveRate * relative_direct - dist_factor*relative_dist;
    }else if(relative_direct < 0){
      delta_angle = -1*sensitiveRate * relative_direct + dist_factor*relative_dist;
    }
  }

  return delta_angle;

}


controlVector shootEnemies(tank_data myTank, bool reverseInd){

  controlVector shooting_command;

  int counter = 0;
  if(reverseInd == TRUE){
    counter = 1;
  }

  int detectedEnemies = 0;
  float enemyDist[2];
  float enemyDirection[2];
  float enemyHeadings[2];
  
  int safe_counter[2];

  for (int i = 0; i < myTank.detectedTanks.dangerTankNum; i++) {
    
    if ((myTank.detectedTanks.tankIDs[i] != myTank.myTank.ID) && (myTank.detectedTanks.tankIDs[i] != myTank.myTank.teamMateCode)) {
      enemyDist[detectedEnemies] = myTank.detectedTanks.potentialHits_Dist[i]; //sure this will not overflow
      enemyDirection[detectedEnemies] = myTank.detectedTanks.potentialHits_Direct[i] + counter*M_PI;
      enemyHeadings[detectedEnemies] = myTank.detectedTanks.deltaDirect[i];
      safe_counter[detectedEnemies] = i;
      if(enemyDirection[detectedEnemies] < 0.0 && enemyDirection[detectedEnemies] < M_PI){
        enemyDirection[detectedEnemies] += 2*M_PI;
      }
      detectedEnemies+=1;
    }
  }

  float thresh_close = 0.5;
  switch (detectedEnemies) {
    case 0:
      shooting_command.fire = FALSE;
      break;
    case 1:
      if(fabs(myTank.myTank.friendDirect - myTank.detectedTanks.potentialHits_Direct[safe_counter[0]]) < thresh_close){ //avoid hitting friends
        shooting_command.fire = FALSE;
      }else{
        shooting_command.fire = TRUE;
        shooting_command.turretAngle = singleEnemyShoot(enemyHeadings[0],enemyDist[0] ) + enemyDirection[0];
      }
      break;
    case 2: //always shoot the closer ones.....
      shooting_command.fire = TRUE;
      if (enemyDist[0] < enemyDist[1]) {
        if(fabs(myTank.myTank.friendDirect - myTank.detectedTanks.potentialHits_Direct[safe_counter[0]]) < thresh_close){ //avoid hitting friends
          shooting_command.fire = FALSE;
        }else{
          shooting_command.fire = TRUE;
          shooting_command.turretAngle = singleEnemyShoot(enemyHeadings[0],enemyDist[0]) + enemyDirection[0];
        }
      }else{
        if(fabs(myTank.myTank.friendDirect - myTank.detectedTanks.potentialHits_Direct[safe_counter[1]]) < thresh_close){ //avoid hitting friends
          shooting_command.fire = FALSE;
        }else{
          shooting_command.fire = TRUE;
          shooting_command.turretAngle = singleEnemyShoot(enemyHeadings[1],enemyDist[1]) + enemyDirection[1];
        }
      }
      break;
  }

  return shooting_command;

}

/***************************
 * Control task function
 ***************************/
tank_data controlling(Tank* tankName, tank_data myTank, bool reverseInd){
  controlVector command;

    if (myTank.danger.tankHits) {
      command = avoidTankHits(myTank, reverseInd);
    }
    else if (myTank.danger.missile){
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

  controlVector shoot = shootEnemies(myTank,reverseInd);
  myTank.control.fire = shoot.fire;
  myTank.control.turretAngle = shoot.turretAngle;

  if (myTank.control.fire == TRUE){
    set_turret_angle(tankName, myTank.control.turretAngle);
    fire(tankName);
  }

  return myTank;
 }


/*****************************************************
 * Sensing task function
 *****************************************************/
float bearingDisambguity(float angle){
  float sat_angle = angle;
  if(angle > M_PI ){
    sat_angle = angle - 2*M_PI;
  }
  else if (angle < -1* M_PI){
    sat_angle = angle + 2*M_PI;
  }

  return sat_angle;
}

tank_data sensing(Tank* myTank, bool revese_count){

    int counter = 0;
    if(revese_count == TRUE){
      counter = 1;
    }

    tank_data sensing_info;

    sensing_info.myTank.radarStatus = poll_radar(myTank, &sensing_info.radar); //get the radar
    //get all the tankInfo
    sensing_info.myTank.position = get_position(myTank);
    sensing_info.myTank.heading = get_heading (myTank);
    sensing_info.myTank.speed = get_speed(myTank);

    if (myTank == RHao_tank1){
      sensing_info.myTank.ID = RHao_tankID1;
      sensing_info.myTank.teamMateCode = RHao_tankID2;
    }

    if(myTank == RHao_tank2){
      sensing_info.myTank.ID = RHao_tankID2;
      sensing_info.myTank.teamMateCode = RHao_tankID1;
    }

    //each time reset the missiles info
    for (int i = 0; i< 3; i++){
      sensing_info.missiles.relative_distance[i] = 10000;  //starting without detecting
      sensing_info.missiles.relative_bear[i] = 0;
      sensing_info.missiles.type[i] = 5; //pretend it is not there

    }
    for (int i = 0; i< 3; i++){
      sensing_info.detectedTanks.potentialHits_Direct[i] = 0;
      sensing_info.detectedTanks.lastDirect[i] = 0;
      sensing_info.detectedTanks.deltaDirect[i] = 0;
      sensing_info.detectedTanks.potentialHits_Dist[i] = 10000; //start without detecting
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
    }else {sensing_info.danger.missile = FALSE;}

    /* read detected tanks direction and bearing */
    sensing_info.myTank.turretDirect = get_turret_angle(myTank);
    sensing_info.detectedTanks.dangerTankNum = sensing_info.radar.numTanks;
    for (int i = 0; i< sensing_info.detectedTanks.dangerTankNum; i++){
      sensing_info.detectedTanks.tankIDs[i] = sensing_info.radar.tankIDs[i];
      sensing_info.detectedTanks.potentialHits_Direct[i] = bearingDisambguity(sensing_info.radar.tankBearings[i] + counter*M_PI);
      sensing_info.detectedTanks.potentialHits_Dist[i] = sensing_info.radar.tankDistances[i];
      sensing_info.detectedTanks.deltaDirect[i] = sensing_info.detectedTanks.potentialHits_Direct[i] - sensing_info.detectedTanks.lastDirect[i];
      sensing_info.detectedTanks.lastDirect[i] = sensing_info.detectedTanks.potentialHits_Direct[i];
    
      if(sensing_info.detectedTanks.tankIDs[i] == sensing_info.myTank.teamMateCode){
        sensing_info.myTank.friendDirect = sensing_info.detectedTanks.potentialHits_Direct[i];
      }
    }

    sensing_info.danger.tankHits = FALSE;
    for (int i = 0; i < sensing_info.detectedTanks.dangerTankNum; i++) {
      if ( sensing_info.detectedTanks.potentialHits_Dist[i] < 190) {
        sensing_info.danger.tankHits = TRUE;
      }
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

      OSMutexPend(&RHao_MSensing1,0,OS_OPT_PEND_BLOCKING,&ts,&err);
      RHao_tankData1 = sensing(RHao_tank1, RHao_reverseInd1);
      OSMutexPost(&RHao_MSensing1,OS_OPT_POST_1,&err);

      OSMutexPend(&RHao_MSensing2,0,OS_OPT_PEND_BLOCKING,&ts,&err);
      RHao_tankData2 = sensing(RHao_tank2, RHao_reverseInd2);
      OSMutexPost(&RHao_MSensing2,OS_OPT_POST_1,&err);

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
       if(RHao_tankData1.danger.tankHits){
         BSP_LED_On(0);
       }else{BSP_LED_Off(0);}
       
       if(RHao_tankData2.danger.tankHits){
         BSP_LED_On(1);
       }else{BSP_LED_Off(1);}

       OSSemPost(&RHao_done_alarmCheck, OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED,&err);
    }
}

// Student initialization function
static void RHao_control_task(void* p_arg){
  CPU_TS ts;
  OS_ERR err;

  while (DEF_TRUE) {
     OSSemPend(&RHao_done_alarmCheck,0,OS_OPT_PEND_BLOCKING,&ts,&err);

    OSMutexPend(&RHao_MControl1,0,OS_OPT_PEND_BLOCKING,&ts,&err);
    bool reverse1 = safeReverseStuck(RHao_tank1,RHao_tankData1);
    int last1 = reverseCnt1;
    if (reverse1){  //if the tank is stuck
        tank_reverse1 = !tank_reverse1;
        OSMutexPend(&RHao_MReverse1,0,OS_OPT_PEND_BLOCKING,&ts,&err);
        reverseCnt1 +=1;
        set_reverse(RHao_tank1,tank_reverse1);
        OSMutexPost(&RHao_MReverse1,OS_OPT_POST_1,&err);
    }

    if(reverseCnt1 - last1 == 0){
      if((reverseCnt1-lastCnt1) > abs(reverseCnt1-1) ){
      RHao_reverseInd1 = !RHao_reverseInd1;
      reverseCnt1 = 0;
      }
    }

    RHao_tankData1 = controlling(RHao_tank1, RHao_tankData1, RHao_reverseInd1);
    OSMutexPost(&RHao_MControl1,OS_OPT_POST_1,&err);

    OSMutexPend(&RHao_MControl2,0,OS_OPT_PEND_BLOCKING,&ts,&err);
    bool reverse2 = safeReverseStuck(RHao_tank2,RHao_tankData2);
    int last = reverseCnt2;
    if (reverse2){  //if the tank is stuck
      OSMutexPend(&RHao_MReverse2,0,OS_OPT_PEND_BLOCKING,&ts,&err);
      reverseCnt2 +=1;
      tank_reverse2 = !tank_reverse2;
      set_reverse(RHao_tank2,tank_reverse2);
      OSMutexPost(&RHao_MReverse2,OS_OPT_POST_1,&err);
    }

    if(reverseCnt2 - last == 0){
      if((reverseCnt2-lastCnt2) > abs(reverseCnt2-1) ){
      RHao_reverseInd2 = !RHao_reverseInd2;
      reverseCnt2 = 0;
      }
    }

    RHao_tankData2 = controlling(RHao_tank2, RHao_tankData2, RHao_reverseInd2);
    OSMutexPost(&RHao_MControl2,OS_OPT_POST_1,&err);

    /*------------------enemy set up--------------------- */
//    if (is_stuck(RHao_enemy1)){  
//         enemy_reverse = !enemy_reverse;
//         set_reverse(RHao_enemy1,enemy_reverse);           
//     }
     set_steering(RHao_enemy1, 0.0);
     accelerate(RHao_enemy1, 1.0);
    //RHao_enemyData1 = controlling(RHao_enemy1, RHao_enemyData1, enemy_reverseCount);

//    if(is_stuck(RHao_enemy2)){
//      enemy_reverse2 = !enemy_reverse2;
//      set_reverse(RHao_enemy2, enemy_reverse2);
//    }
     set_steering(RHao_enemy2, 0.0);
     accelerate(RHao_enemy2, 1.0);
    //RHao_enemyData2 = controlling(RHao_enemy2, RHao_enemyData2, enemy_reverseCount2);

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

      OSMutexCreate(&RHao_MControl1,"control1",&err);
      OSMutexCreate(&RHao_MControl2,"control2",&err);
      OSMutexCreate(&RHao_MSensing1,"sensing1",&err);
      OSMutexCreate(&RHao_MSensing2,"sensing2",&err);

      OSMutexCreate(&RHao_MReverse1,"reverse1",&err);
      OSMutexCreate(&RHao_MReverse2,"reverse2",&err);

      // Perform any user-required initializations here
      RHao_tank1 = create_tank(teamNumber,"hao");
      create_turret(RHao_tank1);
      RHao_tankID1 = initialize_tank(RHao_tank1);

      RHao_tank2 = create_tank(teamNumber,"hao");
      create_turret(RHao_tank2);
      RHao_tankID2 = initialize_tank(RHao_tank2);

      RHao_enemy1 = create_tank(2,"evil");
      create_turret(RHao_enemy1);
      RHao_enemyID1 = initialize_tank(RHao_enemy1);
      
      RHao_enemy2 = create_tank(2,"evil");
      create_turret(RHao_enemy2);
      RHao_enemyID2 = initialize_tank(RHao_enemy2);


      // Register user functions here
      register_user(RHao_control_task,5,0);
      register_user(RHao_sensing_task,5,0);
      register_user(RHao_alarm_task,5,0);
}
