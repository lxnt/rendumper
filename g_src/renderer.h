#if !defined(RENDERER_H)
#define RENDERER_H

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
    Either<texture_fullid, int> screen_to_texid(int x, int y);

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

class renderer_2d_base : public renderer {
  protected:
    void *screen;
    std::map<texture_fullid, void *> tile_cache;
    int dispx, dispy, dimx, dimy;
    // We may shrink or enlarge dispx/dispy in response to zoom requests. dispx/y_z are the
    // size we actually display tiles at.
    int dispx_z, dispy_z;
    // Viewport origin
    int origin_x, origin_y;

    void *tile_cache_lookup(texture_fullid &id, bool convert=true);
    virtual bool init_video(int w, int h);

  public:
    std::list<std::pair<void *, uint64_t>> martians_to_render;

    void update_tile(int x, int y);
    void update_all();
    virtual void render();
    virtual ~renderer_2d_base();
    void grid_resize(int w, int h);
    renderer_2d_base();

    int zoom_steps, forced_steps;
    int natural_w, natural_h;

    void compute_forced_zoom();
    std::pair<int, int> compute_zoom(bool clamp = false);
    void resize(int w, int h);
    void reshape(pair<int,int> max_grid);

  private:
    void set_fullscreen();
    bool get_mouse_coords(int &x, int &y);
    void zoom(zoom_commands cmd);  
};

class renderer_offscreen : public renderer_2d_base {
    virtual bool init_video(int, int);
    df_buffer_t *world;

  public:
    virtual ~renderer_offscreen();
    renderer_offscreen(int, int);
    void update_all(int, int);
    void save_to_file(const std::string &file);
};


#endif
