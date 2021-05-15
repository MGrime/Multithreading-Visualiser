// THIS IS NOT MINE. TAKEN FROM THE SPRITE_ENGINE LAB IN CO4302
/*
 *  Implements SDL class, encapsulating SDL library initialisation
 *  
 *  SDL provides platform independence for many system tasks. However it is a C library and benefits from encapsulation.
 */
#include "sdl_init.h"
#include <sdl.h>
#include <stdexcept>
#include <stdlib.h>

namespace msc {  namespace platform {

// Constructor
// Parameter: subSystems, subsystems required - use flags from SDL initialisation functions.
// Parameter: throwException (optional), set true if exception on failure is required, otherwise call
//            GetActiveSubSystems after construction to see which subsystems were successfully initialised.
// Throws: std::runtime_error if throwException is true and one or more subsystems cannot be activated.
SDL::SDL(uint32_t subSystems, bool throwException /*= true*/) : mActiveSubSystems(0) // Important to set initial subsytems to 0 because ActivateSubSystems updates it
{
  if (mFirstUse)
  {
    SDL_SetMainReady();                  // Needed in case SDL was prevented from redefining main() for the app
    if (!mIsLibrary)  atexit(SDL_Quit);  // Final cleanup for SDL on app exit, but not on library exit (that case must be managed in main app)
    mFirstUse = false;
  }
  ActivateSubSystems(subSystems);
  if (throwException && (mActiveSubSystems & subSystems) != subSystems)
  {
    ReleaseSubSystems(mActiveSubSystems);
    throw std::runtime_error("SDL subsystem initialisation failure");
  }
}

SDL::~SDL()
{
  ReleaseSubSystems(mActiveSubSystems);
}

// Copy constructor. Create an object with the same available subsystems as an existing one. Objects will be independent.
SDL::SDL(SDL const& o) : mActiveSubSystems(0) // Important to set initial subsytems to 0 because ActivateSubSystems updates it
{
  ActivateSubSystems(o.mActiveSubSystems);
}

// Assignment operator. Updates available subsystems to match those of other object. Objects remain independent.
SDL& SDL::operator=(SDL const& o)
{
  ReleaseSubSystems(mActiveSubSystems & ~o.mActiveSubSystems);
  ActivateSubSystems(o.mActiveSubSystems);
  return *this;
}


//---------------------------------------------------------------------------------------------------------------------

// Activate a set of SDL subsytems (in addition to the current active list of subsystems). Check which subsystems
// were successfully activated with @ref GetActiveSubSystems. Must release subsystems with @ref ReleaseSubSystems.
// 
// Parameter: newSubSystems, the additional subsystems required - use flags from SDL initialisation functions.
//
// Returns: The requested subsystems that were successfully activated. More may actually be activated due to
// dependencies but this value will only list those that were requested.
uint32_t SDL::ActivateSubSystems(uint32_t newSubSystems)
{
  // Disgusting hacks required to deal with deficiencies in SDL state reporting. Subsystems are initialised one at a
  // time so a failure does not affect the initialisation of other subsystems. However, undocumented subsystem
  // dependencies cause problems, a failed subsystem may have succesfully initialised dependencies and there is no
  // reliable way to check which in SDL. So subsystem dependencies are placed first in the mSubSystems array. Only
  // attempt to initialise a subsystem with dependencies if its dependencies already succeeded. If it fails we know
  // the dependencies only had their reference counts incremented in that call, which we assume would have succeeded.
  // From there we can release all unneeded dependency initialisations correctly in the case of a failed subsystem.
  // 
  // Result is that the subsystems can be reliably released regardless of failure of individial subsystems. 
  
  uint32_t reqSubSystems = mActiveSubSystems | newSubSystems; // Store requirements prior to adding dependencies (including subsytems previously initialised)
    
  // Add dependencies. Will be initialised multiple times, but only way to reliably check result of failed SDL_Initxxx call
  uint32_t fullSubSystems = newSubSystems;
  if ((fullSubSystems & SDL_INIT_GAMECONTROLLER) != 0)  fullSubSystems |= SDL_INIT_JOYSTICK;
  if ((fullSubSystems & SDL_INIT_VIDEO) != 0 ||
    (fullSubSystems & SDL_INIT_JOYSTICK) != 0)        fullSubSystems |= SDL_INIT_EVENTS;

  fullSubSystems &= ~mActiveSubSystems; // Remove requirements that have already been initialised by this object

  for (uint32_t i = 0; i < mNumSubSystems; i++)
  {
    if ((fullSubSystems & mSubSystems[i]) != 0)
    {
      // Don't try to initialise systems whose dependencies already failed
      if ((mSubSystems[i] == SDL_INIT_VIDEO || mSubSystems[i] == SDL_INIT_JOYSTICK) && (mActiveSubSystems & SDL_INIT_EVENTS) == 0) continue;
      if (mSubSystems[i] == SDL_INIT_GAMECONTROLLER && (mActiveSubSystems & SDL_INIT_JOYSTICK) == 0) continue;
      if (SDL_InitSubSystem(mSubSystems[i]) == 0)
      {
        mActiveSubSystems += mSubSystems[i];
      }
      else
      {
        // Decrease reference counts on dependencies of failed subsystem
        if (mSubSystems[i] == SDL_INIT_VIDEO || mSubSystems[i] == SDL_INIT_JOYSTICK)
        {
          ReleaseSubSystems(SDL_INIT_EVENTS);
          // More releases required if dependencies weren't required in themselves (...)
          if ((reqSubSystems & SDL_INIT_EVENTS) == 0)
          {
            ReleaseSubSystems(SDL_INIT_EVENTS);
            mActiveSubSystems -= SDL_INIT_EVENTS;
          }
        }
        if (mSubSystems[i] == SDL_INIT_GAMECONTROLLER)
        {
          ReleaseSubSystems(SDL_INIT_JOYSTICK);
          // More releases required if dependencies weren't required in themselves (...)
          if ((reqSubSystems & SDL_INIT_JOYSTICK) == 0)
          {
            ReleaseSubSystems(SDL_INIT_JOYSTICK);
            mActiveSubSystems -= SDL_INIT_JOYSTICK;
          }
          if ((reqSubSystems & SDL_INIT_EVENTS) == 0)
          {
            ReleaseSubSystems(SDL_INIT_EVENTS);
            mActiveSubSystems -= SDL_INIT_EVENTS;
          }
        }
      }
    }
  }
  return mActiveSubSystems & newSubSystems;
}

// Release a set of subsystems created by ActivateSubSystems.
// Parameter: releaseSubSystems, the subsystems to release - use flags from SDL initialisation functions.
void SDL::ReleaseSubSystems(uint32_t releaseSubSystems)
{
  releaseSubSystems &= mActiveSubSystems; // Ignore subsystems that this object has not activated

  for (uint32_t i = 0; i < mNumSubSystems; i++)
  {
    if ((releaseSubSystems & mSubSystems[i]) != 0)
    {
      SDL_QuitSubSystem(mSubSystems[i]);
    }
  }
  mActiveSubSystems -= releaseSubSystems;
}


//---------------------------------------------------------------------------------------------------------------------

bool SDL::mIsLibrary = false; // Whether this class is being used from a library (i.e. exiting does not imply application end)
bool SDL::mFirstUse  = true;  // True if no objects of this class have been created before

// List of subsystems in SDL in dependency order
// Order is important, reflects dependencies within SDL. E.g. initialising VIDEO implies EVENTS will also be initialised
const uint32_t SDL::mSubSystems[] = { SDL_INIT_EVENTS, SDL_INIT_VIDEO, SDL_INIT_JOYSTICK, SDL_INIT_GAMECONTROLLER,
                                      SDL_INIT_TIMER, SDL_INIT_AUDIO, SDL_INIT_HAPTIC };
const uint32_t SDL::mNumSubSystems = sizeof(mSubSystems) / sizeof(mSubSystems[0]);


} } // namespaces

