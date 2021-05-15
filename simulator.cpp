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
#ifdef _RANDOM_RADIUS_
		sColData.radius = rand_float_dist(CIRCLE_RADIUS_RANGE.x(), CIRCLE_RADIUS_RANGE.y())(rng);
#else
		sColData.radius = 1.0f;
#endif
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
#ifdef _RANDOM_RADIUS_
		mColData.radius = rand_float_dist(CIRCLE_RADIUS_RANGE.x(), CIRCLE_RADIUS_RANGE.y())(rng);
#else
		mColData.radius = 1.0f;
#endif
		
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

	#ifdef _USE_TL_ENGINE_
	
	// Create engine instance
	m_TLEngine = New3DEngine(tle::kTLX);
	m_TLEngine->StartWindowed(1000,1000);

	// load default media folder
	m_TLEngine->AddMediaFolder(R"(.\media)");

	// Load meshes
	m_StationaryMesh = m_TLEngine->LoadMesh("Stationary.x");
	m_MovingMesh = m_TLEngine->LoadMesh("Moving.x");

	// Create models
	int index = 0;
	for (auto& stationary : m_StationaryCollisionData)
	{
		m_StationaryCircleModels.at(index) = m_StationaryMesh->CreateModel(stationary.position.x(), stationary.position.y(), 0.0f);
		m_StationaryCircleModels.at(index)->Scale(0.5f);
		++index;
	}
	index = 0;
	for (auto& moving : m_MovingCollisionData)
	{
		m_MovingCirclesModels.at(index) = m_MovingMesh->CreateModel(moving.position.x(), moving.position.y(), 0.0f);
		m_MovingCirclesModels.at(index)->Scale(0.5f);
		++index;
	}

	// Need to scale accordingly. We can simply scale by radius multiplicatively
	#ifdef _RANDOM_RADIUS_

	index = 0;
	for (auto& stationary : m_StationaryCircleModels)
	{
		stationary->Scale(0.5f * m_StationaryCollisionData.at(index).radius);
		++index;
	}
	index = 0;
	for (auto& moving : m_MovingCirclesModels)
	{
		moving->Scale(0.5f * m_MovingCollisionData.at(index).radius);
		++index;
	}
	
	#endif
	
	// Create camera
	m_TLCamera = m_TLEngine->CreateCamera(tle::ECameraType::kManual, CAMERA_DEFAULT_POSITION.x(), CAMERA_DEFAULT_POSITION.y(), CAMERA_DEFAULT_POSITION.z());
	
	#endif
	
}

simulator::~simulator()
{
	for (auto i = 0u; i < m_NumWorkers; ++i)
	{
		auto& pairedWorker = m_CollisionWorkers.at(i);
		pairedWorker.worker.thread.detach();
	}
	#ifdef _USE_TL_ENGINE_

	m_TLEngine->Stop();
	m_TLEngine->Delete();
	
	#endif
}

