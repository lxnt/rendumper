/* the texture loader interface.
    A facility for loading graphics and mapping individual tiles (cels)
    to indices that are subsequently used in the buffer_t::screen
    and buffer_t::texpos arrays.

    buffer_t::screen has the textures with indices 0-255, this is the
    'tileset' aka 'font'.

    buffer_t::texpos has the rest. These come from the 'graphicset',
    the creature graphics.

    Tiles/cels are assigned an index, starting from 0, in the order
    of their load.

    The methods are always called in the simuloop thread, from under
    mainloop(). reset() and following reload of textures is done in
    single mainloop() call.

    Supported image file formats must include BMP and PNG.
*/

#include "itypes.h"

struct itextures {
    virtual void release() = 0;

    /* Copy a texture to another spot; return assigned index. */
    virtual long clone_texture(long src) = 0;

    /* Convert a texture to grayscale in place. */
    virtual void grayscale_texture(long pos) = 0;

    /* Load multiple textures from an image file assuming there are dimx x dimy of them.
       Resulting texture pixel dimensions are returned in (*disp_x, *disp_y).
       Assigned indices are written into tex_pos[].
       If convert_magenta is true, and the image data has no alpha channel, assume
       pixels of color #FF00FF fully transparent, and the rest fully opaque */
    virtual void load_multi_pdim(const char *filename, long *tex_pos, long dimx, long dimy,
                                   bool convert_magenta, long *disp_x, long *disp_y) = 0;

    /* Load an image file as a single texture; return assigned index. */
    virtual long load(const char *filename, bool convert_magenta) = 0;

    /* Index sequence is not affected; a noop, or just free underlying storage. */
    virtual void delete_texture(long pos) = 0;

    /* Interface to the renderers. to be redesigned. */
    virtual df_texalbum_t *get_album() = 0;
    virtual void release_album(df_texalbum_t *) = 0;

    /* Drop everything, reset index. Does not invalidate any texalbums already given out.
        Note that this is not equivalent to async_cmd::reset_textures,
        which just simply "reuploaded" them, whatever that meant for the current renderer. */
    virtual void reset(void) = 0;

    /* Load a 16x16 cel page from a PNG stream held in the given buffer.
        This calls reset().
        If load() or load_multi_pdim() get called while an rcfont is loaded, it is dropped, using
        reset() again. Intended for simple tests only, use load_multi_pdim() otherwise. */
    virtual void set_rcfont(const void *, int) = 0;
};

#if defined (DFMODULE_BUILD)
extern "C" DFM_EXPORT itextures * DFM_APIEP gettextures(void);
#else // using glue and runtime loading.
extern gettextures_t gettextures;
#endif