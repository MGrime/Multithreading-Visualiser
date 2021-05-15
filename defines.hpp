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

// Will use improved algorithm
// Line sweep can handle easily 2 million in the same time it takes standard to do 100k
#define _LINE_SWEEP_

// Will pause after each "frame" i.e. each time all circles processed
// Note will mess with _TIME_LOOPS_ - Results will not be accurate
//#define _PAUSE_AFTER_EACH_FRAME_

#pragma endregion

#pragma region CONSTANTS

constexpr unsigned int NUM_OF_CIRCLES = 2000000;
constexpr unsigned int NUM_STATIONARY_CIRCLES = NUM_OF_CIRCLES / 2;
constexpr unsigned int NUM_MOVING_CIRCLES = NUM_OF_CIRCLES / 2;

const Vector2f X_SPAWN_RANGE = Vector2f(-1000.0f, 1000.0f);
const Vector2f Y_SPAWN_RANGE = Vector2f(-1000.0f, 1000.0f);

const Vector2f X_VELOCITY_RANGE = Vector2f(-5.0f, 5.0f);
const Vector2f Y_VELOCITY_RANGE = Vector2f(-5.0f, 5.0f);

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