#ifndef __BATTLEFIELD_HEADER_H__
#define __BATTLEFIELD_HEADER_H__

#define MINX 20
#define MINY 20
#define MAXX 800
#define MAXY 700
#define STEER_LIMIT 0.785398163f  // Maximum possible wheel angle (45 deg)
#define ACCELERATE_LIMIT 5.0f
#define MAX_MISSILES 3
#define NUM_TANKS 1

// Represents a point in 2D space
typedef struct { int x, y; } Point;

// Represents size of the tank
typedef struct { int width, length; } Size;

// Represents a rectangle
typedef struct { Point bottom_left, bottom_right, top_left, top_right; } Rect;

// Represents a tank/turret and all its defining characteristics
typedef int Tank;
typedef int Turret;

// Sensory information about battlefield tanks
typedef struct
{
    int numTanks;                          // Number of additional tanks
    int numMissiles;                       // Number of missiles
    int tankIDs[NUM_TANKS];                // IDs of tanks
    float tankBearings[NUM_TANKS];         // Relative directions of tanks
    float tankDistances[NUM_TANKS];        // Distances to tanks
    int tankHealths[NUM_TANKS];            // Percent health remaining of tanks
    float missileBearings[MAX_MISSILES];   // Relative directions of missiles
    float missileDistances[MAX_MISSILES];  // Distances to missiles
} RadarData;

// Senses and returns a list of tanks and missiles in the battlefield
//   If tank does not exist (has been destroyed), this will return false
bool poll_radar(Tank* tank, RadarData* data);

// Sets tank acceleration - may be positive, negative, or 0
bool accelerate(Tank* tank, float new_acceleration);

// Indicates that tank is moving in reverse
bool set_reverse(Tank* tank, bool reverse);

// Indicates a new angle to orient the wheels for steering
bool set_steering(Tank* tank, float angle);

// Returns the current position of the tank
Point get_position(Tank* tank);

// Returns the current speed of the tank in m/s
float get_speed(Tank* tank);

// Returns the heading (angle with respect to the positive x-axis) of the
//  tank in positive radians - more positive moves counter-clockwise
float get_heading(Tank* tank);

// Returns whether the tank has run into an impassable object
bool is_stuck(Tank* tank);

// Create a new tank
//  Set 'teamNumber' to integer 1 and 'teamName' to your last name (in quotes)
Tank* create_tank(char teamNumber, const char* teamName);

// Returns the rectangle occupied by the specified tank or tank configuration
Rect get_tank_area(Tank* tank);

// Creates a turret for the specified tank
Turret* create_turret(Tank* tank);

// Returns the angle (in radians) of a turret relative to its corresponding tank
float get_turret_angle(Tank* tank);

// Sets the angle (in radians) of a turret relative to its corresponding tank
//  Returns false if angle is outside of legal limits
bool set_turret_angle(Tank* tank, float angle);

// Fires a missile from the specified tank's turret
//  Must wait 1.5 seconds to reload between firings
//  Returns true if firing was successful, else false
bool fire(Tank* tank);

// Enable generation of external missiles
//  This is enabled by default
void enableExternalMissiles(bool enable);

// Registers a user function and returns user id
int register_user(void(*f)(void*), unsigned int priority, void* param);

// Initializes tank and returns its ID
int initialize_tank(Tank* tank);
void initialize(bool partII);

#endif      // #ifndef __BATTLEFIELD_HEADER_H__
