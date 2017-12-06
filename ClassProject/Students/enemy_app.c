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

float Enemy_danger_range = 300.0;
float Enemy_thresh = 2.0;

bool Enemy_tank_reverse1; //indicating the reverse
bool Enemy_tank_reverse2; //indicating the reverse

int Enemy_tankID1;
int Enemy_tankID2;

bool Enemy_reverseInd1 = FALSE;
bool Enemy_reverseInd2 = FALSE;

int Enemy_reverseCnt1 = 0;
int Enemy_reverseCnt2 = 0;

static OS_SEM Enemy_done_control;
static OS_SEM Enemy_done_sensing;

static OS_MUTEX Enemy_MControl1;
static OS_MUTEX Enemy_MControl2;
static OS_MUTEX Enemy_MSensing1;
static OS_MUTEX Enemy_MSensing2;

static OS_MUTEX Enemy_MReverse1;
static OS_MUTEX Enemy_MReverse2;

typedef struct {int ID; bool radarStatus; Point position; float heading; float speed; int region; int teamMateCode;float turretDirect; float friendDirect;} Enemy_tankInfo;
typedef struct {float steer; float acceleration; float turretAngle; bool fire;} controlVector;
typedef struct {bool missile; bool tankHits;} Enemy_dangerInfo;
typedef struct {int dangerTankNum;int tankIDs[3]; float potentialHits_Direct[3];float potentialHits_Dist[3]; float lastDirect[3];float deltaDirect[3]; float effectDirection;} detectTanksInfo;
typedef struct {int num; float relative_distance[3]; float relative_bear[3]; int missile_region; int type[3];} Enemy_missileInfo; // indicate the missiles
typedef struct {Enemy_tankInfo myTank; detectTanksInfo detectedTanks; Enemy_dangerInfo danger; RadarData radar; controlVector control; Enemy_missileInfo missiles;} tank_data;

Tank* Enemy_tank1;
Tank* Enemy_tank2;

tank_data Enemy_tankData1; //necessary datas of the current tank
tank_data Enemy_tankData2;

float Enemy_front_limit_pos_1 = M_PI / 8;
float Enemy_front_limit_neg_1 = -1* M_PI / 8;
float Enemy_front_limit_pos_2 = M_PI /6;
float Enemy_front_limit_neg_2 = -1* M_PI / 6;

float Enemy_back_limit_pos_1 = 7 * M_PI / 8;
float Enemy_back_limit_neg_1 = -7 * M_PI / 8;
float Enemy_back_limit_pos_2 = 5* M_PI /6;
float Enemy_back_limit_neg_2 = -5* M_PI /6;


//Tank* Enemy_enemy1;
//Tank* Enemy_enemy2;
//
//int enemy1;
//int enemy2;
//
//tank_data Enemy_enemyData1; //necessary datas of the current tank
//tank_data Enemy_enemyData2;
//
//bool Enemy_enemyInd1;
//bool Enemy_enemyInd2;
//
//bool enemy_reverse1;
//bool enemy_reverse2;

/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/

