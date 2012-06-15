/* for the time being, use SDL's begin_code.h for the DECLSPEC (DFAPI here)
   which defines necessary dllimport/export stuff */
#if defined(DFAPI_EXPORT) // building the dlls
# define BUILD_SDL
#endif
#include "begin_code.h"
#include "end_code.h" // struct pack forcing is of no use.
#define DFAPI DECLSPEC
#define APIENTRY __stdcall

/* the texture loader interface. 
    It provides a facility for loading graphics and mapping individual tiles (cels)
    to indices that are subsequently used in the gps.screen and gps.texpos arrays 
    
*/

struct itextures {
    virtual void release() = 0;

    virtual long clone_texture(long src) = 0;
    virtual void grayscale_texture(long pos) = 0;
    virtual void load_multi_pdim(const char *filename, long *tex_pos, long dimx, long dimy,
                                   bool convert_magenta, long *disp_x, long *disp_y) = 0;
    virtual long load(const char *filename, bool convert_magenta) = 0;
    virtual void delete_texture(long pos) = 0;
    virtual void reset(void) = 0;
};
extern "C" DFAPI itextures * APIENTRY gettextures(void);
