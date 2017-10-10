#include <includes.h>
#include <string.h>
#include "BattlefieldLib.h"


/*
*********************************************************************************************************
*                                       LOCAL GLOBAL VARIABLES
*********************************************************************************************************
*/

#define PI 3.14159265358979323846

// tank and sensor data
Tank* DTran_tran1; 
Tank* DTran_tran2;
Tank* DTran_enemy1;
Tank* DTran_enemy2;


typedef struct {int num; int ID[3]; int bear_abs[3]; int direction[3]; int distance_danger[3]; int bear_danger[3]; float bear_delta[3]; float old_bear[3]; int numSamples;} tank_missile_data; // indicate for other tanks or missiles
typedef struct {float steer; float acc; float turret_angle; bool fire; int numEnemies;} control_data;
typedef struct {bool stuck; bool stuck_risk; bool hit_risk; bool danger;} avoid_risk_data; 
typedef struct {int ID; int team_mate_ID; bool status; bool reverse ;Point position; float heading; float speed; int direction; float turret_angle;} tank_info;

typedef struct {tank_info myTank; avoid_risk_data risk; RadarData sensor; tank_missile_data otherTanks; tank_missile_data missiles; control_data control;} info; 

info DTran_tran1_data;
info DTran_tran2_data; 


float DTran_R_tank = 10; // radius of tank used to determine distance danger level
int DTran_xL = 350;
int DTran_yL = 350;
int DTran_xH = 550;
int DTran_yH = 450;
int DTran_numSamples = 1000;

int DTran_ID1;
int DTran_ID2;



bool DTran_reverse1; 
bool DTran_reverse2;


// semaphore for controlling the whole system
static OS_SEM DTran_control_finish;
static OS_SEM DTran_sensing_finish;
static OS_SEM DTran_checkdanger_finish; 

/*
*********************************************************************************************************
*                                      LOCAL FUNCTION PROTOTYPES
*********************************************************************************************************
*/


int direction_determine(float angle){

  int direction = 2*angle/PI; 
 
  return direction;
}

float abs_bearing(float angle){
  float b; 
  
 if (angle > 3*PI/2){
  b = 2*PI - angle;
  }
  else if (PI< angle <=3*PI/2){
  b = angle - PI;
  }
  
  else if ( PI/2 < angle <= PI){
  b = PI - angle;
  }
  else if ( angle <= PI/2){
  b = angle;
  }
  
  return b;
}

int distance_danger_check(float missile_distance){
 int level; 
 
 if(missile_distance > 30*DTran_R_tank){
    level = 0; // no dangerous
 }
 if ( 20*DTran_R_tank < missile_distance <= 30*DTran_R_tank){
    level = 1; // low dangerous
 }
 if (10*DTran_R_tank < missile_distance <= 20*DTran_R_tank){
    level = 2; // medium dangerous
 }
 if (missile_distance <= 10*DTran_R_tank){
    level = 3; // high dangerous
 }
 
  return level; 
}