void simulator::run()
{
	m_Timer.Start();

	// Program loop is handled by different parts depending on if visual is running
	while (
#ifdef _USE_TL_ENGINE_
		m_TLEngine->IsRunning()
#else
		true
#endif
		)
	{
		// Storing time in a static variable because might want to use last frametime in next frame
		static float timeToProcess = 0.0f;
		
		#ifdef _USE_TL_ENGINE_

		// Handle pause input seperately
		handle_tl_pause();
		// If paused
		if (m_IsPaused)
		{
			timeToProcess = m_Timer.GetLapTime();
			update_tl(timeToProcess);
			// Skip loop
			continue;
		}
		
		#endif
		
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

			// Reset number of collisions
			#ifdef _TRACK_COLLISIONS_

			pairedWorker.work.numberOfCollisions = 0u;

			#endif

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

		// Reset number of collisions
		#ifdef _TRACK_COLLISIONS_

		mainThreadWork.numberOfCollisions = 0u;

		#endif

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

		// Get time without macro. This is because we need it for TL Engine
		timeToProcess = m_Timer.GetLapTime();

		#ifdef _USE_TL_ENGINE_
		
		// Update visualiser
		update_tl(timeToProcess);

		#endif
		
		// Output macro dependent how long the loop took

		// Want to output time
		#ifdef _TIME_LOOPS_
			#ifdef _TRACK_COLLISIONS_
				uint32_t totalCollisions = 0u;

				for (auto& pairedWorker : m_CollisionWorkers)
				{
					totalCollisions += pairedWorker.work.numberOfCollisions;
				}
		
				TOUT << "Processed " << NUM_OF_CIRCLES << " circles in " << timeToProcess << " Total Collisions: " << totalCollisions << '\n';
			#else
				TOUT << "Processed " << NUM_OF_CIRCLES << " circles in " << timeToProcess << '\n';
			#endif
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
		process_collision(&pairedWorker.work);
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

				// Track how many collision this thread handles
				#ifdef _TRACK_COLLISIONS_

				work->numberOfCollisions++;
				
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
#ifdef _RANDOM_RADIUS_
		const float rightBound = mColData.position.x() + (2.0f * CIRCLE_RADIUS_RANGE.y());
		const float leftBound = mColData.position.x() - (2.0f * CIRCLE_RADIUS_RANGE.y());
#else
		const float rightBound = mColData.position.x() + mColData.radius + mColData.radius;
		const float leftBound = mColData.position.x() - mColData.radius - mColData.radius;
#endif

		// Perform line sweep binary search to find stationary circles that are overlapping
		auto s = work->sCirclesCol;
		auto e = work->sCirclesCol + work->sNumberOfCircles;
		stationary_circle_data* circleFound;
		bool found = false;
		do
		{
			circleFound = s + (e - s) / 2;

			
			if (rightBound <= circleFound->position.x())
			{
				e = circleFound;
			}
			else if (leftBound >= circleFound->position.x())
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
			while (rightBound > stationaryToStart->position.x() && stationaryToStart != work->sCirclesCol + work->sNumberOfCircles)
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

					// Track how many collision this thread handles
					#ifdef _TRACK_COLLISIONS_

					work->numberOfCollisions++;

					#endif
				}
				
				++stationaryToStart;
			}

			stationaryToStart = circleFound;
			// Sweep left
			while (stationaryToStart-- != work->sCirclesCol && leftBound < stationaryToStart->position.x())
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

					// Track how many collision this thread handles
					#ifdef _TRACK_COLLISIONS_

					work->numberOfCollisions++;

					#endif
				}
			}
			
		}
		
	}
#endif
}

#ifdef _USE_TL_ENGINE_
void simulator::update_tl(float deltaTime)
{
	#pragma region UPDATE VISUALISATION
	// Update model positions
	int index = 0;
	for (auto& moving : m_MovingCollisionData)
	{
		m_MovingCirclesModels.at(index)->SetPosition(moving.position.x(), moving.position.y(), 0.0f);
		++index;
	}
	#pragma endregion

	#pragma region UPDATE CAMERA
	// Get current positions
	auto currentXPos = m_TLCamera->GetX();
	auto currentYPos = m_TLCamera->GetY();
	auto currentZPos = m_TLCamera->GetZ();

	// ZOOM	
	if (m_TLEngine->KeyHeld(tle::Key_E))
	{
		currentZPos += CAMERA_ZOOM_SPEED * deltaTime;
	}
	if (m_TLEngine->KeyHeld(tle::Key_Q))
	{
		currentZPos -= CAMERA_ZOOM_SPEED * deltaTime;
	}

	// MOVEMENT
	if (m_TLEngine->KeyHeld(tle::Key_W))
	{
		currentYPos += CAMERA_MOVE_SPEED * deltaTime;
	}
	if (m_TLEngine->KeyHeld(tle::Key_S))
	{
		currentYPos -= CAMERA_MOVE_SPEED * deltaTime;
	}
	if (m_TLEngine->KeyHeld(tle::Key_A))
	{
		currentXPos -= CAMERA_MOVE_SPEED * deltaTime;
	}
	if (m_TLEngine->KeyHeld(tle::Key_D))
	{
		currentXPos += CAMERA_MOVE_SPEED * deltaTime;
	}

	
	// RESET
	if (m_TLEngine->KeyHeld(tle::Key_R))
	{
		currentXPos = CAMERA_DEFAULT_POSITION.x();
		currentYPos = CAMERA_DEFAULT_POSITION.y();
		currentZPos = CAMERA_DEFAULT_POSITION.z();
	}

	m_TLCamera->SetPosition(currentXPos, currentYPos, currentZPos);
	#pragma endregion
	
	// Draw frame
	m_TLEngine->DrawScene();
	
}

void simulator::handle_tl_pause()
{
	if (m_TLEngine->KeyHit(tle::Key_P))
	{
		m_IsPaused = !m_IsPaused;
	}
}
#endif
