#include <iomanip>  // setw
#include <utility>
#include <list>

#include <stdint.h>

#include "enabler.h"
#include "find_files.h"
#include "texture_handler.h"
#include "graphics.h"
#include "init.h"
#include "interface.h"

#include "isimuloop.h"
extern isimuloop *simuloop;
extern ilogger *addst_logr;

using namespace std;

extern enablerst enabler;
extern texture_handlerst texture;
graphicst gps;
extern interfacest gview;

extern string errorlog_prefix;

void process_object_lines(textlinesst &lines,string &chktype,string &graphics_dir);

void graphicst::resize(int x, int y)  {
  dimx = x; dimy = y;
  init.display.grid_x = x;
  init.display.grid_y = y;
  setclipping(0, x-1, 0, y-1);
  force_full_display_count++;
  screen_limit = screen + dimx * dimy * 4;
}

void graphicst::addcoloredst(const char *str, const char *colorstr)
{
    if ((screeny < clipy[0]) || (screeny > clipy[1]))
        return;

    std::string copy(str);
    std::string colors(colorstr, copy.size());

    int head_cut = clipx[0] - screenx;

    if (head_cut > 0) {
        copy.erase(0, head_cut);
        colors.erase(0, head_cut);
    }
    int tail_size = screenx + copy.size() - clipx[1];

    if (tail_size > 0) {
        copy.resize(copy.size() - tail_size);
        colors.resize(copy.size() - tail_size);
    }
    screenx += simuloop->add_string(copy.c_str(), colors.data(), screenx, screeny, DF_MONOSPACE_LEFT, 0);
}

typedef std::pair<std::string, unsigned> to_be_continued_t;
static std::list<to_be_continued_t> previously;

void graphicst::addst(const string &str_orig, justification just, int space)
{
    /* just don't bother if it's y-clipped. */
    if ((screeny < clipy[0]) || (screeny > clipy[1]))
        return;

    /* also don't bother with zero-length strings */
    if (str_orig.size() == 0)
        return;

    unsigned attr = ((screenbright << 6) | ((screenb & 7) << 3) | (screenf & 7)) & 0xFFu;

    /* Poor little msvc 2010 can't emplace stuff. Oh, the sorrow. */
    to_be_continued_t tmp(str_orig, attr);
    previously.push_back(tmp);

    if (just == justify_cont) {
        if (space)
            addst_logr->warn("nonzero space on justify_cont: \"%s\", space=%d", str_orig.c_str(), space);
        return; /* more to follow, and without touching screenx. hopefully. */
    }

    /* TODO: optimize for non-justify_cont case. But profile first. */

    std::string copy;
    std::string colors;

    for (auto i = previously.begin(); i != previously.end(); ++i) {
        copy.append(i->first);
        colors.append(i->first.size(), i->second);
    }
    previously.clear();

    /* this head clipping will have funny effect with ttf on -
       it'd cut less that would have been if amount was calculated ttf-warily
       a rare corner case, hopefully. */
    int head_cut = clipx[0] - screenx;
    if (head_cut > 0) {
        copy.erase(0, head_cut);
        colors.erase(0, head_cut);
    }

    int tail_size = screenx + copy.length() - clipx[1];

    if (tail_size > 0) {
        if ((space == 0) || (space > copy.length() - tail_size))
            space = copy.length() - tail_size;
    }

    uint32_t align;
    switch (just) {
    case justify_left:
        align = DF_TEXTALIGN_LEFT;
        break;
    case justify_center:
        align = DF_TEXTALIGN_CENTER;
        break;
    case justify_right:
        align = DF_TEXTALIGN_RIGHT;
        break;
    case justify_cont: // can't be here.
    case not_truetype:
    default:
        align = DF_MONOSPACE_LEFT;
        break;
    }
    screenx += simuloop->add_string(copy.c_str(), colors.data(), screenx, screeny, align, space);
}

void graphicst::erasescreen_clip()
{
	changecolor(0,0,0);
	short x2,y2;
	for(x2=clipx[0];x2<=clipx[1];x2++)
		{
		for(y2=clipy[0];y2<=clipy[1];y2++)
			{
			locate(y2,x2);
			addchar(' ');
			}
		}
}

void graphicst::erasescreen_rect(int x1, int x2, int y1, int y2)
{ 
  changecolor(0,0,0);
  for (int x = x1; x <= x2; x++) {
    for (int y = y1; y <= y2; y++) {
      locate(y, x);
      addchar(' ');
    }
  }
}