int enemy_getMissileType(float missile_bearing, float missile_distance){

    float missle_heading;
    int missile_type;

    if (missile_bearing > M_PI) {
      missle_heading = missile_bearing - 2*M_PI;
    }

    if (missile_distance <= 270) {
      if ( (-0.0 <= missle_heading <= Enemy_front_limit_pos_1 )|| (-1*M_PI <= missle_heading <= Enemy_back_limit_neg_1)) {
          missile_type = 1;
      }
      else if ((Enemy_front_limit_neg_1 <= missle_heading < 0) || (Enemy_back_limit_pos_1 <= missle_heading <=(M_PI) )) {
          missile_type = 2;
      }
      else if ((Enemy_front_limit_pos_1 < missle_heading <= Enemy_front_limit_pos_2 )|| (Enemy_back_limit_neg_1 < missle_heading <= Enemy_back_limit_neg_2)) {
          missile_type = 3;
      }
      else if ((Enemy_front_limit_neg_2 < missle_heading < Enemy_front_limit_neg_1) || (Enemy_back_limit_pos_2 < missle_heading < Enemy_back_limit_pos_1)) {
        missile_type = 4;
      }
      else{
        missile_type = 5;
      }
    }else{
      if ( (0 <= missle_heading <= Enemy_front_limit_pos_2 )|| (-1*M_PI <= missle_heading <= Enemy_back_limit_neg_2)) {
          missile_type = 6;
      }
      else if ((Enemy_front_limit_neg_2 <= missle_heading < 0) || (Enemy_back_limit_pos_2 <= missle_heading <= M_PI )) {
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
 float enemy_singleMissileSteer(float missile_type){
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
     cmd_steer = -0.5;
   }
   else if (missile_type == 7) {
     cmd_steer = 0.5;
   }
   else if (missile_type == 5) {
     cmd_steer = 0.0;
   }

   return cmd_steer;
 }


controlVector enemy_dwaMissileAvoid(tank_data myTank){
    controlVector command;

    float cmd_steer = 0.0;
    float cmd_vel = 1.0;
    int num_missles = myTank.missiles.num;

    int missile_type;
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
          cmd_steer = enemy_singleMissileSteer(missile_type);
          cmd_vel = 1.2;
          break;
      case 2:
          missle_1 = myTank.missiles.relative_bear[0];
          missle_2 = myTank.missiles.relative_bear[1];
          dist_1 = myTank.missiles.relative_distance[0];
          dist_2 = myTank.missiles.relative_distance[1];
          if(dist_1 <= 50 && dist_2 > 50 ){
             missile_type = myTank.missiles.type[0];
             cmd_steer = enemy_singleMissileSteer(missile_type);
          }else if(dist_1 > 50 && dist_2 <= 50){
             missile_type = myTank.missiles.type[1];
             cmd_steer = enemy_singleMissileSteer(missile_type);
          }else{
            ave_diret = (missle_1 + missle_2) / 2;
            ave_dist = (dist_1 + dist_2) / 2;
            ave_type =  enemy_getMissileType(ave_diret,ave_dist);
            cmd_steer = enemy_singleMissileSteer(ave_type);
          }

          cmd_vel = 1.5;
          break;
      case 3:
          missle_1 = myTank.missiles.relative_bear[0];
          missle_2 = myTank.missiles.relative_bear[1];
          missle_3 = myTank.missiles.relative_bear[2];

          dist_1 = myTank.missiles.relative_distance[0];
          dist_2 = myTank.missiles.relative_distance[1];
          dist_3 = myTank.missiles.relative_distance[2];

          if(dist_1 <= 50 ){
             missile_type = myTank.missiles.type[0];
             cmd_steer = enemy_singleMissileSteer(missile_type);
          }else if(dist_2 <= 50){
             missile_type = myTank.missiles.type[1];
             cmd_steer = enemy_singleMissileSteer(missile_type);
          }else if(dist_3 <= 50){
             missile_type = myTank.missiles.type[2];
             cmd_steer = enemy_singleMissileSteer(missile_type);
          }else{
            ave_diret = (missle_1 + missle_2 + missle_3) / 3;
            ave_dist = (dist_1 + dist_2 + dist_3) / 3;
            ave_type =  enemy_getMissileType(ave_diret, ave_dist);
            cmd_steer = enemy_singleMissileSteer(ave_type);
          }
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

bool enemy_safeReverseStuck(Tank* tankName, tank_data myTank){

    bool safe_reverse = FALSE;

    if (is_stuck(tankName)){  //if the tank is stuck
      safe_reverse = TRUE;

      float missile_dist;
      int num = myTank.missiles.num;
      for (int k = 0; k < num; k++) {
        missile_dist = myTank.missiles.relative_distance[k];
        if (missile_dist < 70) {
          safe_reverse = FALSE;
          break;
        }else{safe_reverse = TRUE;}
      }

    }

    return safe_reverse;
}


controlVector enemy_avoidTankHits(tank_data myTank, bool reverseInd){

    controlVector command;
    command.steer = 0.0;
    float steer_avoidHit = 0.0;

    if (myTank.detectedTanks.dangerTankNum > 0) {
      float direction = myTank.detectedTanks.effectDirection;
      if ((direction >= 0.0 && direction <= Enemy_thresh)) {
          steer_avoidHit = -0.6;
      }

      if( (direction > -1*Enemy_thresh && direction < 0.0)){
        steer_avoidHit = 0.6;
      }
    }

  if(!reverseInd){
      command.steer = steer_avoidHit;
  }else{
      command.steer = -steer_avoidHit;
  }

  command.acceleration = 0.9;

  return command;

}

float enemy_singleEnemyShoot(float relative_direct,float relative_dist, float base_current){
  float delta_angle = 0.0;
  float sensitiveRate = -0.07;

  float dist_factor = 0.0005;
  if(fabs(relative_direct - 0.0001) < 0.001){
    delta_angle = 0.0;
  }else{
     if (relative_direct > 0) {
    //  delta_angle = sensitiveRate * relative_direct - dist_factor*relative_dist;
      if(base_current > 0 && base_current < M_PI){
        delta_angle = sensitiveRate * relative_direct - dist_factor*relative_dist;
      }
      if(base_current < 0 && base_current >= -1*M_PI){
        delta_angle = sensitiveRate * relative_direct + dist_factor*relative_dist;
      }
     }else if(relative_direct < 0){
      if(base_current > 0 && base_current < M_PI){
        delta_angle = sensitiveRate * relative_direct + dist_factor*relative_dist;
      }
      if(base_current < 0 && base_current >= -1*M_PI){
        delta_angle = sensitiveRate * relative_direct - dist_factor*relative_dist;
      }

    }
  }

  return delta_angle;

}


controlVector enemy_shootEnemies(tank_data myTank, bool reverseInd){

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
        shooting_command.turretAngle = enemy_singleEnemyShoot(enemyHeadings[0],enemyDist[0],enemyDirection[0] ) + enemyDirection[0];
      }
      break;
    case 2: //always shoot the closer ones.....
    shooting_command.fire = TRUE;
    if(fabs(myTank.myTank.friendDirect - myTank.detectedTanks.potentialHits_Direct[safe_counter[0]]) < thresh_close){ //avoid hitting friends
      shooting_command.fire = FALSE;
    }else{
      shooting_command.fire = TRUE;
      if (enemyDist[0] < enemyDist[1]) shooting_command.turretAngle = enemy_singleEnemyShoot(enemyHeadings[0],enemyDist[0],enemyDirection[0]) + enemyDirection[0];
      if (enemyDist[1] < enemyDist[0]) shooting_command.turretAngle = enemy_singleEnemyShoot(enemyHeadings[1],enemyDist[1],enemyDirection[1]) + enemyDirection[1];
    }
      break;
  }

  return shooting_command;

}

/***************************
 * Control task function
 ***************************/
tank_data enemy_controlling(Tank* tankName, tank_data myTank, bool reverseInd){
  controlVector command;

  if (myTank.danger.tankHits) {
    command = enemy_avoidTankHits(myTank, reverseInd);
  }
  else if (myTank.danger.missile){
   command = enemy_dwaMissileAvoid(myTank);
  }
  else{
  command.acceleration = 1.5;
  command.steer = 0.0;
  }

  myTank.control.acceleration = command.acceleration;
  myTank.control.steer = command.steer;

  set_steering(tankName, myTank.control.steer);
  accelerate(tankName, myTank.control.acceleration);

  controlVector shoot = enemy_shootEnemies(myTank,reverseInd);
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
float enemy_bearingDisambguity(float angle){
  float sat_angle = angle;

  if (angle > 2*M_PI){
    sat_angle = angle - 2*M_PI;
  }

  if(sat_angle > M_PI ){
    sat_angle = sat_angle - 2*M_PI;
  }
  else if (sat_angle < -1* M_PI){
    sat_angle = sat_angle + 2*M_PI;
  }

  return sat_angle;
}

tank_data enemy_sensing(Tank* myTank, bool revese_count){

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

    if (myTank == Enemy_tank1){
      sensing_info.myTank.ID = Enemy_tankID1;
      sensing_info.myTank.teamMateCode = Enemy_tankID2;
    }

    if(myTank == Enemy_tank2){
      sensing_info.myTank.ID = Enemy_tankID2;
      sensing_info.myTank.teamMateCode = Enemy_tankID1;
    }

//    //each time reset the missiles info
//    for (int i = 0; i< 3; i++){
//      sensing_info.missiles.relative_distance[i] = 10000;  //starting without detecting
//      sensing_info.missiles.relative_bear[i] = 0;
//      sensing_info.missiles.type[i] = 5; //pretend it is not there
//
//    }
//    for (int i = 0; i< 3; i++){
//      sensing_info.detectedTanks.potentialHits_Direct[i] = 0;
//      sensing_info.detectedTanks.lastDirect[i] = 0;
//      sensing_info.detectedTanks.deltaDirect[i] = 0;
//      sensing_info.detectedTanks.potentialHits_Dist[i] = 10000; //start without detecting
//    }
//    sensing_info.detectedTanks.effectDirection = 0;
//    sensing_info.missiles.num = 0;
//    /* read missile direction and bearing */
//    for (int i = 0; i< sensing_info.radar.numMissiles; i++){
//     if(sensing_info.radar.missileDistances[i] < Enemy_danger_range){
//        sensing_info.missiles.num += 1;
//        sensing_info.missiles.relative_distance[i] = sensing_info.radar.missileDistances[i];
//        sensing_info.missiles.relative_bear[i] = sensing_info.radar.missileBearings[i];
//        sensing_info.missiles.type[i] = enemy_getMissileType(sensing_info.missiles.relative_bear[i], sensing_info.missiles.relative_distance[i]);
//     }
//    }

    if (sensing_info.missiles.num > 0) {
        sensing_info.danger.missile = TRUE;
    }else {sensing_info.danger.missile = FALSE;}

    /* read detected tanks direction and bearing */
    sensing_info.myTank.turretDirect = get_turret_angle(myTank);
    sensing_info.detectedTanks.dangerTankNum = sensing_info.radar.numTanks;
    for (int i = 0; i< sensing_info.detectedTanks.dangerTankNum; i++){
      sensing_info.detectedTanks.tankIDs[i] = sensing_info.radar.tankIDs[i];
      sensing_info.detectedTanks.potentialHits_Direct[i] = enemy_bearingDisambguity(sensing_info.radar.tankBearings[i] + counter*M_PI);
      sensing_info.detectedTanks.potentialHits_Dist[i] = sensing_info.radar.tankDistances[i];
      sensing_info.detectedTanks.deltaDirect[i] = sensing_info.detectedTanks.potentialHits_Direct[i] - sensing_info.detectedTanks.lastDirect[i];
      sensing_info.detectedTanks.lastDirect[i] = sensing_info.detectedTanks.potentialHits_Direct[i];

      if(sensing_info.detectedTanks.tankIDs[i] == sensing_info.myTank.teamMateCode){
        sensing_info.myTank.friendDirect = sensing_info.detectedTanks.potentialHits_Direct[i];
      }
    }

    sensing_info.danger.tankHits = FALSE;

    int total_num = 0;
    float total_direct = 0;
    float current_direction = 0.0;
    for (int j = 0; j < sensing_info.detectedTanks.dangerTankNum; j++) {
      if(sensing_info.detectedTanks.potentialHits_Dist[j] < 260){
        current_direction = sensing_info.detectedTanks.potentialHits_Direct[j];
        if (( current_direction> 0.0 && current_direction < Enemy_thresh) || (current_direction > -1*Enemy_thresh && current_direction < 0.0)) {
          sensing_info.danger.tankHits = TRUE;
          total_direct += current_direction;
          total_num += 1;
        }
      }
    }
    if (total_num > 0) {
      sensing_info.detectedTanks.effectDirection = total_direct / total_num;
    }

    return sensing_info;
}

// Student initialization function
/****************
 * The task functions, including the sensing information receiving, and danger check and control task
 ****************/
static void Enemy_sensing_task(void* p_arg){
    CPU_TS ts;
    OS_ERR err;

    while (DEF_TRUE) {
      OSSemPend(&Enemy_done_control,0,OS_OPT_PEND_BLOCKING,&ts,&err);

      OSMutexPend(&Enemy_MSensing1,0,OS_OPT_PEND_BLOCKING,&ts,&err);
      Enemy_tankData1 = enemy_sensing(Enemy_tank1, Enemy_reverseInd1);
      OSMutexPost(&Enemy_MSensing1,OS_OPT_POST_1,&err);

      OSMutexPend(&Enemy_MSensing2,0,OS_OPT_PEND_BLOCKING,&ts,&err);
      Enemy_tankData2 = enemy_sensing(Enemy_tank2, Enemy_reverseInd2);
      OSMutexPost(&Enemy_MSensing2,OS_OPT_POST_1,&err);

      BSP_LED_Off(0);

      OSSemPost(&Enemy_done_sensing, OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED,&err);
    }
}

static void Enemy_control_task(void* p_arg){
  CPU_TS ts;
  OS_ERR err;

  while (DEF_TRUE) {
     OSSemPend(&Enemy_done_sensing,0,OS_OPT_PEND_BLOCKING,&ts,&err);

    OSMutexPend(&Enemy_MControl1,0,OS_OPT_PEND_BLOCKING,&ts,&err);
    bool reverse1 = enemy_safeReverseStuck(Enemy_tank1,Enemy_tankData1);
    int last1 = Enemy_reverseCnt1;
    if (reverse1){  //if the tank is stuck
        Enemy_tank_reverse1 = !Enemy_tank_reverse1;
        OSMutexPend(&Enemy_MReverse1,0,OS_OPT_PEND_BLOCKING,&ts,&err);
        Enemy_reverseCnt1 +=1;
        set_reverse(Enemy_tank1,Enemy_tank_reverse1);
        OSMutexPost(&Enemy_MReverse1,OS_OPT_POST_1,&err);
    }

    if(Enemy_reverseCnt1 - last1 == 0){
      if(Enemy_reverseCnt1 > abs(Enemy_reverseCnt1-1) ){
      Enemy_reverseInd1 = !Enemy_reverseInd1;
      Enemy_reverseCnt1 = 0;
      }
    }

    Enemy_tankData1 = enemy_controlling(Enemy_tank1, Enemy_tankData1, Enemy_reverseInd1);
    OSMutexPost(&Enemy_MControl1,OS_OPT_POST_1,&err);

    /* second tank */
    OSMutexPend(&Enemy_MControl2,0,OS_OPT_PEND_BLOCKING,&ts,&err);
    bool reverse2 = enemy_safeReverseStuck(Enemy_tank2,Enemy_tankData2);
    int last = Enemy_reverseCnt2;
    if (reverse2){  //if the tank is stuck
      OSMutexPend(&Enemy_MReverse2,0,OS_OPT_PEND_BLOCKING,&ts,&err);
      Enemy_reverseCnt2 +=1;
      Enemy_tank_reverse2 = !Enemy_tank_reverse2;
      set_reverse(Enemy_tank2,Enemy_tank_reverse2);
      OSMutexPost(&Enemy_MReverse2,OS_OPT_POST_1,&err);
    }

    if(Enemy_reverseCnt2 - last == 0){
      if(Enemy_reverseCnt2 > abs(Enemy_reverseCnt2-1) ){
      Enemy_reverseInd2 = !Enemy_reverseInd2;
      Enemy_reverseCnt2 = 0;
      }
    }

    Enemy_tankData2 = enemy_controlling(Enemy_tank2, Enemy_tankData2, Enemy_reverseInd2);
    OSMutexPost(&Enemy_MControl2,OS_OPT_POST_1,&err);
//
//    bool reverse3 = enemy_safeReverseStuck(Enemy_enemy1,Enemy_enemyData1);
//    if(reverse3){
//      enemy_reverse1 = !enemy_reverse1;
//      set_reverse(Enemy_enemy1,enemy_reverse1);
//    }
//
//    Enemy_enemyData1 = enemy_controlling(Enemy_enemy1, Enemy_enemyData1, Enemy_enemyInd1);
//    bool reverse4 = enemy_safeReverseStuck(Enemy_enemy2,Enemy_enemyData2);
//
//    if(reverse4){
//      enemy_reverse2 = !enemy_reverse2;
//      set_reverse(Enemy_enemy2,enemy_reverse2);
//    }
//    Enemy_enemyData2 = enemy_controlling(Enemy_enemy2, Enemy_enemyData2, Enemy_enemyInd2);


    OSSemPost(&Enemy_done_control, OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED,&err);
    }

}

void enemy_init(char teamNumber)
{
      // Initialize any system constructs (such as mutexes) here
      OS_ERR err;
      BSP_Init();

//      enableExternalMissiles(FALSE);

      OSSemCreate(&Enemy_done_control,"Enemy_done_control",1,&err);  // start from a control task
      OSSemCreate(&Enemy_done_sensing,"Enemy_done_sensing",0,&err); //get the sensing info

      OSMutexCreate(&Enemy_MControl1,"control1",&err);
      OSMutexCreate(&Enemy_MControl2,"control2",&err);
      OSMutexCreate(&Enemy_MSensing1,"sensing1",&err);
      OSMutexCreate(&Enemy_MSensing2,"sensing2",&err);

      OSMutexCreate(&Enemy_MReverse1,"reverse1",&err);
      OSMutexCreate(&Enemy_MReverse2,"reverse2",&err);


      // Perform any user-required initializations here
      Enemy_tank1 = create_tank(teamNumber,"enemy");
      create_turret(Enemy_tank1);
      Enemy_tankID1 = initialize_tank(Enemy_tank1);

      Enemy_tank2 = create_tank(teamNumber,"enemy");
      create_turret(Enemy_tank2);
      Enemy_tankID2 = initialize_tank(Enemy_tank2);

      register_user(Enemy_control_task,5,0);
      register_user(Enemy_sensing_task,5,0);
}
