#include "simulator.hpp"

#include "libraries/threadstream.hpp"

#include <algorithm>

simulator::simulator(uint32_t seed)
{
	#pragma region SIMULATION SETUP
	// Init a number generator
	std::default_random_engine rng(seed);

	// Create distributions from data in constants.hpp
	const auto positionXDist = rand_float_dist(X_SPAWN_RANGE.x(), X_SPAWN_RANGE.y());
	const auto positionYDist = rand_float_dist(Y_SPAWN_RANGE.x(), Y_SPAWN_RANGE.y());
	const auto velocityXDist = rand_float_dist(X_VELOCITY_RANGE.x(), X_VELOCITY_RANGE.y());
	const auto velocityYDist = rand_float_dist(Y_VELOCITY_RANGE.x(), Y_VELOCITY_RANGE.y());

	// RGB is 0-1
	const auto colorDist = rand_float_dist(0.0f, 1.0f);

	// Setup stationary circles
	for (auto i = 0u; i < NUM_STATIONARY_CIRCLES; ++i)
	{
		// Get array refs
#ifdef _LINE_SWEEP_
		auto& sColData = m_StationaryCollisionData.at(i);
#else
		auto& sColData = m_StationaryCollisionData.at(i);
#endif
		// Collision setup
		sColData.position = Vector2f(positionXDist(rng), positionYDist(rng));
		sColData.radius = 1.0f;
	}

	// Sort stationary circles to allow line sweep
	std::sort(m_StationaryCollisionData.begin(), m_StationaryCollisionData.end(), [](auto& a, auto& b)
	{
		return a.position.x() < b.position.x();
	});

	// Now setup unique data for sorted collision circles
	for (auto i = 0u; i < NUM_STATIONARY_CIRCLES; ++i)
	{
		auto& sUniqueData = m_StationaryUniqueData.at(i);
		sUniqueData.color = Vector3f(colorDist(rng), colorDist(rng), colorDist(rng));
		sUniqueData.hp = 100;
		sUniqueData.name = "S" + std::to_string(i);

		// Store reference to unique array if using better algorithm
	#ifdef _LINE_SWEEP_
		auto& uniqueIndex = m_StationaryCollisionData.at(i).uniqueIndex;
		uniqueIndex = i;
	#endif
		
	}

	// Setup moving circles
	for (auto i = 0u; i < NUM_MOVING_CIRCLES; ++i)
	{
		// Get array refs
		auto& mColData = m_MovingCollisionData.at(i);
		auto& mUniqueData = m_MovingUniqueData.at(i);

		// Collision setup
		mColData.position = Vector2f(positionXDist(rng), positionYDist(rng));
		mColData.velocity = Vector2f(velocityXDist(rng), velocityYDist(rng));
		mColData.radius = 1.0f;
		
		// Unique setup
		mUniqueData.color = Vector3f(colorDist(rng), colorDist(rng), colorDist(rng));
		mUniqueData.hp = 100;
		mUniqueData.name = "M" + std::to_string(i);
	}
	#pragma endregion

	#pragma region THREADING SETUP
	// Work out hardware threads if possible
	m_NumWorkers = std::thread::hardware_concurrency();
	// Sometimes it doesn't work so assume 8
	if (m_NumWorkers == 0) m_NumWorkers = 8;
	// Main thread already running
	--m_NumWorkers;
	TOUT << "Using " << m_NumWorkers << " threads!\n";
	// Setup threads
	for (uint32_t i = 0; i < m_NumWorkers; ++i)
	{
		auto& pairedWorker = m_CollisionWorkers.at(i);
		pairedWorker.worker.thread = std::thread(&simulator::check_collision, this, i);
	}
	
	#pragma endregion

	#ifdef _TIME_LOOPS_
	#pragma region INSTRUMENTATION SETUP

	m_Timer = msc::platform::Timer(false);
	
	#pragma endregion
	#endif
	
}

simulator::~simulator()
{
	for (auto i = 0u; i < m_NumWorkers; ++i)
	{
		auto& pairedWorker = m_CollisionWorkers.at(i);
		pairedWorker.worker.thread.detach();
	}
}

