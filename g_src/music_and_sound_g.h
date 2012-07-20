#ifndef MUSIC_AND_SOUND_G
#define MUSIC_AND_SOUND_G

#define SOUND_CHANNELNUM 16

// If the bool is false, a sound; otherwise a song
typedef std::pair<bool,int> slot;

class musicsoundst {
 public:
  bool initsound(); // Returns false if it failed
  void update();
  void set_master_volume(long newvol);

  void set_song(std::string &filename, slot slot);
  void set_song(std::string &filename, int slot);
  void playsound(slot slot);
  void playsound(int slot); // Assumes sound

  void startbackgroundmusic(slot slot);
  void startbackgroundmusic(int slot); // Assumes song
  void stopbackgroundmusic();
  void stop_sound();
  void stop_sound(slot slot);
  void playsound(int s,int channel);
  void set_sound(std::string &filename,int slot,int pan=-1,int priority=0);
  void deinitsound();
                 
  // Deprecated:
  void forcebackgroundmusic(int slot, unsigned long time);
  void playsound(int s,int min_channel,int max_channel,int force_channel);
  void set_sound_params(int slot,int p1,int vol,int pan,int priority);

  musicsoundst();

  ~musicsoundst();
};

extern musicsoundst musicsound;

#endif
