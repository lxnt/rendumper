# -*- encoding: utf-8 -*-

import dfr

import os, time, math
import logging, ctypes
from collections import namedtuple

from py3sdl2 import *

from OpenGL.GL import *
from OpenGL.error import GLError

from pygame2.sdl.video import SDL_Surface

logconfig(calltrace = os.environ.get('GLTRACE'))
#logging.getLogger('fgt.shader').setLevel(logging.DEBUG)
#import sys, pprint
#print(pprint.pformat(sys.modules))


class PmsPageman(object):
    rawcel = namedtuple("RawTextureCel", "pos surf w h")
    def __init__(self, raws, album_w = 1024):
        first = self.rawcel(*raws[0])
        psize = max(first.w, first.h)
        par = first.w / first.h
        if par > 1:
            self.pszar = Coord3(1, 1/par, psize)
        else:
            self.pszar = Coord3(par, 1, psize)
        
        fidx_side = int(math.ceil(math.sqrt(len(raws))))
        self.findex = CArray(None, "HHHH", fidx_side*fidx_side)
        
        cx = 0 # stuff in cels, filling up partially occupied rows 
        cy = 0 # with cels of lesser height.
        row_h = first.h # current row height.
        index = 0
        album = rgba_surface(album_w, album_w)
        for cel in ( self.rawcel(*r) for r in raws):
            if cel.surf == 0:
                index += 1
                continue
            src = Rect(0,0, cel.w, cel.h)
            if cx >= album_w:
                cx = 0
                cy += row_h
                row_h = cel.h
                if cy + row_h > album.h: # grow it
                    a = rgba_surface(album.w, album.h + 1024)
                    a.fill((0,0,0,0))
                    a.blit(album, Rect(0, 0, a.w, album.h), Rect(0, 0, a.w, album.h))
                    album = a
            dst = Rect(cx, cy, cel.w, cel.h)
            surf = SDL_Surface.from_address(cel.surf)
            album.blit(surf, dst, src)
            self.findex.set((cx, cy, cel.w, cel.h), index)
            index += 1
            cx += cel.w
        # cut off unused tail
        a = rgba_surface(album.w, cy + row_h)
        a.blit(album, Rect(0, 0, a.w, album.h), Rect(0, 0, a.w, cy + row_h))
        self.font = a
        self.count = index
        
        self.dump("eba")
        
    def dump(self, dumpdir):
        self.font.write_bmp(os.path.join(dumpdir, 'album.bmp'))
        self.findex.dump(open(os.path.join(dumpdir, "album.index"), 'wb'))

    def upload(self, tex):
        self.font.upload_tex2d(tex.font)
        upload_tex2d(tex.findex, GL_RGBA16UI,
            self.findex.w, self.findex.h,
            GL_RGBA_INTEGER, GL_UNSIGNED_SHORT, self.findex.ptr, GL_NEAREST)

class PmsShader(Shader0):
    sname = "pms"
    aloc = { b'screen': 0,
             b'texpos': 1,
             b'addcolor': 2,
             b'grayscale': 3,
             b'cf': 4,
             b'cbr': 5,
             b'position': 6 }

    def __call__(self, tex, gridsize, pszar, ansicolors, final_alpha):
        glUseProgram(self.program)
        glUniform3f(self.uloc[b'pszar'], *pszar)
        glUniform2i(self.uloc[b'gridsize'], *gridsize)        
        glUniform1f(self.uloc[b'final_alpha'], final_alpha)
        ac_uloc = self.uloc[b"ansicolors"] if self.uloc[b"ansicolors"] != -1 else self.uloc[b"ansicolors[0]"]
        glUniform3fv(ac_uloc, 16, bar2voidp(ansicolors))
        glActiveTexture(GL_TEXTURE0)
        glBindTexture(GL_TEXTURE_2D, tex.font)
        glUniform1i(self.uloc[b"font"], 0)
        glActiveTexture(GL_TEXTURE1)
        glBindTexture(GL_TEXTURE_2D, tex.findex)
        glUniform1i(self.uloc[b"findex"], 1)

