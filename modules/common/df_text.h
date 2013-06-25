#if !defined(DF_ZHBAN_H)
#define DF_ZHBAN_H

#include <vector>
#include <stdint.h>

#include "zhban.h"

/* Here goes stuff related to the zhban text shaper support */
/* Note: the C++ shit here means simuloop and renderer modules used
   together must use the same C++ runtime. I hate this. So ...
   TODO: rewrite this in stdlib-free C++ or plain old C */

struct df_text_t {
    std::vector<zhban_rect_t> zrects;           // shaping results
    std::vector<uint32_t> attr_offsets;         // per-string offsets into *attrs buffer
    std::vector<uint32_t> grid_coords;          // destination grid coords
    std::vector<uint32_t> string_lengths;       // string lengths
    std::vector<uint32_t> string_offsets;       // per-string offsets into *strings buffer
    std::vector<uint32_t> pixel_offsets;        // left offset when blitting

    uint16_t *strings;                          // buffer with all the strings
    uint32_t strings_used;                      // used string buffer space
    uint32_t strings_allocated;                 // allocated string buffer space
    uint8_t *attrs;                             // buffer with all strings' per-char attributes
    uint32_t attrs_used;
    uint32_t attrs_allocated;

    df_text_t() :
        zrects(), attr_offsets(), grid_coords(),
        string_lengths(), string_offsets(), pixel_offsets(),
        strings(NULL), strings_used(0), strings_allocated(0),
        attrs(NULL), attrs_used(0), attrs_allocated(0)
        { }

    void reset() {
        zrects.clear();
        attr_offsets.clear();
        grid_coords.clear();
        string_lengths.clear();
        string_offsets.clear();
        pixel_offsets.clear();
        attrs_used = 0;
        strings_used = 0;
    }

    void add_string(const uint32_t grid_x, const uint32_t grid_y,
                    const uint16_t *str, const char *atr, uint32_t len,
                    const uint32_t w, const uint32_t h,
                    const uint32_t ox, const uint32_t oy,
                    const uint32_t pixel_offset) {
        if (strings_allocated - strings_used < len*2) {
            strings_allocated = strings_allocated < len * 2 ? len * 4 : strings_allocated * 2;
            strings = (uint16_t *)realloc(strings, strings_allocated);
        }
        memcpy(strings + strings_used/2, str, len*2);
        string_offsets.push_back(strings_used);
        string_lengths.push_back(len);
        pixel_offsets.push_back(pixel_offset);
        strings_used += len*2;

        if (attrs_allocated - attrs_used < len) {
            attrs_allocated = attrs_allocated < len ? len * 2 : attrs_allocated * 2;
            attrs = (uint8_t *)realloc(attrs, attrs_allocated);
        }
        memcpy(attrs + attrs_used, atr, len);
        attr_offsets.push_back(attrs_used);
        attrs_used += len;

        grid_coords.push_back(grid_x);
        grid_coords.push_back(grid_y);

        zhban_rect_t zrect;
        zrect.w = w;
        zrect.h = h;
        zrect.origin_x = ox;
        zrect.origin_y = oy;
        zrects.push_back(zrect);
    }

    int size() { return string_offsets.size(); }
};

#endif