#!/usr/bin/python
# -*- encoding: utf-8 -*-

"""
Converts the bunch of SoundSense .xml files into a binary index
suitable for the sound-sdl2mixer intake.

The binary format is:

Header
Sound
 SoundFile
 ...
Sound
 SoundFile
 SoundFile
 ...
string, string, string, ...

struct Header {                // 16 bytes
    int32_t sound_count;       // count of struct Sound-s
    int32_t soundfile_count;   // count total of struct SoundFile-s
    int32_t stringtab_size;    // string table size
    int32_t channel_count;     // number of logical channels (music, trade, etc) used,
                               // including the 'non-channel' with id=0
}

struct Sound {                  // 80 bytes
    int32_t pattern_offset;     // regexp offset into string table (byte offset from start of table)
    int32_t channel_id;         // channel id, 0 for none.
    int32_t loop;               // loop setting: 0=none, 1=start, 2=stop
    int32_t concurrency;        // The number of concurrent (i.e., simultaneous) sounds allowed
                                // to be played besides this sound. If SoundSense is currently
                                // playing more than this, the sound is ignored. The default is
                                // unlimited. (kinda priority?)
    int32_t delay;              // Adds a delay before the sound is played. This is measured in
                                // milliseconds and the default is 0.
    int32_t timeout;            // This initiates a time out during which this particular sound
                                // is prevented from playing again. This is measured in milliseconds
                                // and the default is 0
    int32_t threshold;          // number, 0-4, defines level of filter which is applied to this sound.
                                // (was: playbackThreshold; now, what's a filter?
    int32_t probability;        // 100*P(playing_if_triggered)
    int32_t haltOnMatch;        // If this is set to True and more than one equivalent logPattern exists,
                                // SoundSense will only process the first logPattern
    int32_t soundfile_first;    // index of first SoundFile for this sound in the soundfile table
    int32_t soundfile_count;    // count of SoundFile structures there
    int32_t weight_sum;         // sum of soundfile weights
    uint8_t padding[32];        // padding to hold extra data when loaded and align to 16 bytes
}

struct SoundFile {              // 32 bytes
    int32_t filename_offset;    // filename offset into string table
    int32_t filename_length;    // to not pound on strlen
    int32_t weight;             // This controls the likelihood of a sound being chosen. Default is 100.
    int32_t randomBalance;      // if set to true will randomply distribute sound betweem stereo channels.
    int32_t balanceAdjustment;  // adjusts stereo channel, can range for -127 (full left) to 127 (full right).
                                // was: float from -1.0 to 1.0
    int32_t volumeAdjustment;   // This can be used to adjust the volume of the sample. The value can
                                // range from -0 to 128 and the default is 64.
                                // (dBs mapped to linear so that max +6 gives 128, and set as Mix_Chunk volume)
                                // was: from -40 to +6 decibels and the default is 0.
    uint8_t padding[8];         // padding to hold extra data when loaded and align to 16 bytes
}

byte order is little-endian.
strings in the stringtab are zero-padded to 4 byte alignment.
"""

import os, os.path, sys, argparse, struct, math
from lxml import etree
from lxml.etree import XPath, XMLSyntaxError, Element, Comment
Wall = False
def pot_align(value, pot):
    return (value + (1<<pot) - 1) & ~((1<<pot) -1)

class StringTab(object):
    def __init__(self, kill_dupes = True):
        self.data = bytearray("\x00"*8) # so that no real offset is zero.
        self.offsets = {}
        self.kill_dupes = kill_dupes
        self.dupes_killed = 0

    def push(self, buf):
        if self.kill_dupes and buf in self.offsets:
            self.dupes_killed += 1
            return self.offsets[buf]
        rv = len(self.data)
        buflen = len(buf) + 1 # plus trailing zeroes
        padlen = pot_align(buflen, 2) - len(buf)
        self.data += buf
        self.data += b'\x00' * padlen
        if pot_align(len(self.data), 2) != len(self.data):
            print("len(buf)={:d} buflen={:d} padlen={:d} len(self.data) = {:d}, pa= {:d}".format(
                len(buf), buflen, padlen, len(self.data), pot_align(len(self.data), 2)))
            raise Exception
        if self.kill_dupes:
            self.offsets[buf] = rv
        return rv

    def dump(self):
        return self.data