void graphicst::erasescreen()
{
	memset(screen, 0, dimx*dimy*4);

	memset(screentexpos, 0, dimx*dimy*sizeof(long));
}

void graphicst::setclipping(long x1,long x2,long y1,long y2)
{
	if(x1<0)x1=0;
	if(x2>init.display.grid_x-1)x2=init.display.grid_x-1;
	if(y1<0)y1=0;
	if(y2>init.display.grid_y-1)y2=init.display.grid_y-1;

	clipx[0]=x1;
	clipx[1]=x2;
	clipy[0]=y1;
	clipy[1]=y2;
}


enum df_3bitcolors : unsigned char {
    BLACK = 0,
    BLUE = 1,
    GREEN = 2,
    CYAN = 3,
    RED = 4,
    MAGENTA = 5,
    BROWN = 6,
    LGRAY = 7
};

void graphicst::dim_colors(long x, long y, char dim)
{
    if (   x >= clipx[0]
        && x <= clipx[1]
        && y >= clipy[0]
        && y <= clipy[1]) {

        size_t base = 4*(x*dimy + y);

        unsigned char& fg = screen[base + 1];
        unsigned char& bg = screen[base + 2];
        unsigned char& br = screen[base + 3];

        switch (dim) {
        case 4:
            switch (bg) {
                case RED:
                case MAGENTA:
                case BROWN:
                    bg = BLUE;
                    break;
                case GREEN:
                case LGRAY:
                    bg = CYAN;
                    break;
            }
            switch (fg) {
                case RED:
                case MAGENTA:
                case BROWN:
                    fg = BLUE;
                    break;
                case GREEN:
                case LGRAY:
                    fg = CYAN;
                    break;
            }
            if (fg == bg)
                fg = BLACK;

            br = 0;
            break;
        case 3:
            switch (bg) {
                case RED:
                case MAGENTA:
                    bg = BROWN;
                    break;
                case GREEN:
                case LGRAY:
                    bg = CYAN;
                    break;
            }
            switch (fg) {
                case BLUE:
                    br = 0;
                    break;
                case RED:
                case MAGENTA:
                    fg = BROWN;
                    break;
                case GREEN:
                    fg = CYAN;
                    break;
                case LGRAY:
                    fg = CYAN;
                    break;
            }
            if (fg != LGRAY)
                br = 0;

            if (fg == bg && br == 0)
                fg = BLACK;

            break;
        case 2:
            switch (bg) {
                case RED:
                case MAGENTA:
                    bg = BROWN;
                    break;
            }
            switch (fg) {
                case RED:
                case MAGENTA:
                    fg = BROWN;
                    break;
            }
            if (fg != LGRAY)
                br = 0;

            if (fg == bg && br == 0)
                fg = BLACK;

            break;
        case 1:
            if (fg != LGRAY)
                br = 0;

            if (fg == bg && br == 0)
                fg = BLACK;

            break;
        }

        if (fg == BLACK && bg == BLACK && br == 0)
            br = 1;
    }
}

void graphicst::rain_color_square(long x, long y)
{
    if (   x >= clipx[0]
        && x <= clipx[1]
        && y >= clipy[0]
        && y <= clipy[1] ) {
        size_t base = 4*(x*dimy + y);
        unsigned char& fg = screen[base + 1];
        unsigned char& bg = screen[base + 2];
        unsigned char& br = screen[base + 3];

        fg = BLUE;
        bg = BLACK;
        br = 1;
    }
}

void graphicst::snow_color_square(long x, long y)
{
    if (   x >= clipx[0]
        && x <= clipx[1]
        && y >= clipy[0]
        && y <= clipy[1] ) {
        size_t base = 4*(x*dimy + y);
        unsigned char& fg = screen[base + 1];
        unsigned char& bg = screen[base + 2];
        unsigned char& br = screen[base + 3];

        fg = LGRAY;
        bg = BLACK;
        br = 1;
    }
}

void graphicst::color_square(long x, long y, unsigned char f, unsigned char b, unsigned char br)
{
    if (   x >= clipx[0]
        && x <= clipx[1]
        && y >= clipy[0]
        && y <= clipy[1] ) {
        size_t base = 4*(x*dimy + y);
        screen[base + 1] = f;
        screen[base + 2] = b;
        screen[base + 3] = br;
    }
}

