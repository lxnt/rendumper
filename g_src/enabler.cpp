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

using namespace std;

iplatform *platform = NULL;
isimuloop *simuloop = NULL;
ilogger *stubs_logr = NULL;
ilogger *mainlogr = NULL;
ilogger *nputlogr = NULL;
ilogger *addst_logr = NULL;

enablerst enabler;

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

enablerst::enablerst() {
  fullscreen = false;
  sync = NULL;
  renderer = NULL;
  calculated_fps = calculated_gfps = frame_sum = gframe_sum = frame_last = gframe_last = 0;
  fps = 100; gfps = 20;
  fps_per_gfps = fps / gfps;
  last_tick = 0;
  tracking_on = 1;
}

bool enablerst::is_fullscreen()         { DFM_STUB(enablerst::is_fullscreen); return false; }
void enablerst::toggle_fullscreen()     { DFM_STUB(enablerst::toggle_fullscreen); }
int enablerst::get_fps()                { DFM_STUB(enablerst::get_fps); return 1; }
int enablerst::get_gfps()               { DFM_STUB(enablerst::get_gfps); return 1;}
void enablerst::unpause_async_loop()    { DFM_STUB(enablerst::unpause_async_loop); }
void enablerst::pause_async_loop()      { DFM_STUB(enablerst::pause_async_loop); }
void enablerst::async_wait()            { DFM_STUB(enablerst::async_wait); }
void enablerst::async_loop()            { DFM_STUB(enablerst::async_loop); }
void enablerst::do_frame()              { DFM_STUB(enablerst::do_frame); }
void enablerst::eventLoop_SDL()         { DFM_STUB(enablerst::eventLoop_SDL); }
void enablerst::do_update_fps(queue<int> &, int &, int &, int &) { DFM_STUB(enablerst::do_update_fps); }
void enablerst::clear_fps()             { DFM_STUB(enablerst::clear_fps); }
void enablerst::update_fps()            { DFM_STUB(enablerst::update_fps); }
void enablerst::update_gfps()           { DFM_STUB(enablerst::update_gfps); }
int call_loop(void *)                   { DFM_STUB(call_loop); return 0; }
int enablerst::loop(string)             { DFM_STUB(enablerst::loop); return 0; }

int enablerst::calculate_fps()          { return getsimuloop()->get_actual_sfps(); }
int enablerst::calculate_gfps()         { return getsimuloop()->get_actual_rfps(); }
void enablerst::override_grid_size(int x, int y) { getrenderer()->override_grid_size(x, y); }
void enablerst::release_grid_size()     { getrenderer()->release_grid_size(); }
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
            mainlogr->error("enablerst::zoom_display(): unknown cmd %d\n", command);
            break;
    }
}
void enablerst::set_fps(int fps) { getsimuloop()->set_target_sfps(fps); }
void enablerst::set_gfps(int gfps) { getsimuloop()->set_target_rfps(gfps); }
void enablerst::reset_textures(void) { getrenderer()->reset_textures(); }
bool enablerst::uses_opengl(void) { DFM_STUB(enablerst::uses_opengl); return false; }
/* was: renderer::swap_arrays()/renderer::gps_allocate()/renderer::resize().
    must be in libgraphics.so since it depends on the gps object */
static bool export_effects = false;
unsigned char *gps_screenfxpos = NULL;
static unsigned long tflag;

void assimilate_buffer(df_buffer_t *buf) {
    gps.screen = buf->screen;
    gps.screentexpos = buf->texpos;
    gps.screentexpos_addcolor = buf->addcolor;
    gps.screentexpos_grayscale = buf->grayscale;
    gps.screentexpos_cf = buf->cf;
    gps.screentexpos_cbr = buf->cbr;
    gps_screenfxpos = export_effects ? buf->fx : NULL;

    /* maybe force_full_display_count will overflow. ;) */
    gps.resize(buf->w, buf->h);

    // flag &= ~ENABLERFLAG_RENDER; // triggers rendering: not needed.
    /* convert level to edge. */
    if (tflag != enabler.flag) {
        if ((enabler.flag & ENABLERFLAG_MAXFPS)
        && !(tflag & ENABLERFLAG_MAXFPS)) {
            mainlogr->trace("ENABLERFLAG_MAXFPS set");
            simuloop->set_max_sfps();
        } else if (!(enabler.flag & ENABLERFLAG_MAXFPS)
                 && (tflag & ENABLERFLAG_MAXFPS)) {
            mainlogr->trace("ENABLERFLAG_MAXFPS cleared");
            simuloop->set_nominal_sfps();
        }
        tflag = enabler.flag;
    }
    mainlogr->trace("buffer %p/%p assimilated", buf, buf->screen);
}

