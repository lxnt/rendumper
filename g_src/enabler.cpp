#include <cassert>
#include "glue.h"
#include "iplatform.h"
#include "irenderer.h"
#include "isimuloop.h"

#include "platform.h"
#include "enabler.h"
#include "random.h"
#include "init.h"
#include "music_and_sound_g.h"

#ifdef unix
# include <locale.h>
#endif

#define DFM_STUB(foo) getplatform()->log_error("Stub '%s' called.\n", #foo)

using namespace std;

static iplatform *platform = NULL;

enablerst enabler;

// For the printGLError macro
int glerrorcount = 0;

// Reports an error to the user, using a MessageBox and stderr.
void report_error(const char *error_preface, const char *error_message)
{
  char *buf = NULL;
  // +4 = +colon +space +newline +nul
  buf = new char[strlen(error_preface) + strlen(error_message) + 4];
  sprintf(buf, "%s: %s\n", error_preface, error_message);
  MessageBox(NULL, buf, "Error", MB_OK);
  fprintf(stderr, "%s", buf);
  delete [] buf;
}

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

enablerst::enablerst() {
  fullscreen = false;
  sync = NULL;
  renderer = NULL;
  calculated_fps = calculated_gfps = frame_sum = gframe_sum = frame_last = gframe_last = 0;
  fps = 100; gfps = 20;
  fps_per_gfps = fps / gfps;
  last_tick = 0;
}

void renderer::display() { DFM_STUB(renderer::display); }
void renderer::cleanup_arrays() { DFM_STUB(renderer::cleanup_arrays); }
void renderer::gps_allocate(int, int) { DFM_STUB(renderer::gps_allocate); }
void renderer::swap_arrays() { DFM_STUB(renderer::swap_arrays); }
void enablerst::pause_async_loop()  { DFM_STUB(enablerst::pause_async_loop); }
void enablerst::async_wait() { DFM_STUB(enablerst::async_wait); }
void enablerst::async_loop() { DFM_STUB(enablerst::async_loop); }
void enablerst::do_frame() { DFM_STUB(enablerst::do_frame); }
void enablerst::eventLoop_SDL() { DFM_STUB(enablerst::eventLoop_SDL); }
int enablerst::calculate_fps() { DFM_STUB(enablerst::calculate_fps); return 0; }
int enablerst::calculate_gfps() { DFM_STUB(enablerst::calculate_gfps);  return 0; }
void enablerst::do_update_fps(queue<int> &, int &, int &, int &) { DFM_STUB(enablerst::do_update_fps); }
void enablerst::clear_fps() { DFM_STUB(enablerst::clear_fps); }
void enablerst::update_fps() { DFM_STUB(enablerst::update_fps); }
void enablerst::update_gfps() { DFM_STUB(enablerst::update_gfps); }
int call_loop(void *) {  DFM_STUB(call_loop); return 0; }
int enablerst::loop(string) { DFM_STUB(enablerst::loop); return 0; }

void enablerst::override_grid_size(int x, int y) { getrenderer()->override_grid_size(x, y); }
void enablerst::release_grid_size() { getrenderer()->release_grid_size(); }
void enablerst::zoom_display(zoom_commands command) {
    irenderer *r = getrenderer();
    switch (command) {
        case zoom_in:
            r->zoom_in();
            break;
        case zoom_out:
            r->zoom_out();
            break;
        case zoom_reset:
            r->zoom_reset();
            break;
        default:
            getplatform()->log_error("enablerst::zoom_display(): unknown cmd %d\n", command);
            break;
    }
}
void enablerst::set_fps(int fps) { getsimuloop()->set_target_sfps(fps); }
void enablerst::set_gfps(int gfps) { getsimuloop()->set_target_rfps(gfps); }

/* was: renderer::swap_arrays()/renderer::gps_allocate()/renderer::resize().
    must be in libgraphics.so since it depends on the gps object */
static void assimilate_buffer(df_buffer_t *buf) {
    gps.screen = buf->screen;
    gps.screentexpos = buf->texpos;
    gps.screentexpos_addcolor = buf->addcolor;
    gps.screentexpos_grayscale = buf->grayscale;
    gps.screentexpos_cf = buf->cf;
    gps.screentexpos_cbr = buf->cbr;

    /* maybe force_full_display_count will overflow. ;) */
    gps.resize(buf->dimx, buf->dimy);
}

static void add_input_ncurses(int key, uint32_t now) {
    enabler.add_input_ncurses(key, now, false);
}

