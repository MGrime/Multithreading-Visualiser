#pragma once

#include <random>
#include <condition_variable>
#include <string>
#include <thread>

#include <Eigen/Core>
using Eigen::Vector2f;
using Eigen::Vector3f;

#pragma region MACROS

// Will output result of each collision
// Note will mess with _TIME_LOOPS_ - Results will not be accurate
//#define _OUTPUT_ALL_ 

// Will output time to complete each loop
#define _TIME_LOOPS_

// Will track how many collisions each frame
#define _TRACK_COLLISIONS_

// Will use improved algorithm
// Line sweep can handle easily 2 million in the same time it takes standard to do 100k
#define _LINE_SWEEP_

// Will pause after each "frame" i.e. each time all circles processed
// Note will mess with _TIME_LOOPS_ - Results will not be accurate
//#define _PAUSE_AFTER_EACH_FRAME_

// Will use TL Engine to show a visualization of the simulation
// NOTE - Timing is NOT accurate when visualization is setup. Also _OUTPUT_ALL_ will massively hurt renderer frame-rate
// Also TL-Engine input will be REALLY laggy and strange
// Lower number of circles if performance is bad on rendering. You can also increase the Spawn range to reduce multiple collisions per frame
#define _USE_TL_ENGINE_

// will randomise radiuses of circles
#define _RANDOM_RADIUS_

#pragma endregion

#pragma region CONSTANTS

constexpr unsigned int	NUM_OF_CIRCLES = 100000;
constexpr unsigned int	NUM_STATIONARY_CIRCLES = NUM_OF_CIRCLES / 2;
constexpr unsigned int	NUM_MOVING_CIRCLES = NUM_OF_CIRCLES / 2;
constexpr uint32_t		SPAWN_SEED = 10000u;

const Vector2f X_SPAWN_RANGE = Vector2f(-2000.0f, 2000.0f);
const Vector2f Y_SPAWN_RANGE = Vector2f(-2000.0f, 2000.0f);

const float CAMERA_START_Z = -((X_SPAWN_RANGE.y() - X_SPAWN_RANGE.x() + (Y_SPAWN_RANGE.y() - Y_SPAWN_RANGE.x())) / 2.0f);

const Vector2f X_VELOCITY_RANGE = Vector2f(-5.0f, 5.0f);
const Vector2f Y_VELOCITY_RANGE = Vector2f(-5.0f, 5.0f);

#ifdef _USE_TL_ENGINE_

const float CAMERA_MOVE_SPEED = 100.0f;
const float CAMERA_ZOOM_SPEED = 1000.0f;

// Typically -1250 back from the spawn range for best results
const Vector3f CAMERA_DEFAULT_POSITION = Vector3f(0.0f, 0.0f, CAMERA_START_Z - 1250.0f);

#endif

#ifdef _RANDOM_RADIUS_

const Vector2f CIRCLE_RADIUS_RANGE = Vector2f(1.0f, 5.0f);

#endif

#pragma endregion

#pragma region CIRCLE STRUCTS

struct stationary_circle_data
{
	Vector2f	position = Vector2f(0.0f, 0.0f);
	float		radius = 1.0f;
#ifdef _LINE_SWEEP_
	size_t		uniqueIndex = 0u; // Line sweep requires this structure to have a index to the unique array
#endif
};

struct moving_circle_data
{
	Vector2f	position = Vector2f(0.0f, 0.0f);
	Vector2f	velocity = Vector2f(0.0f, 0.0f);
	float		radius = 1.0f;
};

struct circle_unique_data
{
	std::string name;
	int32_t		hp = 100;
	Vector3f	color = Vector3f(1.0f, 1.0f, 1.0f);
};

#pragma endregion

#pragma region THREADING STRUCTS

// The worker pool contains these types
// Wakes up on signal, signals back when complete
// Mutex guards data
struct worker_thread
{
	std::thread				thread;
	std::condition_variable workReady;
	std::mutex				lock;
};

// This is the structure used by the worker threads to process a collision
struct collision_work
{
	// Used to avoid suprious wake ups
	bool complete = true;

	// Pointer to full array of stationary circles
	stationary_circle_data* sCirclesCol = nullptr;
	circle_unique_data* sCirclesUnique = nullptr;
	std::mutex* sCirclesMutexes = nullptr;
	size_t					sNumberOfCircles = 0u;

	// Pointer to moving circles section
	moving_circle_data* mCirclesCol = nullptr;
	circle_unique_data* mCircleUnique = nullptr;
	size_t					mNumberOfCircles = 0u;
	
	#ifdef _TRACK_COLLISIONS_
	// how many collision happened in this threads work
	uint32_t numberOfCollisions = 0u;
	#endif
	
};

// Not sure if ill need this but nice to have
struct paired_worker
{
	worker_thread	worker;
	collision_work	work;
};

#pragma endregion

#pragma region USING SHORTENERS

// Shorten super long array types
typedef std::array<stationary_circle_data, NUM_STATIONARY_CIRCLES>	stationary_collision_array;
typedef std::array<circle_unique_data, NUM_STATIONARY_CIRCLES>		stationary_unique_array;
typedef std::array<std::mutex, NUM_STATIONARY_CIRCLES>				stationary_mutex_array;
typedef std::array<moving_circle_data, NUM_MOVING_CIRCLES>			moving_collision_array;
typedef std::array<circle_unique_data, NUM_MOVING_CIRCLES>			moving_unique_array;

// Shorten horrid random syntax
typedef std::uniform_real_distribution<float> rand_float_dist;

#pragma endregion