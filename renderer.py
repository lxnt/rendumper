# -*- encoding: utf-8 -*-

import dfr
import os, time
#import logging
#logging.basicConfig(level=logging.DEBUG)

import OpenGL
OpenGL.FORWARD_COMPATIBLE_ONLY = True
OpenGL.FULL_LOGGING = 'GLTRACE' in os.environ
OpenGL.ERROR_ON_COPY = True

from OpenGL.GL import *
from OpenGL.GL.shaders import *
from OpenGL.GLU import *
from OpenGL.GL.ARB import shader_objects
from OpenGL.GL.ARB.texture_rg import *
from OpenGL.GL.ARB.framebuffer_object import *
from OpenGL.GL.ARB.debug_output import *

from OpenGL.error import GLError

def gl_init():
    print("gl_init woo-hoo")
    
def gl_fini(): 
    print("gl_fini woo-hoo")
    
def zoom(cmd): 
    print("zoom({}) woo-hoo".format(cmd))
    
def resize(w, h):
    print("resize({}, {}) woo-hoo".format(w, h))
    dfr.grid_resize(80,50)
    print("ulod: {}".format(repr(dfr.ulod_info())))
    
def uniforms():
    print("uniforms woo-hoo")
    time.sleep(0.1)
    
def accept_textures(raws):
    print("accept_textures woo-hoo")
    print("{} raws: {}".format(len(raws), repr(raws)))

dfr.set_callback('gl_init', gl_init)
dfr.set_callback('gl_fini', gl_fini)
dfr.set_callback('zoom', zoom)
dfr.set_callback('resize', resize)
dfr.set_callback('uniforms', uniforms)
dfr.set_callback('accept_textures', accept_textures)


