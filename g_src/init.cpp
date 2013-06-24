#include "init.h"
#include "iplatform.h"

extern enablerst enabler;
extern graphicst gps;

init_displayst::init_displayst()
{
	flag.set_size_on_flag_num(INIT_DISPLAY_FLAGNUM);
	windowed=INIT_DISPLAY_WINDOW_PROMPT;

	partial_print_count=0;
}

/* Skip the lines that have no tokens. Comments, that is. */
static bool is_comment(const std::string& str) {
    int ip = 0;
    while ((ip < str.size()) && isspace(str[ip]))
        ip++;

    if (str[ip] != '[') {
        getplatform()->getlogr("df.settings")->trace("skipping '%s'", str.c_str());
        return true;
    }
    return false;
}

void initst::begin()
{
  static bool called = false;
  if (called) return;
  called = true;
  
	string small_font="data/art/curses_640x300.png";
	string large_font="data/art/curses_640x300.png";
	std::ifstream fseed("data/init/init.txt");
	if(fseed.is_open())
		{
		string str;

		while(std::getline(fseed,str))
			{
			if ((str.length()>1) && !is_comment(str))
				{
				string token;
				string token2;
				grab_token_string_pos(token,str,1);
				if(str.length()>=token.length()+2)
					{
					grab_token_string_pos(token2,str,token.length()+2);
					}
                                getplatform()->set_setting("init", token.c_str(), token2.c_str());

                                if(!token.compare("TRUETYPE")) {
                                  const char *str = token2.c_str();
                                  char *endptr;
                                  int limit = strtol(str, &endptr, 10);
                                  font.ttf_limit = 0;
                                  if (endptr != str) {
                                    font.use_ttf = ttf_auto;
                                    font.ttf_limit = limit;
                                  } else if (token2 == "YES") {
                                    font.use_ttf = ttf_on;
                                  } else {
                                    font.use_ttf = ttf_off;
                                  }
                                }
                                
				if(!token.compare("FONT"))
					{
					small_font="data/art/";
					small_font+=token2;
					}
				if(!token.compare("FULLFONT"))
					{
					large_font="data/art/";
					large_font+=token2;
					}
				if(!token.compare("WINDOWEDX"))
					{
					display.desired_windowed_width=convert_string_to_long(token2);
					}
				if(!token.compare("WINDOWEDY"))
					{
					display.desired_windowed_height=convert_string_to_long(token2);
					}
                                if(!token.compare("RESIZABLE")) {
                                  if (token2=="NO")
                                    display.flag.add_flag(INIT_DISPLAY_FLAG_NOT_RESIZABLE);
                                }
				if(!token.compare("FULLSCREENX"))
					{
					display.desired_fullscreen_width=convert_string_to_long(token2);
					}
				if(!token.compare("FULLSCREENY"))
					{
					display.desired_fullscreen_height=convert_string_to_long(token2);
					}

				if(token=="PRINT_MODE")
					{
					if(token2=="PARTIAL")
						{
						display.flag.add_flag(INIT_DISPLAY_FLAG_PARTIAL_PRINT);

						string token3;
						if(str.length()>=token.length()+token2.length()+3)
							{
							grab_token_string_pos(token3,str,token.length()+token2.length()+3);
							}
						long l=convert_string_to_long(token3);
						if(l<0)l=0;
						if(l>100)l=100;
						display.partial_print_count=(char)l;
						}
                                        if(token2=="PROMPT")
                                                {
                                                  int answer = MessageBox(NULL, "Using only 2D (Click no) is more reliable, but means you lose features and, often, speed. Edit data/init/init.txt PRINT_MODE to avoid this dialog box.", "Use OpenGL?", MB_YESNO);
                                                  if (answer == IDYES)
                                                    token2 = "STANDARD";
                                                  else
                                                    token2 = "2D";
                                                }
                                        if(token2=="TEXT") {
                                          display.flag.add_flag(INIT_DISPLAY_FLAG_TEXT);
                                          display.flag.add_flag(INIT_DISPLAY_FLAG_PARTIAL_PRINT);
                                          display.partial_print_count=0;
                                        }
					if(token2=="FRAME_BUFFER")
						{
						display.flag.add_flag(INIT_DISPLAY_FLAG_FRAME_BUFFER);
						display.flag.add_flag(INIT_DISPLAY_FLAG_PARTIAL_PRINT);
						display.partial_print_count=0;
						}
					if(token2=="ACCUM_BUFFER")
						{
						display.flag.add_flag(INIT_DISPLAY_FLAG_ACCUM_BUFFER);
						display.flag.add_flag(INIT_DISPLAY_FLAG_PARTIAL_PRINT);
						display.partial_print_count=0;
						}
					if(token2=="VBO")
						{
						display.flag.add_flag(INIT_DISPLAY_FLAG_VBO);
						// display.flag.add_flag(INIT_DISPLAY_FLAG_PARTIAL_PRINT);
						display.partial_print_count=0;
						}
                                        if(token2=="2DSW")
                                                {
                                                display.flag.add_flag(INIT_DISPLAY_FLAG_2D);
						display.flag.add_flag(INIT_DISPLAY_FLAG_PARTIAL_PRINT);
                                                display.partial_print_count=0;
                                                }
                                        if(token2=="2D" || token2=="2DHW")
                                                {
                                                display.flag.add_flag(INIT_DISPLAY_FLAG_2D);
                                                display.flag.add_flag(INIT_DISPLAY_FLAG_2DHW);
						display.flag.add_flag(INIT_DISPLAY_FLAG_PARTIAL_PRINT);
                                                display.partial_print_count=0;
                                                }
                                        if(token2=="2DASYNC")
                                                {
                                                display.flag.add_flag(INIT_DISPLAY_FLAG_2D);
                                                display.flag.add_flag(INIT_DISPLAY_FLAG_2DHW);
                                                display.flag.add_flag(INIT_DISPLAY_FLAG_2DASYNC);
						display.flag.add_flag(INIT_DISPLAY_FLAG_PARTIAL_PRINT);
                                                display.partial_print_count=0;
                                                }
                                        if(token2=="SHADER")
                                          {
                                            display.flag.add_flag(INIT_DISPLAY_FLAG_SHADER);
                                          }
 					}

				if(token=="SINGLE_BUFFER")
					{
					if(token2=="YES")
						{
						display.flag.add_flag(INIT_DISPLAY_FLAG_SINGLE_BUFFER);
						}
					}

				if(display.flag.has_flag(INIT_DISPLAY_FLAG_USE_GRAPHICS))
					{
					if(!token.compare("GRAPHICS_FONT"))
						{
						small_font="data/art/";
						small_font+=token2;
						}
					if(!token.compare("GRAPHICS_FULLFONT"))
						{
						large_font="data/art/";
						large_font+=token2;
						}
					if(!token.compare("GRAPHICS_WINDOWEDX"))
						{
						display.desired_windowed_width=convert_string_to_long(token2);
						}
					if(!token.compare("GRAPHICS_WINDOWEDY"))
						{
						display.desired_windowed_height=convert_string_to_long(token2);
						}
					if(!token.compare("GRAPHICS_FULLSCREENX"))
						{
						display.desired_fullscreen_width=convert_string_to_long(token2);
						}
					if(!token.compare("GRAPHICS_FULLSCREENY"))
						{
						display.desired_fullscreen_height=convert_string_to_long(token2);
						}
					if(!token.compare("GRAPHICS_BLACK_SPACE"))
						{
						if(token2=="YES")
							{
							display.flag.add_flag(INIT_DISPLAY_FLAG_BLACK_SPACE);
							}
						else display.flag.remove_flag(INIT_DISPLAY_FLAG_BLACK_SPACE);
						}
					}

				if(!token.compare("GRAPHICS"))
					{
					if(token2=="YES")
						{
						display.flag.add_flag(INIT_DISPLAY_FLAG_USE_GRAPHICS);
						}
					}

				if(!token.compare("BLACK_SPACE"))
					{
					if(token2=="YES")
						{
						display.flag.add_flag(INIT_DISPLAY_FLAG_BLACK_SPACE);
						}
					}

				if(token=="ZOOM_SPEED")
					{
                                          input.zoom_speed = convert_string_to_long(token2);
					}
				if(token=="MOUSE")
					{
					if(token2=="NO")
						{
						input.flag.add_flag(INIT_INPUT_FLAG_MOUSE_OFF);
						}
					}
				if(token=="VSYNC")
					{
					if(token2=="YES")
						{
						window.flag.add_flag(INIT_WINDOW_FLAG_VSYNC_ON);
						}
					if(token2=="NO")
						{
						window.flag.add_flag(INIT_WINDOW_FLAG_VSYNC_OFF);
						}
					}
                                if(token=="ARB_SYNC") {
                                  if (token2 == "YES")
                                    display.flag.add_flag(INIT_DISPLAY_FLAG_ARB_SYNC);
                                }

#ifdef _WIN32
				if(token=="PRIORITY")
					{
					if(token2=="REALTIME")
						{
						SetPriorityClass(GetCurrentProcess(),REALTIME_PRIORITY_CLASS);
						}
					if(token2=="HIGH")
						{
						SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS);
						}
					if(token2=="ABOVE_NORMAL")
						{
						SetPriorityClass(GetCurrentProcess(),ABOVE_NORMAL_PRIORITY_CLASS);
						}
					if(token2=="NORMAL")
						{
						SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);
						}
					if(token2=="BELOW_NORMAL")
						{
						SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);
						}
					if(token2=="IDLE")
						{
						SetPriorityClass(GetCurrentProcess(),IDLE_PRIORITY_CLASS);
						}
					}
#endif

				if(token=="TEXTURE_PARAM")
					{
					if(token2=="LINEAR")
						{
						window.flag.add_flag(INIT_WINDOW_FLAG_TEXTURE_LINEAR);
						}
					}
				if(token=="TOPMOST")
					{
					if(token2=="YES")
						{
						window.flag.add_flag(INIT_WINDOW_FLAG_TOPMOST);
						}
					}
				if(token=="FPS")
					{
					if(token2=="YES")
						{
						gps.display_frames=1;
						}
					}
				if(token=="MOUSE_PICTURE")
					{
					if(token2=="YES")
						{
						input.flag.add_flag(INIT_INPUT_FLAG_MOUSE_PICTURE);
						}
					}
				if(!token.compare("FPS_CAP"))
					{
                                          enabler.set_fps(convert_string_to_long(token2));
					}
				if(!token.compare("G_FPS_CAP"))
					{
                                          enabler.set_gfps(convert_string_to_long(token2));
					}
				if(token=="WINDOWED")
					{
					if(token2=="YES")
						{
						display.windowed=INIT_DISPLAY_WINDOW_TRUE;
						}
					if(token2=="NO")
						{
						display.windowed=INIT_DISPLAY_WINDOW_FALSE;
						}
					if(token2=="PROMPT")
						{
						display.windowed=INIT_DISPLAY_WINDOW_PROMPT;
						}
					}
				if(!token.compare("SOUND"))
					{
					if(token2!="YES")
						{
						media.flag.add_flag(INIT_MEDIA_FLAG_SOUND_OFF);
						}
					}
				if(!token.compare("INTRO"))
					{
					if(token2!="YES")
						{
						media.flag.add_flag(INIT_MEDIA_FLAG_INTRO_OFF);
						}
					}
				if(!token.compare("VOLUME"))
					{
					media.volume=convert_string_to_long(token2);
					}
				if(!token.compare("KEY_HOLD_MS"))
					{
					input.hold_time=convert_string_to_long(token2);

					if(input.hold_time<100)input.hold_time=100;
					}
				if(token=="KEY_REPEAT_MS")
					{
					input.repeat_time=convert_string_to_long(token2);

					if(input.repeat_time<100)input.repeat_time=100;
					}
                                if(token=="KEY_REPEAT_ACCEL_LIMIT") {
                                  input.repeat_accel_limit = convert_string_to_long(token2);
                                  if (input.repeat_accel_limit < 1) input.repeat_accel_limit = 1;
                                }
                                if(token=="KEY_REPEAT_ACCEL_START") {
                                  input.repeat_accel_start = convert_string_to_long(token2);
                                }
				if(token=="MACRO_MS")
					{
					input.macro_time=convert_string_to_long(token2);

					if(input.macro_time<1)input.macro_time=1;
					}
				if(token=="RECENTER_INTERFACE_SHUTDOWN_MS")
					{
					input.pause_zoom_no_interface_ms=convert_string_to_long(token2);

					if(input.pause_zoom_no_interface_ms<0)input.pause_zoom_no_interface_ms=0;
					}
				if(token=="COMPRESSED_SAVES")
					{
					if(token2=="YES")
						{
						media.flag.add_flag(INIT_MEDIA_FLAG_COMPRESS_SAVES);
						}
					}
				}
			}
		}
	fseed.close();

	std::ifstream fseed2("data/init/colors.txt");
	if(fseed2.is_open())
		{
		string str;

		while(std::getline(fseed2,str))
			{
			if(str.length()>1)
				{
                                if (is_comment(str))
                                    continue;
				string token;
				string token2;
				grab_token_string_pos(token,str,1);
				if(str.length()>=token.length()+2)
					{
					grab_token_string_pos(token2,str,token.length()+2);
					}

                                getplatform()->set_setting("colors", token.c_str(), token2.c_str());


				if(!token.compare("BLACK_R"))
					{
					enabler.ccolor[0][0]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("BLACK_G"))
					{
					enabler.ccolor[0][1]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("BLACK_B"))
					{
					enabler.ccolor[0][2]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("BLUE_R"))
					{
					enabler.ccolor[1][0]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("BLUE_G"))
					{
					enabler.ccolor[1][1]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("BLUE_B"))
					{
					enabler.ccolor[1][2]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("GREEN_R"))
					{
					enabler.ccolor[2][0]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("GREEN_G"))
					{
					enabler.ccolor[2][1]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("GREEN_B"))
					{
					enabler.ccolor[2][2]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("CYAN_R"))
					{
					enabler.ccolor[3][0]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("CYAN_G"))
					{
					enabler.ccolor[3][1]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("CYAN_B"))
					{
					enabler.ccolor[3][2]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("RED_R"))
					{
					enabler.ccolor[4][0]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("RED_G"))
					{
					enabler.ccolor[4][1]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("RED_B"))
					{
					enabler.ccolor[4][2]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("MAGENTA_R"))
					{
					enabler.ccolor[5][0]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("MAGENTA_G"))
					{
					enabler.ccolor[5][1]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("MAGENTA_B"))
					{
					enabler.ccolor[5][2]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("BROWN_R"))
					{
					enabler.ccolor[6][0]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("BROWN_G"))
					{
					enabler.ccolor[6][1]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("BROWN_B"))
					{
					enabler.ccolor[6][2]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("LGRAY_R"))
					{
					enabler.ccolor[7][0]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("LGRAY_G"))
					{
					enabler.ccolor[7][1]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("LGRAY_B"))
					{
					enabler.ccolor[7][2]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("DGRAY_R"))
					{
					enabler.ccolor[8][0]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("DGRAY_G"))
					{
					enabler.ccolor[8][1]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("DGRAY_B"))
					{
					enabler.ccolor[8][2]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("LBLUE_R"))
					{
					enabler.ccolor[9][0]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("LBLUE_G"))
					{
					enabler.ccolor[9][1]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("LBLUE_B"))
					{
					enabler.ccolor[9][2]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("LGREEN_R"))
					{
					enabler.ccolor[10][0]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("LGREEN_G"))
					{
					enabler.ccolor[10][1]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("LGREEN_B"))
					{
					enabler.ccolor[10][2]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("LCYAN_R"))
					{
					enabler.ccolor[11][0]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("LCYAN_G"))
					{
					enabler.ccolor[11][1]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("LCYAN_B"))
					{
					enabler.ccolor[11][2]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("LRED_R"))
					{
					enabler.ccolor[12][0]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("LRED_G"))
					{
					enabler.ccolor[12][1]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("LRED_B"))
					{
					enabler.ccolor[12][2]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("LMAGENTA_R"))
					{
					enabler.ccolor[13][0]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("LMAGENTA_G"))
					{
					enabler.ccolor[13][1]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("LMAGENTA_B"))
					{
					enabler.ccolor[13][2]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("YELLOW_R"))
					{
					enabler.ccolor[14][0]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("YELLOW_G"))
					{
					enabler.ccolor[14][1]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("YELLOW_B"))
					{
					enabler.ccolor[14][2]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("WHITE_R"))
					{
					enabler.ccolor[15][0]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("WHITE_G"))
					{
					enabler.ccolor[15][1]=(float)convert_string_to_long(token2)/255.0f;
					}
				if(!token.compare("WHITE_B"))
					{
					enabler.ccolor[15][2]=(float)convert_string_to_long(token2)/255.0f;
					}
				}
			}
		}
	fseed2.close();
}
