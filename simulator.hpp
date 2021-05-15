#pragma once
#include <array>

#include "defines.hpp"
#include "libraries/timer.h"

#ifdef _USE_TL_ENGINE_

#include <TL-Engine.h>

#endif

class simulator
{
public:
	simulator(uint32_t seed = 10000u);

	~simulator();
	
	void run();

private:
	#pragma region CIRCLE DATA
	// Arrays are synchronized. Index 2 in unique + collision array is same circle
	// Typedefs in defines.hpp because they get really LONG

	// Array of data to process stationary circles in collision
	stationary_collision_array	m_StationaryCollisionData = stationary_collision_array();
	// Other data for stationary circles when outputting
	stationary_unique_array		m_StationaryUniqueData = stationary_unique_array();
	// Array to protect HP of stationary circles
	stationary_mutex_array		m_StationaryMutexes;
	
	// Array of data to process moving circles in collision
	moving_collision_array		m_MovingCollisionData = moving_collision_array();
	// Other data for moving circles when outputting
	moving_unique_array			m_MovingUniqueData = moving_unique_array();

	#pragma endregion

	#pragma region THREAD POOL
	// Maximum possible thread pool size
	static const uint32_t MAX_WORKERS = 31u;

	// Array of up to max workers
	std::array<paired_worker, MAX_WORKERS> m_CollisionWorkers;

	// Actual amount of threads in use
	uint32_t m_NumWorkers = 0u;
	#pragma endregion

	#pragma region FUNCTIONS

	void check_collision(uint32_t threadIndex);
	void process_collision(collision_work* work);
	// As above but with a line sweep algorithm
	void process_collision_sweep(collision_work* work);
	
	#pragma endregion

	#ifdef _TIME_LOOPS_
	#pragma region INSTRUMENTATION

	msc::platform::Timer m_Timer;
	
	#pragma endregion
	#endif

	#ifdef _USE_TL_ENGINE_

	// Engine variables
	tle::I3DEngine* m_TLEngine;
	tle::ICamera* m_TLCamera;

	// Load different coloured squares for each type
	tle::IMesh* m_StationaryMesh;
	tle::IMesh* m_MovingMesh;

	// Array to store model instnaces
	std::array<tle::IModel*, NUM_STATIONARY_CIRCLES> m_StationaryCircleModels;
	std::array<tle::IModel*, NUM_MOVING_CIRCLES> m_MovingCirclesModels;
	
	// Updates and draws frame
	void update_tl();
	
	#endif

};

