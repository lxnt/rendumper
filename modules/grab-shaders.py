#!/usr/bin/python

import sys, os, os.path, glob

usage = """
 Function: collect shaders.

 Usage:
  grab-shaders.py <shaders dir> <output file>
  shaders-dir is expected to contain file pairs
  of form <name>.fs and <name>.vs
  output-file contains generated C code:
  structure array and a getter function.

  char *get_embedded_shader_source(const char *setname, const unsigned int type);

  function accepts shader type GLenum as an unsigned int.

  GL_WHATEVER_SHADER constants' values are hardcoded as
  to not recompile 712K of glext.h or 828K of glew.h again and again.

  returns NULL on not found; null-terminated shader source otherwise.

"""

def main():
    sdir = sys.argv[1]
    fname = sys.argv[2]

    vsp = glob.glob(os.path.join(sdir, '*.vs'))
    fsp = glob.glob(os.path.join(sdir, '*.fs'))

    vstab = []
    for n in vsp:
        vstab.append(os.path.basename(n))

    vfstab = []
    for n in fsp:
        vsn = os.path.basename(n)[:-2] + 'vs'
        if vsn in vstab:
            vfstab.append((vsn, os.path.basename(n)))
    if len(vfstab) == 0:
        sys.stderr.write("No shaders found")
        raise SystemExit(1)

    c_function = """
#include <string.h>

char *get_embedded_shader_source(const char *setname, const unsigned int type) {
    for (unsigned i = 0; i < sizeof(shader_source_sets)/sizeof(shader_source_set_t); i++)
        if (strcmp(shader_source_sets[i].name, setname) == 0)
            switch(type) {
            case 0x8B30: /* fragment */
                return shader_source_sets[i].f_src;
            case 0x8B31: /* vertex */
                return shader_source_sets[i].v_src;
            case 0x8DD9: /* geometry */
            case 0x8E87: /* tess ctrl */
            case 0x8E88: /* tess eval */
            case 0x91B9: /* compute */
                return NULL;  /* not supported */
            }
    return NULL; /* set not found */
}
"""

    c_struct = """
struct shader_source_set_t {
    const char *name;
    char *v_src, *f_src;
} shader_source_sets[] = {
"""
    item_f = """    {{ "{0}", {1}, {2} }} """
    c_chars = []
    def xpmize(fname, name):
        rv = 'char {0}[] = {{ \n    '.format(name)
        zeppelin = 0
        for c in file(fname).read():
            zeppelin += 1
            if zeppelin % 16 == 0:
                rv += "\n    "
            elif zeppelin % 8 == 0:
                rv += ' '
            rv += "0x{0:02x},".format(ord(c))
        return rv + " 0x00\n};";
    c_items = []
    for t in vfstab:
        vs, fs = t
        vs_sym = vs.replace('.', '_')
        fs_sym = fs.replace('.', '_')
        c_chars.append(xpmize(os.path.join(sdir, vs), vs_sym))
        c_chars.append(xpmize(os.path.join(sdir, fs), fs_sym))
        c_items.append(item_f.format(vs[:-3], vs_sym, fs_sym))
    c_code = "\n".join(c_chars)
    c_code += c_struct +  ",\n".join(c_items) + "\n};\n"
    c_code += c_function;
    file(fname, "w").write(c_code)

if __name__ == "__main__":
    main()