void simulator::run()
{
	#ifdef _TIME_LOOPS_
	m_Timer.Start();
	#endif
	while (true)
	{
		// Update positions single threaded
		for (auto i = 0u; i < NUM_MOVING_CIRCLES; ++i)
		{
			auto& mColData = m_MovingCollisionData.at(i);
			mColData.position += mColData.velocity;
		}

		// Check collisions threaded

		// These pointers will move
		auto* movingColPointer = m_MovingCollisionData.data();
		auto* movingUniquePointer = m_MovingUniqueData.data();
		
		for (auto i = 0u; i < m_NumWorkers; ++i)
		{
			auto& pairedWorker = m_CollisionWorkers.at(i);

			pairedWorker.work.sCirclesCol = m_StationaryCollisionData.data();
			pairedWorker.work.sCirclesUnique = m_StationaryUniqueData.data();
			pairedWorker.work.sCirclesMutexes = m_StationaryMutexes.data();
			pairedWorker.work.sNumberOfCircles = NUM_STATIONARY_CIRCLES;

			pairedWorker.work.mCirclesCol = movingColPointer;
			pairedWorker.work.mCircleUnique = movingUniquePointer;
			pairedWorker.work.mNumberOfCircles = static_cast<uint32_t>(m_MovingCollisionData.size() / m_NumWorkers);

			// Flag the work as incomplete
			{
				std::unique_lock<std::mutex> l(pairedWorker.worker.lock);
				pairedWorker.work.complete = false;
			}

			// Notify
			pairedWorker.worker.workReady.notify_one();

			// Move on
			movingColPointer += pairedWorker.work.mNumberOfCircles;
			movingUniquePointer += pairedWorker.work.mNumberOfCircles;
		}

		// Process remaning on main thread
		const uint32_t remainingCircles = static_cast<uint32_t>(m_MovingCollisionData.size()) - static_cast<uint32_t>(movingColPointer - m_MovingCollisionData.data());
		auto mainThreadWork = collision_work();

		mainThreadWork.sCirclesCol = m_StationaryCollisionData.data();
		mainThreadWork.sCirclesUnique = m_StationaryUniqueData.data();
		mainThreadWork.sCirclesMutexes = m_StationaryMutexes.data();
		mainThreadWork.sNumberOfCircles = NUM_STATIONARY_CIRCLES;


		mainThreadWork.mCirclesCol = movingColPointer;
		mainThreadWork.mCircleUnique = movingUniquePointer;
		mainThreadWork.mNumberOfCircles = remainingCircles;

		// Process
		process_collision(&mainThreadWork);

		// Wait for workers to finish
		for (auto i = 0u; i < m_NumWorkers; ++i)
		{
			auto& pairedWorker = m_CollisionWorkers.at(i);
			{
				std::unique_lock <std::mutex> l(pairedWorker.worker.lock);
				pairedWorker.worker.workReady.wait(l, [&]() {return pairedWorker.work.complete; });
			}
		}

		// Output macro dependent how long the loop took
		#ifdef _TIME_LOOPS_

		const float timeToProcess = m_Timer.GetLapTime();
		TOUT << "Processed " << NUM_OF_CIRCLES << " circles in " << timeToProcess << '\n';
		
		#endif

		#ifdef  _PAUSE_AFTER_EACH_FRAME_
		// Wait for input
		std::cin.get();
		
		#endif
		
		
	}
}

void simulator::check_collision(uint32_t threadIndex)
{
	auto& pairedWorker = m_CollisionWorkers.at(threadIndex);

	while (true)
	{
		// Acquire the mutex
		{
			std::unique_lock<std::mutex> l(pairedWorker.worker.lock);
			pairedWorker.worker.workReady.wait(l, [&]() { return !pairedWorker.work.complete; });
		}

		// Do work
#ifdef _LINE_SWEEP_
		process_collision_sweep(&pairedWorker.work);
#else
		process_collision(&work);
#endif

		{
			std::unique_lock<std::mutex> l(pairedWorker.worker.lock);
			pairedWorker.work.complete = true;
		}

		pairedWorker.worker.workReady.notify_one();
	}
	
}