void graphicst::prepare_graphics(string &src_dir)
{
	if(!init.display.flag.has_flag(INIT_DISPLAY_FLAG_USE_GRAPHICS))return;

	texture.clean();

	//GET READY TO LOAD
	svector<char *> processfilename;
	long f;
	textlinesst setuplines;
	char str[400];

	//LOAD THE OBJECT FILES UP INTO MEMORY
		//MUST INSURE THAT THEY ARE LOADED IN THE PROPER ORDER, IN CASE THEY REFER TO EACH OTHER
	string chk=src_dir;
	chk+="graphics/graphics_*";
#ifdef _WIN32
	chk+=".*";
#endif
	find_files_by_pattern_with_exception(chk.c_str(),processfilename,"text");

	string chktype="GRAPHICS";
	for(f=0;f<processfilename.size();f++)
		{
		strcpy(str,src_dir.c_str());
		strcat(str,"graphics/");
		strcat(str,processfilename[f]);
		setuplines.load_raw_to_lines(str);

		errorlog_prefix="*** Error(s) found in the file \"";
		errorlog_prefix+=str;
		errorlog_prefix+='\"';
		process_object_lines(setuplines,chktype,src_dir);
		errorlog_prefix.clear();


		delete[] processfilename[f];
		}
	processfilename.clear();

        enabler.reset_textures();
}

void graphicst::add_tile(long texp,char addcolor)
{
	if(screenx>=clipx[0]&&screenx<=clipx[1]&&
		screeny>=clipy[0]&&screeny<=clipy[1])
		{
		screentexpos[screenx*dimy + screeny]=texp;
		screentexpos_addcolor[screenx*dimy + screeny]=addcolor;
		screentexpos_grayscale[screenx*dimy + screeny]=0;
		}
}

void graphicst::add_tile_grayscale(long texp,char cf,char cbr)
{
	if(screenx>=clipx[0]&&screenx<=clipx[1]&&
		screeny>=clipy[0]&&screeny<=clipy[1])
		{
		screentexpos[screenx*dimy + screeny]=texp;
		screentexpos_addcolor[screenx*dimy + screeny]=0;
		screentexpos_grayscale[screenx*dimy + screeny]=1;
		screentexpos_cf[screenx*dimy + screeny]=cf;
		screentexpos_cbr[screenx*dimy + screeny]=cbr;
		}
}

void graphicst::draw_border(int x1, int x2, int y1, int y2) {
  // Upper and lower
  for (int x = x1; x <= x2; x++) {
    locate(y1, x);
    addchar(' ');
    locate(y2, x);
    addchar(' ');
  }
  // Left and right
  for (int y = y1+1; y < y2; y++) {
    locate(y, x1);
    addchar(' ');
    locate(y, x2);
    addchar(' ');
  }
}

void graphicst::get_mouse_text_coords(int32_t &mx, int32_t &my) {
  mx = mouse_x; my = mouse_y;
}

void render_things()
{
  //GRAB CURRENT SCREEN AT THE END OF THE LIST
  viewscreenst *currentscreen=&gview.view;
  while(currentscreen->child!=NULL)currentscreen=currentscreen->child;
  
  //NO INTERFACE LEFT, LEAVE
  if(currentscreen==&gview.view)return;
  
  if(currentscreen->breakdownlevel==INTERFACE_BREAKDOWN_NONE)
	{
	currentscreen->render();
	}
  else gps.erasescreen();

  // Render REC when recording macros. Definitely want this screen-specific. Or do we?
  const Time now = GetTickCount();
  if (enabler.is_recording() && now % 1000 > 500) {
    gps.locate(0, 20);
    gps.changecolor(4,1,1);
    gps.addst("REC");
  }
  // Render PLAY when playing a macro
  if (enabler.is_macro_playing() && now % 1000 <= 500) {
    gps.locate(0,20);
    gps.changecolor(2,1,1);
    gps.addst("PLAY");
  }
  // Render # <i> when building a repetition prefix
  if (enabler.prefix_building()) {
    gps.locate(0,20);
    gps.changecolor(3,1,1);
    gps.addst("#" + enabler.prefix());
  }
  if (gps.display_frames) {
    ostringstream fps_stream;
    if (true)
      {
          char boof[1024];
          sprintf(boof, "fps=%12d gfps=%12d", enabler.calculate_fps(), enabler.calculate_gfps());
          fps_stream << std::string(boof);
      }
    else
        fps_stream << "FPS: " << setw(3) << enabler.calculate_fps() << " (" << enabler.calculate_gfps() << ")";
    string fps = fps_stream.str();
    gps.changecolor(7,3,1);
    static gps_locator fps_locator(0, 25);
    fps_locator(fps.size());
    gps.addst(fps);
  }
}