class PmsVAO(VAO0):
    _primitive_type = GL_POINTS
    _data_type = None # varies, not of interest
    _attrs = [ # index size type stride offset 0, 2, GL_INT, 0, 0 
        VertexAttr( 0, 4, GL_UNSIGNED_BYTE, 0, None), # screen: { ch, fg, bg, bold }
        VertexAttr( 1, 1, GL_INT, 0, None),           # texpos
        VertexAttr( 2, 1, GL_INT, 0, None),           # addcolor
        VertexAttr( 3, 1, GL_BYTE, 0, None),          # grayscale
        VertexAttr( 4, 1, GL_BYTE, 0, None),          # cf
        VertexAttr( 5, 1, GL_BYTE, 0, None),          # cbr
        VertexAttr( 6, 2, GL_SHORT, 0, None),         # position/grid
    ]
    
    def update(self, ulod):
        """ relies on wha? on ze buffer not being grown/shrinked at all. it's 9Mb all the way
            needs to be called each time there's a game tick """
        glcalltrace("{}.{}()".format(self.__class__.__name__, 'update'))
        count = ulod['w'] * ulod['h']
        do_create = self._count == 0 or self._count > count
        do_reset_offsets = count != self._count
        self._count = count
        data_size = ulod['w']*ulod['h']*16
        data_ptr = ctypes.c_void_p(ulod['head'])
        
        if do_reset_offsets:
            self._attrs[0] = self._attrs[0]._replace(offset = ulod['screen'])
            self._attrs[1] = self._attrs[1]._replace(offset = ulod['texpos'])
            self._attrs[2] = self._attrs[2]._replace(offset = ulod['addcolor'])
            self._attrs[3] = self._attrs[3]._replace(offset = ulod['grayscale'])
            self._attrs[4] = self._attrs[4]._replace(offset = ulod['cf'])
            self._attrs[5] = self._attrs[5]._replace(offset = ulod['cbr'])
            self._attrs[6] = self._attrs[6]._replace(offset = ulod['grid'])
            self._create()
        
        glBindBuffer(GL_ARRAY_BUFFER, self._vbo_name)
        if do_create:
            glBufferData(GL_ARRAY_BUFFER, data_size, data_ptr, GL_STREAM_DRAW)
            self._create() # just resets attrs in fact
        elif do_reset_offsets:
            self._create() # just resets attrs in fact
        else:
            glBufferSubData(GL_ARRAY_BUFFER, 0, data_size, data_ptr)

class PmsRenderer(object):
    log = logging.getLogger("pms.renderer")
    def __init__(self, shaderdir):
        self.shaderdir = shaderdir
        dfr.set_callback('gl_init',     self.gl_init)
        dfr.set_callback('gl_fini',     self.gl_fini)
        dfr.set_callback('zoom',        self.zoom)
        dfr.set_callback('resize',      self.resize)
        dfr.set_callback('render',      self.render)
        dfr.set_callback('accept_textures', self.accept_textures)
        
        self.gridsize = Size2(None,None)
        self.pszar = Coord3(None, None, None)
        self.pageman = None

    def gl_init(self):
        # init stuff that requires a context
        glEnable(GL_VERTEX_PROGRAM_POINT_SIZE)
        glEnable(GL_BLEND)
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)
        glDisable(GL_DEPTH_TEST)
        
        self.vao = PmsVAO()
        self.shader = PmsShader(sdir=self.shaderdir)
        self.tex = namedtuple("Texnames", "font findex")._make(glGenTextures(2))

    def gl_fini(self):
        print("gl_fini woo-hoo")
        
    def zoom(self, cmd): 
        #enum zoom_commands { zoom_in, zoom_out, zoom_reset, zoom_fullscreen, zoom_resetgrid };
        #                         0        1         2           3                4
        npz = self.pszar.z
        if cmd == 0:
            npz += 1
        elif cmd == 1:
            npzz -= 1
        elif cmd == 2:
            npz = self.pageman.pszar.z
        elif cmd == 3:
            dfr.toggle_fullscreen()
            return
        elif cmd == 4:
            pass
        else:
            raise ValueError("unknown zoom_cmd '{}'".format(repr(cmd)))
        
        if npz < 4 or npz > 1024:
            return
        self.pszar._replace(z = npz)
        print("zoom({}) woo-hoo".format(cmd))
    
    def toggle_fullscreen(self):
        pass
        
    def resize(self, w, h):
        # do not touch pszar.
        # set up viewport to show intergral number of tiles.
        ng = Size2( int(w//(self.pszar.z*self.pszar.x)),
                    int(h//(self.pszar.z*self.pszar.y)) )
        gpx = Size2( int(ng.w*self.pszar.z*self.pszar.x), 
                     int(ng.h*self.pszar.z*self.pszar.y) )
        dfr.grid_resize(*ng)
        self.gridsize = Size2(*ng)
        pad = Size2( w - gpx.w, h - gpx.h )
        glViewport(int(pad.w//2), int(pad.h//2), gpx.w, gpx.h)
        self.log.error("resize({},{}): grid={} pad={}".format(w, h, ng, pad))
        
    def render(self):
        glClearColor(0,0,0,1)
        glClear(GL_COLOR_BUFFER_BIT)
        self.shader(
            tex = self.tex,
            gridsize = self.gridsize, 
            pszar = self.pszar, 
            ansicolors = dfr.ccolor(),
            final_alpha = 1.0 
        )
        self.vao.update(dfr.ulod_info())
        self.vao()
        
    def accept_textures(self, raws):
        self.pageman = PmsPageman(raws)
        self.pageman.upload(self.tex)
        self.pszar = self.pageman.pszar._make(self.pageman.pszar)
        self.resize(*dfr.window_size())

pmr = PmsRenderer(os.environ['PGLIBDIR'] + '/shaders')
    