class Sound(object):
    def __init__(self, sourceref, el, sf_offs, chandict):
        self.logPattern = el.get("logPattern")
        if self.logPattern is None:
            raise Exception
        self.concurrency = int(el.get("concurrency", "0"))
        self.delay = int(el.get("delay", "0"))
        self.timeout = int(el.get("timeout", "0"))
        self.threshold = int(el.get("playbackThreshold", "0"))
        self.propability = int(el.get("propability", "0"))
        self.haltOnMatch = 0 if el.get("haltOnMatch", "false").lower()  == "false" else 1
        self.channel = el.get("channel")
        if self.channel in chandict:
            self.channel_id = chandict[self.channel]
        else:
            self.channel_id = len(chandict);
            chandict[self.channel] = self.channel_id
        loop = el.get("loop")
        if loop is None:
            self.loop = 0
        elif loop == "start":
            self.loop = 1
        elif loop == "stop":
            self.loop = 2
        else:
            print("{}: warning: bogus loop setting of '{}', ignoring".format(sourceref, loop))
            self.loop = 0
        if self.loop != 0 and self.channel_id == 0:
            print("{}: warning: loop setting of '{}' without a channel, ignoring".format(sourceref, loop))
            self.loop = 0
        self.soundfile_first = sf_offs
        self.weight_sum = 0
        self.sounds = []

    def dump(self, sftab, strtab):
        pattern_offs = strtab.push(self.logPattern.encode('utf-8'))
        if self.channel:
            channel_offs = strtab.push(self.channel.encode('utf-8'))
        else:
            channel_offs = 0;
        rv = bytearray()
        rv += struct.pack("<12i32x", pattern_offs, channel_offs, self.loop,
                    self.concurrency, self.delay, self.timeout, self.threshold,
                    self.propability, self.haltOnMatch, self.soundfile_first,
                                                    len(self.sounds), self.weight_sum);
        for s in self.sounds:
            s.dump(sftab, strtab)
        return rv

    def append(self, sound):
        self.weight_sum += sound.weight
        self.sounds.append(sound)

    def __len__(self):
        return len(self.sounds)

class SoundFile(object):
    def __init__(self, el):
        self.fileName = el.get("fileName")
        if self.fileName is None:
            raise Exception
        if not os.path.exists(self.fileName):
            raise Exception
        self.weight = int(el.get("weight", "100")) # SoundFile.java:10
        vol = 64 * math.pow(10.0, float(el.get("volumeAdjustment", "0"))/20.0)  # java docs for MASTER_GAIN
        self.volumeAdjustment = vol
        self.randomBalance = 1 if el.get("haltOnMatch", "false")  == "true" else 0
        self.balanceAdjustment = int(127 * float(el.get("balanceAdjustment", "0.0")))

    def dump(self, sftab, strtab):
        filename = self.fileName.encode('utf-8')
        filename_offs = strtab.push(filename)
        sftab.push(struct.pack("<6i8x", filename_offs, len(filename), self.weight,
                    self.volumeAdjustment, self.randomBalance, self.balanceAdjustment))


