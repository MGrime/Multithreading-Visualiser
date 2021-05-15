// THIS IS NOT MINE. TAKEN FROM THE SPRITE_ENGINE LAB IN CO4302
// Implements Timer, a cross-platform timer class using SDL

#include "timer.h"
#include <sdl.h>

namespace msc {  namespace platform {

// Default constructor.
// Parameter: start (optional), true if the timer should start counting immediately.
// Throws: std::runtime_error if SDL timer subsystem cannot be created
Timer::Timer(bool start /*= true*/) :
	mSDL(SDL_INIT_TIMER),
	mFrequency(SDL_GetPerformanceFrequency()),
	mIsRunning(start)
{
	Reset();
}

//---------------------------------------------------------------------------------------------------------------------

// Start the timer counting from it's current time, does not start a new lap
void Timer::Start()
{
	if (!mIsRunning)
	{
		mIsRunning = true;

		// Internal timers ignore the time passed while stopped
		uint64_t time = SDL_GetPerformanceCounter();
		mStartTime    += time - mStopTime;
		mLapStartTime += time - mStopTime;
	}
}

// Stop the timer counting, does not reset timer to zero or start a new lap
void Timer::Stop()
{
	if (mIsRunning)
	{
		mIsRunning = false;
		mStopTime = SDL_GetPerformanceCounter();
	}
}

// Reset the timer to zero and start a new lap, does not start or stop the timer
void Timer::Reset()
{
	// Reset start, lap and stop times to current time
	mStartTime = SDL_GetPerformanceCounter();
	mLapStartTime = mStartTime;
	mStopTime     = mStartTime;
}

//---------------------------------------------------------------------------------------------------------------------

// Return time counted in seconds
float Timer::GetTime() const
{
	uint64_t time = (mIsRunning ? SDL_GetPerformanceCounter() : mStopTime);
	double countTime = static_cast<double>(time - mStartTime) / static_cast<double>(mFrequency);
	return static_cast<float>(countTime);
}

// Return time counted in ticks. See GetTickFrequency
uint64_t Timer::GetTimeTicks() const
{
	uint64_t time = (mIsRunning ? SDL_GetPerformanceCounter() : mStopTime);
	return time - mStartTime;
}

// Return time counted in seconds for the current lap and start a new lap
float Timer::GetLapTime()
{
	uint64_t time = (mIsRunning ? SDL_GetPerformanceCounter() : mStopTime);
	double lapTime = static_cast<double>(time - mLapStartTime) / static_cast<double>(mFrequency);
	mLapStartTime = time;
	return static_cast<float>(lapTime);
}

// Return time counted in ticks for the current lap and start a new lap. @see GetTickFrequency
uint64_t Timer::GetLapTimeTicks()
{
	uint64_t time = (mIsRunning ? SDL_GetPerformanceCounter() : mStopTime);
	uint64_t lapTimeTicks = time - mLapStartTime;
	mLapStartTime = time;
	return lapTimeTicks;
}


} } // namespaces
