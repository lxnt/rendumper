#define DFMODULE_BUILD
#include "isimuloop.h"

class sl_implementation : public isimuloop {
  private:
    Uint32 thread_id;
  
    bool quit;
    bool paused;
    unsigned long simticks;
  
    vvfunc_t render_things;
    vvfunc_t mainloop;
    viifunc_t gps_resize;
  
  public:
    implementation() : quit(false), paused(true), simticks(0) {}
    void release(void) = 0;
    
    /* supplies the gps, render_things() and mainloop() 
       to the simulation loop object/thread implementation. */
    void set_callbacks(vvfunc_t render_things, vvfunc_t mainloop, viifunc_t gps_resize);
    
    /* fps control used for movies */
    int  get_fps(void) = 0; // returns actual calculated value (ema).
    void set_fps(int fps) = 0; 

    /* execution control for the direct access to the memory 
       has non-recursive lock semantics */
    void pause(void) = 0; // does not return until the loop is paused
    void unpause(void) = 0;
    
    /* simulation frame counter */
    int simticks(void);
    
    /* if the simulation quitted */
    bool quit(void);
    
    /* instrumentation */
    int render_time(void) = 0;
    int simulate_time(void) = 0; 

};