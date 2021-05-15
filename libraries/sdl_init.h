// THIS IS NOT MINE. TAKEN FROM THE SPRITE_ENGINE LAB IN CO4302
// Declares SDL class, a class that provides RAII access to the SDL library
// 
// SDL provides platform independence for many system tasks. However it is a C library and benefits from encapsulation.
// @todo Makes assumptions about subsystem dependencies.
//       Necessary to get around fact the that subsystem state after a failed SDL_Initxxx call is impossible to determine.
//       Also assumes SDL_SetMainReady is safe to call even if SDL was allowed to override main.
 
#ifndef MSC_PLATFORM_SDL_INIT_H_INCLUDED
#define MSC_PLATFORM_SDL_INIT_H_INCLUDED

#include <stdint.h>

namespace msc {  namespace platform {

// Encapsulate SDL initialisation. Constructing an SDL object makes available SDL subsytems without impacting on SDL
// use elsewhere in an application. Similarly destruction will fully clean up without affecting other SDL code.
// Ideal for RAII use.
// 
// On Windows, OSX and Linux, no additional bootstrap code is required. On mobile platforms SDL must be allowed to
// override main as discussed in the SDL documentation. If this class is used in a library then call SpecifyLibraryUse
// and ensure SDL_Quit is called before the main app terminates.
// 
// Supports copy semantics @todo but not move semantics yet
class SDL
{
	//---------------------------------------------------------------------------------------------------------------------
  // Construction
	//---------------------------------------------------------------------------------------------------------------------
public:
  // Constructor
  // Parameter: subSystems, subsystems required - use flags from SDL initialisation functions.
  // Parameter: throwException (optional), set true if exception on failure is required, otherwise call
  //            GetActiveSubSystems after construction to see which subsystems were successfully initialised.
  // Throws: std::runtime_error if throwException is true and one or more subsystems cannot be activated.
  SDL(uint32_t subSystems, bool throwException = true);

  ~SDL();

  // Copy constructor. Create an object with the same available subsystems as an existing one. Objects will be independent
  SDL(SDL const& o);

  // Assignment operator. Updates available subsystems to match those of other object. Objects remain independent.
  SDL& operator=(SDL const& o);


	//---------------------------------------------------------------------------------------------------------------------
  // Public Interface
	//---------------------------------------------------------------------------------------------------------------------
public:
  // Return all the subsystems activated from this object (including dependencies) using subsystems flags from SDL
  // initialisation functions.
  uint32_t GetActiveSubSystems(void) const  { return(mActiveSubSystems); }

  // Activate a set of SDL subsytems (in addition to the current active list of subsystems). Check which subsystems
  // were successfully activated with @ref GetActiveSubSystems. Must release subsystems with @ref ReleaseSubSystems.
  // 
  // Parameter: newSubSystems, the additional subsystems required - use flags from SDL initialisation functions.
  //
  // Returns: The requested subsystems that were successfully activated. More may actually be activated due to
  // dependencies but this value will only list those that were requested.
  uint32_t ActivateSubSystems(uint32_t newSubSystems);

  // Release a set of subsystems created by ActivateSubSystems.
  // Parameter: releaseSubSystems, the subsystems to release - use flags from SDL initialisation functions.
  void ReleaseSubSystems(uint32_t releaseSubSystems);


  // Specify whether this code class is being used from a library (i.e. exiting does not imply application end)
  // Assumes it is not if this function is not called
  static void SpecifyLibraryUse(bool isLibrary)  { mIsLibrary = isLibrary; }


	//---------------------------------------------------------------------------------------------------------------------
  // Data
	//---------------------------------------------------------------------------------------------------------------------
private:
  static const uint32_t mSubSystems[];  // List of available subsystems in SDL in dependency order
  static const uint32_t mNumSubSystems; // Number of subsystems in the list above

  static bool mIsLibrary; // Whether this class is being used from a library (i.e. exiting does not imply application end)
  static bool mFirstUse;  // True if no objects of this class have been created before

  uint32_t mActiveSubSystems; // Subsystems successfully activated by this object. 
};


} } // namespaces

#endif // Header guard