/* Drop all pointers to the (possibly GL-mapped) buffer since it is done with. */
void eject_buffer(df_buffer_t *buf) {
    if (!buf || buf->screen != gps.screen)
        mainlogr->fatal("eject_buffer(%p): wrong buffer.", buf);

    gps.screen = NULL;
    gps.screentexpos = NULL;
    gps.screentexpos_addcolor = NULL;
    gps.screentexpos_grayscale = NULL;
    gps.screentexpos_cf = NULL;
    gps.screentexpos_cbr = NULL;
    gps_screenfxpos = NULL;
    gps.screen_limit = NULL;
    mainlogr->trace("buffer %p/%p ejected", buf, buf->screen);
}

void fg_dump(const char *filename);
#if defined(KILL_SWITCH)
static bool seen_quit = false;
#endif
static void add_input_event(df_input_event_t *event) {
    /* TODO: move the mouse tracking into enabler_input
       ALSO: fix this sad excuse of an 'interface'
       NOTE: this differs from the original:
            does not honor the INIT_INPUT_FLAG_MOUSE_OFF
            does not do mouse hiding.
    */
    switch (event->type) {
    case df_input_event_t::DF_BUTTON_UP:
    case df_input_event_t::DF_BUTTON_DOWN:
        gps.mouse_x = event->grid_x;
        gps.mouse_y = event->grid_y;
        switch (event->button) {
        case df_input_event_t::DF_BUTTON_LEFT:
            enabler.mouse_lbut = event->type == df_input_event_t::DF_BUTTON_DOWN;
            enabler.mouse_lbut_down = enabler.mouse_lbut;
            if (!enabler.mouse_lbut)
                enabler.mouse_lbut_lift = 0;
            return;
            break;
        case df_input_event_t::DF_BUTTON_RIGHT:
            enabler.mouse_rbut = event->type == df_input_event_t::DF_BUTTON_DOWN;
            enabler.mouse_rbut_down = enabler.mouse_rbut;
            if (!enabler.mouse_rbut)
                enabler.mouse_rbut_lift = 0;
            return;
            break;
        default:
            break;
        }
        break;
    case df_input_event_t::DF_MOUSE_MOVE:
        gps.mouse_x = event->grid_x;
        gps.mouse_y = event->grid_y;
        nputlogr->trace("DF_MOUSE_MOVE %d,%d", gps.mouse_x, gps.mouse_y);
        return;
#if defined(FG_DUMP)
    case df_input_event_t::DF_KEY_DOWN:
        if (event->sym == DFKS_PRINTSCREEN) {
            fg_dump("fugr.dump");
            return;
        }
        break;
#endif
#if defined(KILL_SWITCH)
    case df_input_event_t::DF_QUIT:
        if (not seen_quit)
            seen_quit = true;
        else
            nputlogr->fatal("aborted by user");
#endif
    default:
        break;
    }
    enabler.add_input(*event);
}

int main (int argc, char* argv[]) {
    /* here decide what platform to load, without init.txt. somehow. */
    if (!lock_and_load(argc > 1 ? argv[1] : "sdl2gl3", NULL))
        return 1;

    platform = getplatform();
    mainlogr = platform->getlogr("df");
    stubs_logr = platform->getlogr("df.stubs");
    nputlogr = platform->getlogr("df.input");
    addst_logr = platform->getlogr("df.addst");
    irenderer *renderer = getrenderer();
    simuloop = getsimuloop();

    simuloop->set_callbacks(mainloop,
                            render_things,
                            assimilate_buffer,
                            eject_buffer,
                            add_input_event);

    init.begin(); // Load init.txt settings

    if (!init.media.flag.has_flag(INIT_MEDIA_FLAG_SOUND_OFF)) {
        if (!load_module("sound_sdl2mixer") || !musicsound.initsound()) {
            report_error("Initializing sound failed, no sound will be played", "");
            mainlogr->warn("Initializing sound failed, no sound will be played");
            init.media.flag.add_flag(INIT_MEDIA_FLAG_SOUND_OFF);
        }
    }
    mainlogr->info("export_effects: %s", export_effects ? "Y": "N");
    // Load keyboard map
    keybinding_init();
    enabler.load_keybindings("data/init/interface.txt");

    for (int i = 1; i < argc; ++i) {
        char *option = argv[i];
        enabler.command_line += option;
        enabler.command_line += " ";
    }
    if (beginroutine()) { // TODO: think of moving to simuloop.
        simuloop->start();

        renderer->run_here();
        /* renderer has quit. what to do if simuloop didn't? */
        simuloop->join();

        endroutine();
    } else {
        report_error("beginroutine()", "failed");
    }

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
