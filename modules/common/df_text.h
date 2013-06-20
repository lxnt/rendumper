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
    std::vector<zhban_rect_t> zrects;
    std::vector<uint32_t> attr_offsets;
    std::vector<uint32_t> grid_coords;
    std::vector<uint32_t> string_lengths;
    std::vector<uint32_t> string_offsets;
    std::vector<uint32_t> clustermap_offsets;

    uint16_t *strings;
    uint32_t strings_used;
    uint32_t strings_allocated;
    uint8_t *attrs;
    uint32_t attrs_used;
    uint32_t attrs_allocated;
    uint32_t clustermap_size;

    df_text_t() :
        zrects(), attr_offsets(), grid_coords(),
        string_lengths(), string_offsets(),
        clustermap_offsets(),
        strings(NULL), strings_used(0), strings_allocated(0),
        attrs(NULL), attrs_used(0), attrs_allocated(0),
        clustermap_size(0)
        { }

    void reset() {
        zrects.clear();
        attr_offsets.clear();
        grid_coords.clear();
        string_lengths.clear();
        string_offsets.clear();
        clustermap_offsets.clear();
        clustermap_size = 0;
        attrs_used = 0;
        strings_used = 0;
    }

    void add_string(const uint32_t grid_x, const uint32_t grid_y,
                    const uint16_t *str, const char *atr, uint32_t len,
                    const uint32_t w, const uint32_t h,
                    const uint32_t ox, const uint32_t oy) {
        if (strings_allocated - strings_used < len*2) {
            strings_allocated = strings_allocated < len * 2 ? len * 4 : strings_allocated * 2;
            strings = (uint16_t *)realloc(strings, strings_allocated);
        }
        memcpy(strings + strings_used/2, str, len*2);
        string_offsets.push_back(strings_used);
        string_lengths.push_back(len);
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

        clustermap_offsets.push_back(w);
        clustermap_size += w*4;
    }

    int size() { return string_offsets.size(); }
};

#endif