def walk_the_walk(packspath):
    global Wall
    chandict = { None: 0 } # for numbering the channels
    filesizes = { }
    sounds = []
    filerefs = {}
    sfcount = 0
    for walk in os.walk(packspath, followlinks=True):
        for fname in walk[2]:
            if fname.endswith('.xml'):
                fullname = os.path.normpath(os.path.join(walk[0], fname))
                filepath = walk[0]
                data = open(fullname, 'rb').read()

                # hack off xml 1.1 stuff
                if data.startswith(b'<?xml version="1.1" encoding="UTF-8"?>'):
                    data = data[len(b'<?xml version="1.1" encoding="UTF-8"?>'):]
                data = data.replace(b"&#27;", b"")

                parser = etree.XMLParser(remove_blank_text = True, resolve_entities=False)
                parser.feed(data)
                try:
                    t = parser.close()
                except XMLSyntaxError:
                    print("ЕГГОГ АТ {} of {} bytes".format(fullname, len(data)))
                    print(data)
                    raise
                if t.tag != "sounds":
                    continue
                for sound in t.xpath("sound"):
                    sourceref = "{}:{}".format(fullname, sound.sourceline)
                    soundobject = Sound(sourceref, sound, sfcount, chandict)
                    for soundfile in sound.xpath("soundFile"):
                        sourceref2 = "{}:{}".format(fullname, soundfile.sourceline)
                        fn = soundfile.get("fileName")
                        if fn is None:
                            print("{}: warning: missing fileName attribute, ignoring soundFile.".format(sourceref2))
                            continue
                        fn = os.path.normpath(os.path.join(filepath, fn))
                        try:
                            filerefs[fn] += 1
                        except KeyError:
                            filerefs[fn] = 1
                        if not os.path.exists(fn):
                            print("{}: warning: reference to missing file {}, ignoring soundFile".format(sourceref2, fn))
                            continue
                        soundfile.set("fileName", fn)
                        soundobject.append(SoundFile(soundfile))
                        try:
                            filesizes[soundobject.channel].append(os.path.getsize(fn))
                        except KeyError:
                            filesizes[soundobject.channel] = [os.path.getsize(fn)]
                        sfcount += 1
                    if len(soundobject) == 0:
                        if soundobject.channel_id == 0:
                            if Wall:
                                print("{}: warning: sound with no channel and no soundfiles".format(sourceref))
                            continue
                        elif soundobject.loop == 0:
                            print("{}: warning: sound for channel '{}' without loop and no soundfiles, ignoring.".format(sourceref, soundobject.channel))
                            continue
                        else:
                            loop = sound.get('loop')
                            if loop is None:
                                print("{}:{} error: sound w/o sfiles, a channel, but no loop".format(fullname, sound.sourceline))
                            elif loop != 'stop':
                                print("{}:{} error: sound w/o sfiles, a channel, loop={}".format(fullname, sound.sourceline, sound.get("loop")))
                    sounds.append(soundobject)

    for chan in filesizes.keys():
        l = len(filesizes[chan])
        if l == 0:
            print("channel {} empty.".format(chan))
        print("file size percentiles for channel {}".format(chan))
        filesizes[chan].sort()
        for pct in [ 0.2, 0.4, 0.6, 0.8, 1.0 ]:
            sub = filesizes[chan][:int(l*pct)]
            pct_cnt = len(sub)
            if len(sub):
                pct_val = "{:d} bytes".format(sub[-1])
            else:
                pct_val = "n/a"
            sum = 0
            for s in sub:
                sum += s
            print("{:d}th: {}, {:d} files {:d} bytes total".format(
                int(100*pct), pct_val, pct_cnt, sum))

    return sounds, sfcount, chandict

def dump_the_dump(dumpname, sounds, sfcount, chandict):
    dump = bytearray()
    dump += struct.pack("iiii", 0, 0, 0, 0) # header placeholder
    strtab = StringTab()
    sftab = StringTab(kill_dupes = False)
    for s in sounds:
        dump += s.dump(sftab, strtab)
    sfdump = sftab.dump()
    strdump = strtab.dump()
    chancount = len(chandict) - 1
    del chandict[None]
    chans = ' '.join(chandict.keys())
    print("{} soundfiles\n{} sounds\n{} channels: {}\n{} bytes in strings\n{} string dupes killed\n{} bytes in structs\n{} total".format(
                sfcount,
                len(sounds),
                chancount, chans,
                len(strdump), strtab.dupes_killed,
                len(dump) - 16 + len(sfdump),
                len(strdump) + len(dump) + len(sfdump)))
    struct.pack_into("iiii", dump, 0, len(sounds), sfcount, len(strdump), chancount)
    df = open(dumpname, "wb")
    df.write(dump)
    df.write(sfdump)
    df.write(strdump)
    df.close()

def main():
    global Wall
    ap = argparse.ArgumentParser(description = 'a simpler code generator')
    ap.add_argument('src', metavar='srcdir', help='source (SoundSense packs) dir')
    ap.add_argument('dst', metavar='dstfile', default='soundsense.index', help='destination file', nargs='?')
    ap.add_argument('-v', default=False, action='store_true', help='complain more')
    pa = ap.parse_args()
    Wall = pa.v
    dump_the_dump(pa.dst, *walk_the_walk(pa.src))

if __name__ == '__main__':
    main()
