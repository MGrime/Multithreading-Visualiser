// THIS IS NOT MINE. TAKEN FROM THE SPRITE_ENGINE LAB IN CO4302

// Declares Timer, a cross-platform timer class using SDL

#ifndef MSC_PLATFORM_TIMER_H_INCLUDED
#define MSC_PLATFORM_TIMER_H_INCLUDED

#include "sdl_init.h"
#include <stdint.h>

namespace msc {  namespace platform {

// Platform independent timer class using SDL.
//
// Operates like a stopwatch with a lap timer. Start and Stop do not reset the counter, use Reset for that. Lap
// times can be fetched without affecting the main counter. Time can be fetched in seconds or ticks (see GetTickFrequency).
//
// Supports copy/move semantics.
class Timer
{
	//---------------------------------------------------------------------------------------------------------------------
  // Construction
	//---------------------------------------------------------------------------------------------------------------------
public:
  // Default constructor.
  // Parameter: start (optional), true if the timer should start counting immediately.
  // Throws: std::runtime_error if SDL timer subsystem cannot be created
  Timer(bool start = true);


	//---------------------------------------------------------------------------------------------------------------------
  // Public Interface
	//---------------------------------------------------------------------------------------------------------------------
public:
  // Return frequency of the tick used internally by the timer, the smallest unit of time it can measure 
  uint64_t GetTickFrequency() const  { return mFrequency; }

  //---------------------------------------------------------------------------------------------------------------------
  
  // Start the timer counting from it's current time, does not start a new lap
  void Start();

  // Stop the timer counting, does not reset timer to zero or start a new lap
  void Stop();

  // Reset the timer to zero and start a new lap, does not start or stop the timer
  void Reset(); 

  //---------------------------------------------------------------------------------------------------------------------

  // Return time counted in seconds
  float GetTime() const;

  // Return time counted in ticks. See GetTickFrequency
  uint64_t GetTimeTicks() const;

  // Return time counted in seconds for the current lap and start a new lap
  float GetLapTime();     

  // Return time counted in ticks for the current lap and start a new lap. See GetTickFrequency
  uint64_t GetLapTimeTicks();


	//---------------------------------------------------------------------------------------------------------------------
  // Data
	//---------------------------------------------------------------------------------------------------------------------
private:
  SDL mSDL;

  uint64_t mFrequency;    // Internal timer frequency.
  uint64_t mStartTime;    // Count at which timer last started.
  uint64_t mLapStartTime; // Count at which last lap time was taken.
  uint64_t mStopTime;     // Count at which timer last stopped.
  bool mIsRunning;        // true if the timer is running.
};

} } // namespaces

#endif // Header guard