int main (int argc, char* argv[]) {
    set_modpath("libs/");
    /* here decide what platform to load, without init.txt. somehow. */
    if (!load_module("platform_ncurses"))
        return 1;
    load_module("sound_stub");

    platform = getplatform();

  // Initialise minimal SDL subsystems.
  int retval = SDL_Init(SDL_INIT_TIMER);
  // Report failure?
  if (retval != 0) {
    report_error("SDL initialization failure", SDL_GetError());
    return false;
  }

    if (!load_module("renderer_ncurses")) {
        report_error("renderer load failed.", "");
        return 1;
    }

    if (!load_module("simuloop")) {
        report_error("simuloop load failed.", "");
        return 1;
    }

    irenderer *renderer = getrenderer();
    isimuloop *simuloop = getsimuloop();

    simuloop->set_callbacks(mainloop,
                            render_things,
                            assimilate_buffer,
                            add_input_ncurses);


    init.begin(); // Load init.txt settings

    if (!init.media.flag.has_flag(INIT_MEDIA_FLAG_SOUND_OFF))
        if (!musicsound.initsound()) {
            report_error("Initializing sound failed, no sound will be played", "");
            init.media.flag.add_flag(INIT_MEDIA_FLAG_SOUND_OFF);
        }

    // Load keyboard map
    keybinding_init();
    enabler.load_keybindings("data/init/interface.txt");

    for (int i = 1; i < argc; ++i) {
        char *option = argv[i];
        enabler.command_line += option;
        enabler.command_line += " ";
    }

    if (beginroutine()) { // TODO: think of moving to simuloop.
        renderer->start();
        simuloop->start();

        renderer->join();

        endroutine();
    } else {
        report_error("beginroutine()", "failed");
    }

    SDL_Quit();
    return 0;
}

char get_slot_and_addbit_uchar(unsigned char &addbit,long &slot,long checkflag,long slotnum)
{
  if(checkflag<0)return 0;

  //FIND PROPER SLOT
  slot=checkflag>>3;
  if(slot>=slotnum)return 0;

  //FIND PROPER BIT IN THAT SLOT
  addbit=1<<(checkflag%8);

  return 1;
}

void text_system_file_infost::initialize_info()
{
  std::ifstream fseed(filename.c_str());
  if(fseed.is_open())
    {
      string str;

      while(std::getline(fseed,str))
	{
	  if(str.length()>0)number++;
	}
    }
  else
    {
      string str;
      str="Error Initializing Text: ";
      str+=filename;
      errorlog_string(str);
    }
  fseed.close();
}

void text_system_file_infost::get_text(text_infost &text)
{
  text.clean();

  if(number==0)return;

  std::ifstream fseed(filename.c_str());
  if(fseed.is_open())
    {
      string str;

      int num=trandom(number);

      //SKIP AHEAD TO THE RIGHT SPOT
      while(num>0)
	{
	  std::getline(fseed,str);
	  num--;
	}

      //PROCESS THE STRING INTO TEXT ELEMENTS
      if(std::getline(fseed,str))
	{
	  int curpos;
	  string nextstr;
	  char doing_long=0;

	  text_info_elementst *newel;
	  long end=str.length();
			
	  while(end>0)
	    {
	      if(isspace(str[end-1]))end--;
	      else break;
	    }
			
	  str.resize(end);

	  for(curpos=0;curpos<end;curpos++)
	    {
	      //HANDLE TOKEN OR ENDING
	      //TWO FILE TOKENS IN A ROW MEANS LONG
	      //ONE MEANS STRING
	      if(str[curpos]==file_token || curpos==end-1)
		{
		  if(str[curpos]!=file_token)nextstr+=str[curpos];

		  //HAVE SOMETHING == SAVE IT
		  if(!nextstr.empty())
		    {
		      if(doing_long)
			{
			  newel=new text_info_element_longst(atoi(nextstr.c_str()));
			  text.element.push_back(newel);
			  doing_long=0;
			}
		      else
			{
			  newel=new text_info_element_stringst(nextstr);
			  text.element.push_back(newel);
			}

		      nextstr.erase();
		    }
		  //STARTING A LONG
		  else
		    {
		      doing_long=1;
		    }
		}
	      //JUST ADD IN ANYTHING ELSE
	      else
		{
		  nextstr+=str[curpos];
		}
	    }
	}
    }
  fseed.close();
}

void curses_text_boxst::add_paragraph(const string &src,int32_t para_width)
{
	stringvectst sp;
	sp.add_string(src);
	add_paragraph(sp,para_width);
}

void curses_text_boxst::add_paragraph(stringvectst &src,int32_t para_width)
{
	bool skip_leading_spaces=false;

	//ADD EACH OF THE STRINGS ON IN TURN
	string curstr;
	size_t s,pos;
	for(s=0;s<src.str.size();s++)
		{
		//GRAB EACH WORD, AND SEE IF IT FITS, IF NOT START A NEW LINE
		for(pos=0;pos<src.str[s]->dat.size();pos++)
			{
			if(skip_leading_spaces)
				{
				if(src.str[s]->dat[pos]==' ')continue;
				else skip_leading_spaces=false;
				}

			//ADD TO WORD
			curstr+=src.str[s]->dat[pos];

			//IF TOO LONG, CUT BACK TO FIRST SPACE
			if(curstr.length()>para_width)
				{
				long opos=pos;

				size_t minus=0;
				do
					{
					pos--;
					minus++;
					}while(src.str[s]->dat[pos]!=' '&&pos>0);

				//IF WENT ALL THE WAY BACK, INTRODUCE A SPACE
				if(minus==curstr.size())
					{
					src.str[s]->dat.insert(opos-1," ");
					}
				else
					{
					curstr.resize(curstr.size()-minus);
					text.add_string(curstr);
					skip_leading_spaces=true;
					}
				curstr.erase();
				}
			}
		}

	//FLUSH FINAL BIT
	if(!curstr.empty())text.add_string(curstr);
}