int bearing_danger_check (float missile_abs_bearing){
 int level; 
 
 if (missile_abs_bearing > 0.785){
  level = 0; // no dangerous
 }
 if ( 0.5 < missile_abs_bearing <= 0.785){
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

float steering_offset(int missile_direction){
 
  float offset;
  if ((missile_direction == 0) || (missile_direction == 2)){
     offset = -0.3;
  }
  else if ((missile_direction == 1) || (missile_direction == 3)){
     offset = 0.3 ;
  }
  return offset; 
}

int max(int a, int b){
int max; 

if (a >= b){
max = a;
}
else max = b;

return max;
}

float minf(float a, float b){
float min; 
if(a <=b){
 min = a;
}
else min = b;
  
return min;
}

control_data avoid_stuck(info INFO){
control_data ctr; 
int x = INFO.myTank.position.x;
int y = INFO.myTank.position.y; 
int d = INFO.myTank.direction; 

int xL = DTran_xL;
int xH = DTran_xH;
int yL = DTran_yL; 
int yH = DTran_yH; 

float os = 0.3;

  if (x < xL)  {
    if (d == 0) ctr.steer = -os; 
    if (d == 1) ctr.steer = -2*os ;
    if (d == 2) ctr.steer = 2*os;
    if (d == 3) ctr.steer = os;
  }
  if (x > xH){
    if (d == 0) ctr.steer = 2*os;
    if (d == 1) ctr.steer = os;
    if (d == 2) ctr.steer = -os;
    if (d == 3) ctr.steer = -2*os;
  }
  if (y < yL) {
    if (d == 0) ctr.steer = -os;
    if (d == 1) ctr.steer = -2*os;
    if (d == 2) ;
    if (d == 3) ;
  }
  
   if (y > yH) {
    if (d == 0) ;
    if (d == 1) ;
    if (d == 2) ctr.steer = -1*os;
    if (d == 3) ctr.steer = -2*os;
  }
  
ctr.acc = 1;

return ctr;

}

control_data avoid_missile (info INFO){
  
control_data ctr; 

int m[3]; // missile direction
int d[3]; // distance danger level
int b[3]; // bearing danger level
 
int temp;
int bmax;

for(int i = 0; i<3; i++){
m[i] = INFO.missiles.direction[i];
d[i] = INFO.missiles.distance_danger[i];
b[i] = INFO.missiles.bear_danger[i];
}

 switch(INFO.missiles.num){

  case 0: 
    ctr.steer = 0;
    ctr.acc = 1;  
    break;
  
  case 1: 
    ctr.steer = steering_offset(m[0]);  
    ctr.acc = d[0] + 1; 
    break;
  
  case 2: 
    temp = abs(m[1] - m[0]);
    if(temp == 0){
      ctr.steer = steering_offset(m[0]);  
      ctr.acc = max(d[0],d[1]) + 1; 
    }
  
    if(temp == 1){
      bmax = max(b[1],b[0]); 
      if (bmax == b[0]) ctr.steer = steering_offset(m[0]);
      if (bmax == b[1]) ctr.steer = steering_offset(m[1]);
      ctr.acc = max(d[0],d[1]) + 1;
       
    }
  
    if(temp == 2){
      ctr.steer = steering_offset(m[0]);
      ctr.acc = max(d[0],d[1]) + 1;
    }
    if(temp == 3){
      bmax = max(b[1],b[0]); 
      if (bmax == b[0]) ctr.steer = steering_offset(m[0]);
      if (bmax == b[1]) ctr.steer = steering_offset(m[1]);
        ctr.acc = max(d[0],d[1]) + 1;    
    } 
  
    break;

  case 3: 

    if((m[0] == m[1]) && (m[1] == m[2])){
      ctr.steer = steering_offset(m[0]);
      ctr.acc = max(d[0],max(d[1],d[2]))+1; 
    } 
    else  {
      bmax = max(b[0],max(b[1],b[2]));
      if (bmax == b[0]) ctr.steer = steering_offset(m[0]);
      if (bmax == b[1]) ctr.steer = steering_offset(m[1]);
      if (bmax == b[2]) ctr.steer = steering_offset(m[2]);
      ctr.acc = max(d[0],max(d[1],d[2]))+1;
    }
    
  
  break; 
 }

return ctr; 

}

control_data avoid_hit (info INFO){
control_data ctr; 

int n = INFO.otherTanks.num; 

int t[3]; // tank direction
int d[3]; // distance danger level
float l[3]; // distance

int dmax;
float lmin;

for(int i = 0; i< n; i++){
  t[i] = INFO.otherTanks.direction[i];
  d[i] = INFO.otherTanks.distance_danger[i];
  l[i] = INFO.sensor.tankDistances[i];
}

if (n == 1) {
  ctr.steer = steering_offset(t[0]); 
  ctr.acc = d[0] + 1;
} 

if (n == 2) {
  lmin = minf(l[0],l[1]); 
  dmax = max(d[0],d[1]);
  if (lmin == l[0]) ctr.steer = steering_offset(t[0]); 
  if (lmin == l[1]) ctr.steer = steering_offset(t[1]);
  ctr.acc = dmax + 1; 
} 

if (n==3){
  lmin = minf(l[0], minf(l[1],l[2]));
  dmax = max(d[0], max(d[1],d[2]));
  
  if (lmin == l[0]) ctr.steer = steering_offset(t[0]); 
  if (lmin == l[1]) ctr.steer = steering_offset(t[1]);
  if (lmin == l[2]) ctr.steer = steering_offset(t[2]);
  ctr.acc = dmax + 1; 
}

return ctr; 

}

control_data shoot(info INFO){

  control_data ctr; 
 
  int d[2];
  float l[2];
  float a[2]; // shooting angle
  float offset = 0.05;
  float os; 
  int dmin;
  float lmin; 
  int j = 0; 
  
  
  for (int i = 0; i< INFO.otherTanks.num; i++){
    if ((INFO.otherTanks.ID[i]!= INFO.myTank.ID) && (INFO.otherTanks.ID[i]!= INFO.myTank.team_mate_ID)){
        d[j] = INFO.otherTanks.distance_danger[i];
        l[j] = INFO.sensor.tankDistances[i];
        if (INFO.otherTanks.bear_delta[i] > 0) os = (3-d[j])*offset;
        else os = -(3-d[j])*offset; 
        a[j] = INFO.sensor.tankBearings[i] + os ;
        j++;
    }
   
  } 
  
  ctr.numEnemies = j;
  
  if (ctr.numEnemies == 0){
    ctr.fire = FALSE; 
    ctr.turret_angle = 0;
  }
  if (ctr.numEnemies == 1){
    ctr.turret_angle = a[0];
    ctr.fire = TRUE;
  }
  if (ctr.numEnemies == 2){
    lmin = minf(l[0],l[1]); 
    if (lmin == l[0]) ctr.turret_angle = a[0]; 
    if (lmin == l[1]) ctr.turret_angle = a[1];
    ctr.fire = TRUE;
  }

// if (INFO.myTank.reverse == TRUE) ctr.turret_angle = - ctr.turret_angle;  
  
  return ctr;
}

info control(Tank* tankname, info INFO){
 control_data ctr1, ctr2, ctr3, ctr4, ctr;
 
 
 ctr1 = avoid_missile(INFO);
 ctr2 = avoid_hit(INFO);
 ctr3 = avoid_stuck(INFO); 
 ctr4 = shoot(INFO);
 
 if (INFO.risk.danger){
    ctr = ctr1;
 }
 else if (INFO.risk.hit_risk){
    ctr = ctr2;
 }
 else if (INFO.risk.stuck_risk){
    ctr = ctr3;
 }
 
 INFO.control.acc = ctr.acc;
 INFO.control.steer = ctr.steer; 
 INFO.control.turret_angle = ctr4.turret_angle; 
 INFO.control.fire = ctr4.fire;
 INFO.control.numEnemies = ctr4.numEnemies;
 
 
 set_steering(tankname, INFO.control.steer); 
 accelerate(tankname, INFO.control.acc); 
 set_turret_angle(tankname,INFO.control.turret_angle);
 if (INFO.control.fire == TRUE) fire(tankname); 
  
 return INFO;
}

info sensing(Tank* tankname){
  
  info INFO;
  int i;
  
  
  INFO.myTank.status = poll_radar(tankname, &INFO.sensor);
  INFO.myTank.position = get_position(tankname); 
  INFO.myTank.heading = get_heading (tankname); 
  INFO.myTank.speed = get_speed(tankname); 
  INFO.myTank.direction = direction_determine(INFO.myTank.heading);
  INFO.risk.stuck = is_stuck(tankname);
  INFO.missiles.num = INFO.sensor.numMissiles;
  INFO.otherTanks.num = INFO.sensor.numTanks;
  INFO.myTank.turret_angle = get_turret_angle(tankname);
  
  if (tankname == DTran_tran1){
    INFO.myTank.ID = DTran_ID1;
    INFO.myTank.team_mate_ID = DTran_ID2; 
  }
  else if(tankname == DTran_tran2){
    INFO.myTank.ID = DTran_ID2; 
    INFO.myTank.team_mate_ID = DTran_ID1;
  }


// reset missile and other tanks direction and absolute bearing
  for (int i = 0; i< 3; i++){
    INFO.missiles.direction[i] = 0;
    INFO.missiles.bear_abs[i] = 0;
    INFO.otherTanks.direction[i] = 0;
    INFO.otherTanks.bear_abs[i] = 0;
  }   
  
  // read missile direction and absolute bearing
   for (int i = 0; i< INFO.missiles.num ; i++){
     INFO.missiles.direction[i] = direction_determine(INFO.sensor.missileBearings[i]);
     INFO.missiles.bear_abs[i] = abs_bearing(INFO.sensor.missileBearings[i]);
   }  
  
  // read other tanks direction and absolute bearing and bearing change
  
   for (int i = 0; i< INFO.otherTanks.num ; i++){
     INFO.otherTanks.ID[i] = INFO.sensor.tankIDs[i];
     INFO.otherTanks.direction[i] = direction_determine(INFO.sensor.tankBearings[i]);
     INFO.otherTanks.bear_abs[i] = abs_bearing(INFO.sensor.tankBearings[i]);
   } 
  
  // read other tanks and missiles bearing change speed 
    if(INFO.otherTanks.numSamples < DTran_numSamples){
      INFO.otherTanks.numSamples++;
      for (int i=0; i < INFO.otherTanks.num; i++){
        INFO.otherTanks.bear_delta[i] = INFO.sensor.tankBearings[i] - INFO.otherTanks.old_bear[i];
        INFO.missiles.bear_delta[i]  = INFO.sensor.missileBearings[i] - INFO.missiles.old_bear[i];
      }
    }  
    else{
      INFO.otherTanks.numSamples = 0;
      for (int i = 0; i< INFO.otherTanks.num ; i++){
        INFO.otherTanks.old_bear[i] = INFO.sensor.tankBearings[i];
        INFO.missiles.old_bear[i] = INFO.sensor.missileBearings[i];
      } 
    }    
  
  // check the risk of getting stuck 
  
  if ((INFO.myTank.position.x < DTran_xL) || (INFO.myTank.position.x > DTran_xH) ||  (INFO.myTank.position.y < DTran_yL) || (INFO.myTank.position.y > DTran_yH))  {
     INFO.risk.stuck_risk = TRUE;
  }
  else INFO.risk.stuck_risk = FALSE;
  
  // check danger
    
       // reset danger check
       for(int i = 0; i<3;i++){
       INFO.missiles.distance_danger[i] = 0;
       INFO.missiles.bear_danger[i] = 0;
       INFO.otherTanks.distance_danger[i] = 0;
       INFO.otherTanks.bear_danger[i] = 0;
       }
       
       // check danger from missiles
       for (int i = 0; i <INFO.missiles.num;i++){
        INFO.missiles.distance_danger[i] = distance_danger_check(INFO.sensor.missileDistances[i]);
        INFO.missiles.bear_danger[i] = bearing_danger_check(INFO.missiles.bear_abs[i]);
        
       }
       
       // check danger from other tanks
        for (int i = 0; i <INFO.otherTanks.num;i++){
        INFO.otherTanks.distance_danger[i] = distance_danger_check(INFO.sensor.tankDistances[i]);
        INFO.otherTanks.bear_danger[i] = bearing_danger_check(INFO.otherTanks.bear_abs[i]);
        
       }
       
       for (int i = 0; i < INFO.missiles.num; i++){
         if ((INFO.missiles.distance_danger[i] >= 2))  {
          INFO.risk.danger = TRUE;           
         }
         else {
          INFO.risk.danger = FALSE;        
         }               
       }
           
       // check hit risk
       
       for (int i = 0; i < INFO.otherTanks.num; i++){
         if ((INFO.otherTanks.distance_danger[i] >= 3))  {
          INFO.risk.hit_risk = TRUE;           
         }
         else {
          INFO.risk.hit_risk = FALSE;        
         }               
       }
      

  
	
 return INFO;	
	
}


static void DTran_sensing_task(void* p_arg){
  CPU_TS ts;
  OS_ERR err;

    while (DEF_TRUE) {
      
       OSSemPend(&DTran_control_finish,0,OS_OPT_PEND_BLOCKING,&ts,&err);
       
       DTran_tran1_data = sensing(DTran_tran1); 
       DTran_tran2_data = sensing(DTran_tran2);


       BSP_LED_Off(0);
              
       OSSemPost(&DTran_sensing_finish,OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED,&err);      
    }

}

static void DTran_checkdanger_task(void* p_arg){
  CPU_TS ts;
  OS_ERR err;

  
  while (DEF_TRUE) {
    
       OSSemPend(&DTran_sensing_finish,0,OS_OPT_PEND_BLOCKING,&ts,&err);
       
       if (DTran_tran1_data.risk.danger) BSP_LED_On(1); 
       else BSP_LED_Off(1);
       
       if (DTran_tran2_data.risk.danger) BSP_LED_On(2);
       else BSP_LED_Off(2);
       
     
       OSSemPost(&DTran_checkdanger_finish,OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED,&err);      
    }   
}

static void DTran_control_task(void* p_arg){
  CPU_TS ts;
  OS_ERR err;
  
  
  while (DEF_TRUE) {
       OSSemPend(&DTran_checkdanger_finish,0,OS_OPT_PEND_BLOCKING,&ts,&err);
       
        if (is_stuck(DTran_tran1)){  
           DTran_reverse1 = !DTran_reverse1;
           set_reverse(DTran_tran1,DTran_reverse1);           
       }
       else DTran_tran1_data.myTank.reverse = DTran_reverse1; 
       if (is_stuck(DTran_tran2)){  
           DTran_reverse2 = !DTran_reverse2;
           set_reverse(DTran_tran2,DTran_reverse2);
       }
       else DTran_tran2_data.myTank.reverse = DTran_reverse2;       
       
       DTran_tran1_data = control(DTran_tran1, DTran_tran1_data);  
       DTran_tran2_data = control(DTran_tran2, DTran_tran2_data);
       
       
       OSSemPost(&DTran_control_finish,OS_OPT_POST_1 | OS_OPT_POST_NO_SCHED,&err);      
    } 
  
}
// Student initialization function
void DTran_init(char teamNumber)
{  
    
   
    // Initialize any system constructs (such as mutexes) here
       OS_ERR err;
       OSSemCreate(&DTran_control_finish,"DTran_control_finish",1,&err);
       OSSemCreate(&DTran_sensing_finish,"DTran_sensing_finish",0,&err);
       OSSemCreate(&DTran_checkdanger_finish,"DTran_checkdanger_finish",0,&err);
       
    
    // Perform any user-required initializations here 
       DTran_tran1 = create_tank(1,"tran");
       DTran_tran2 = create_tank(1,"tran"); 
       create_turret(DTran_tran1);
       create_turret(DTran_tran2);
 
       DTran_ID1 = initialize_tank(DTran_tran1);
       DTran_ID2 = initialize_tank(DTran_tran2);
       

       for(int i=0; i<3 ; i++){
          DTran_tran1_data.otherTanks.old_bear[i] = 0;
          DTran_tran1_data.missiles.old_bear[i] = 0;
          DTran_tran2_data.otherTanks.old_bear[i] = 0;
          DTran_tran2_data.missiles.old_bear[i] = 0;
       }
       
  
     // enableExternalMissiles(FALSE);
       BSP_Init();
    
    // Register user functions here
       
      register_user(DTran_control_task,5,0);
      register_user(DTran_sensing_task,5,0);
      register_user(DTran_checkdanger_task,5,0);
}