void simulator::process_collision(collision_work* work)
{
	// Check each moving circle in this section
	for (auto i = 0u; i < work->mNumberOfCircles; ++i)
	{
		auto& mColData = work->mCirclesCol[i];
		auto& mUniqueData = work->mCircleUnique[i];

		// check every single stationary circle
		for (auto j = 0u; j < work->sNumberOfCircles; ++j)
		{
			// Get reference to static data
			auto& sColData = work->sCirclesCol[j];
			auto& sUniqueData = work->sCirclesUnique[j];
			auto& sMutex = work->sCirclesMutexes[j];

			// Workout distance between and compare to radius combined
			const Vector2f dxy = sColData.position - mColData.position;
			const auto distance = dxy.norm();
			// If true then collision occurred
			if (distance < mColData.radius + sColData.radius)
			{
				// Lose health. Lock stationary health change behind mutex
				mUniqueData.hp -= 20;
				{
					std::unique_lock<std::mutex> l(sMutex);
					sUniqueData.hp -= 20;
				}

				// Reflect moving circles velocity
				const auto norm = dxy.normalized();
				mColData.velocity = mColData.velocity - 2.0f * norm * mColData.velocity.dot(norm);

				// Output macro dependent
			#ifdef _OUTPUT_ALL_
				TOUT << mUniqueData.name << " HP: " << mUniqueData.hp << " hit " << sUniqueData.name << " HP: " << sUniqueData.hp << '\n';
			#endif				
			}
			
		}
		
	}
}

void simulator::process_collision_sweep(collision_work* work)
{
	// This function compiles to nothing when the macro is disabled
#ifdef _LINE_SWEEP_
	for (auto i = 0u; i < work->mNumberOfCircles; ++i)
	{
		auto& mColData = work->mCirclesCol[i];
		auto& mUniqueData = work->mCircleUnique[i];

		// Pre-calculate
		const float blo = -mColData.radius;
		const float bro = mColData.radius;

		const float sx = mColData.position.x();
		const float sl = sx - mColData.radius;
		const float sr = sx + mColData.radius;

		const float sr_blo = sr - blo;
		const float sl_bro = sl - bro;

		// Perform line sweep binary search to find stationary circles that are overlapping
		auto s = work->sCirclesCol;
		auto e = work->sCirclesCol + work->sNumberOfCircles;
		stationary_circle_data* circleFound;
		bool found = false;
		do
		{
			circleFound = s + (e - s) / 2;
			if (sr_blo <= circleFound->position.x())
			{
				e = circleFound;
			}
			else if (sl_bro >= circleFound->position.x())
			{
				s = circleFound;
			}
			else
			{
				found = true;
			}
		} while (!found && e - s > 1);

		if (found)
		{
			auto stationaryToStart = circleFound;
			// Sweep right
			while (sr_blo > stationaryToStart->position.x() && stationaryToStart != work->sCirclesCol + work->sNumberOfCircles)
			{
				const Vector2f dxy = stationaryToStart->position - mColData.position;
				const auto distance = dxy.norm();
				if (distance < mColData.radius + stationaryToStart->radius)
				{
					mUniqueData.hp -= 20;
					{
						std::unique_lock<std::mutex> l(work->sCirclesMutexes[stationaryToStart->uniqueIndex]);
						work->sCirclesUnique[stationaryToStart->uniqueIndex].hp -= 20;
					}
					
					// Reflect moving circles velocity
					const auto norm = dxy.normalized();
					mColData.velocity = mColData.velocity - 2.0f * norm * mColData.velocity.dot(norm);

				#ifdef _OUTPUT_ALL_
					TOUT << mUniqueData.name << " HP: " << mUniqueData.hp << " hit " << work->sCirclesUnique[stationaryToStart->uniqueIndex].name << " HP: " << work->sCirclesUnique[stationaryToStart->uniqueIndex].hp << '\n';
				#endif
				}

				
				++stationaryToStart;
			}

			stationaryToStart = circleFound;
			// Sweep left
			while (stationaryToStart-- != work->sCirclesCol && sl_bro < stationaryToStart->position.x())
			{
				const Vector2f dxy = stationaryToStart->position - mColData.position;
				const auto distance = dxy.norm();
				if (distance < mColData.radius + stationaryToStart->radius)
				{
					mUniqueData.hp -= 20;
					{
						std::unique_lock<std::mutex> l(work->sCirclesMutexes[stationaryToStart->uniqueIndex]);
						work->sCirclesUnique[stationaryToStart->uniqueIndex].hp -= 20;
					}

					// Reflect moving circles velocity
					const auto norm = dxy.normalized();
					mColData.velocity = mColData.velocity - 2.0f * norm * mColData.velocity.dot(norm);

#ifdef _OUTPUT_ALL_
					TOUT << mUniqueData.name << " HP: " << mUniqueData.hp << " hit " << work->sCirclesUnique[stationaryToStart->uniqueIndex].name << " HP: " << work->sCirclesUnique[stationaryToStart->uniqueIndex].hp << '\n';
#endif
				}
			}
			
		}
		
	}
#endif
}
