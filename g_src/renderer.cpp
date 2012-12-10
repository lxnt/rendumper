
#include "iplatform.h"
#define DFM_STUB(foo) getplatform()->log_error("Stub '%s' called.\n", #foo)

#include "init.h"
#include "enabler.h"
#include "renderer.h"

Either<texture_fullid,texture_ttfid> renderer::screen_to_texid(int x, int y) {
  const int tile = x * gps.dimy + y;
  const unsigned char *s = screen + tile*4;

  struct texture_fullid ret;
  int ch;
  int bold;
  int fg;
  int bg;

  // TTF text does not get the full treatment.
  if (s[3] == GRAPHICSTYPE_TTF) {
    texture_ttfid texpos = *((unsigned int *)s) & 0xffffff;
    return Either<texture_fullid,texture_ttfid>(texpos);
  } else if (s[3] == GRAPHICSTYPE_TTFCONT) {
    // TTFCONT means this is a tile that does not have TTF anchored on it, but is covered by TTF.
    // Since this may actually be stale information, we'll draw it as a blank space,
    ch = 32;
    fg = bg = bold = 0;
  } else {
    // Otherwise, it's a normal (graphical?) tile.
    ch   = s[0];
    bold = (s[3] != 0) * 8;
    fg   = (s[1] + bold) % 16;
    bg   = s[2] % 16;
  }
  
  static bool use_graphics = init.display.flag.has_flag(INIT_DISPLAY_FLAG_USE_GRAPHICS);
  
  if (use_graphics) {
    const long texpos             = screentexpos[tile];
    const char addcolor           = screentexpos_addcolor[tile];
    const unsigned char grayscale = screentexpos_grayscale[tile];
    const unsigned char cf        = screentexpos_cf[tile];
    const unsigned char cbr       = screentexpos_cbr[tile];

    if (texpos) {
      ret.texpos = texpos;
      if (grayscale) {
        ret.r = enabler.ccolor[cf][0];
        ret.g = enabler.ccolor[cf][1];
        ret.b = enabler.ccolor[cf][2];
        ret.br = enabler.ccolor[cbr][0];
        ret.bg = enabler.ccolor[cbr][1];
        ret.bb = enabler.ccolor[cbr][2];
      } else if (addcolor) {
        goto use_ch;
      } else {
        ret.r = ret.g = ret.b = 1;
        ret.br = ret.bg = ret.bb = 0;
      }
      goto skip_ch;
    }
  }
  
  ret.texpos = enabler.is_fullscreen() ?
    init.font.large_font_texpos[ch] :
    init.font.small_font_texpos[ch];
 use_ch:
  ret.r = enabler.ccolor[fg][0];
  ret.g = enabler.ccolor[fg][1];
  ret.b = enabler.ccolor[fg][2];
  ret.br = enabler.ccolor[bg][0];
  ret.bg = enabler.ccolor[bg][1];
  ret.bb = enabler.ccolor[bg][2];

 skip_ch:

  return Either<texture_fullid,texture_ttfid>(ret);
}

void renderer::display()                { DFM_STUB(renderer::display); }
void renderer::cleanup_arrays()         { DFM_STUB(renderer::cleanup_arrays); }
void renderer::gps_allocate(int, int)   { DFM_STUB(renderer::gps_allocate); }
void renderer::swap_arrays()            { DFM_STUB(renderer::swap_arrays); }
void renderer::set_fullscreen()         { DFM_STUB(renderer::set_fullscreen); }
void renderer::zoom(zoom_commands)      { DFM_STUB(renderer::zoom); }
renderer::renderer()                    { DFM_STUB(renderer::renderer);}
renderer::~renderer()                   { DFM_STUB(renderer::~renderer);}
