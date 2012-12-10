#if !defined(RENDERER_H)
#define RENDERER_H

typedef int texture_ttfid; // Just the texpos
struct texture_fullid {
  int texpos;
  float r, g, b;
  float br, bg, bb;

  bool operator< (const struct texture_fullid &other) const {
    if (texpos != other.texpos) return texpos < other.texpos;
    if (r != other.r) return r < other.r;
    if (g != other.g) return g < other.g;
    if (b != other.b) return b < other.b;
    if (br != other.br) return br < other.br;
    if (bg != other.bg) return bg < other.bg;
    return bb < other.bb;
  }
};

template<typename L, typename R>
struct Either {
  bool isL;
  union {
    L left;
    R right;
  };
  Either(const L &l) {
    isL = true;
    left = l;
  }
  Either(const R &r) {
    isL = false;
    right = r;
  }
};

class renderer {
  void cleanup_arrays();
 protected:
  unsigned char *screen;
  long *screentexpos;
  char *screentexpos_addcolor;
  unsigned char *screentexpos_grayscale;
  unsigned char *screentexpos_cf;
  unsigned char *screentexpos_cbr;
  // For partial printing:
  unsigned char *screen_old;
  long *screentexpos_old;
  char *screentexpos_addcolor_old;
  unsigned char *screentexpos_grayscale_old;
  unsigned char *screentexpos_cf_old;
  unsigned char *screentexpos_cbr_old;

  void gps_allocate(int x, int y);
  Either<texture_fullid,texture_ttfid> screen_to_texid(int x, int y);
 public:
  void display();
  virtual void update_tile(int x, int y) = 0;
  virtual void update_all() = 0;
  virtual void render() = 0;
  virtual void set_fullscreen(); // Should read from enabler.is_fullscreen()
  virtual void zoom(zoom_commands);
  virtual void resize(int w, int h) = 0;
  virtual void grid_resize(int w, int h) = 0;
  void swap_arrays();
  renderer();
  virtual ~renderer();
  virtual bool get_mouse_coords(int &x, int &y) = 0;
  virtual bool uses_opengl();
};

#endif
