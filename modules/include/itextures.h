/* the texture loader interface. 
    It provides a facility for loading graphics and mapping individual tiles (cels)
    to indices that are subsequently used in the gps.screen and gps.texpos arrays 
    
*/

#include "ideclspec.h"

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

#if defined (DFMODULE_BUILD) || defined(DFMODULE_IMPLICIT_LINK)
extern "C" DECLSPEC itextures * APIENTRY gettextures(void);
#else // using glue and runtime loading.
extern "C" DECLSPEC itextures * APIENTRY (*gettextures)(void);
#endif