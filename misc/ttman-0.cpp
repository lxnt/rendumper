static list<ttf_id> ttfstr;
void graphicst::addst(const string &str_orig, justification just, int space) {

# non-ttf case was here..

# in addition to possibly submitted tabs, change space after colons into them.
    if (str.size() > 2 && str[0] == ':' && str[1] == ' ')
        str[1] = '\t'; // EVIL HACK

    struct ttf_id id = {str, screenf, screenb, screenbright};

    ttfstr.push_back(id);

    if (just == justify_cont)
        return; // More later

    // This string is done. Time to render.

# **** ok, the above is done already.

    ttf_details details = ttf_manager.get_handle(ttfstr, just);

# here we go ...

ttf_details ttf_managerst::get_handle(const list<ttf_id> &text, justification just) {
    // Check for an existing handle
    handleid id = {text, just};
    auto it = handles.find(id);
    if (it != handles.end())
        return it->second;

# ** cache miss so it looks **

    // Right. Make a new one.
    int handle = ++max_handle;

    // Split out any tabs
    list<ttf_id> split_text;

# ** same as ttfstr - which is known here as &text
#
# for each string in the string list (a result of justify_cont):
#     pos is an index in the concatenation of all strings.
#     for each \t in the string
#         tabpos is its index
#         split the string at this place
#         push left substring     into split_text
#         push tab-subst "tabber" into split_text
#         set pos to after the tab
#     push the last remnant of the string into split text
#
# so we end up with list of strings w/o tab interspersed
# with special tab-"string"
#
    
    for (auto it = text.cbegin(); it != text.cend(); ++it) {
        int pos = 0;
        int tabpos;

        while ((tabpos = it->text.find("\t", pos)) != string::npos) {
            ttf_id left;
            left.fg = it->fg;
            left.bg = it->bg;
            left.bold = it->bold;
            left.text = it->text.substr(pos, tabpos - pos);
            
            split_text.push_back(left);
            
            ttf_id tabber;
            
            tabber.fg = tabber.bg = tabber.bold = 255;
            
            split_text.push_back(tabber);
            pos = tabpos + 1;
        }
        ttf_id right;
        right.fg = it->fg;
        right.bg = it->bg;
        right.bold = it->bold;
        right.text = it->text.substr(pos);

        split_text.push_back(right);
    }

# ** hmm.
# 
# for all strings in the list:
#     accumulate character count in text_width
#     accumulate pixel count in ttf_width
#
# regular strings go via SDL_TTF
#
# tab placeholders count for one character, but
# their pixel width is calculated so that they
# emulate real tab stops.
#
# tab stops are assumed to be uniform.
# tab width is font's em_width * tab_width which is 2 by default.
#
    // Find the total width of the text
    vector<Uint16> text_unicode;
    int ttf_width = 0, ttf_height = 0, text_width = 0;
    
    for (auto it = split_text.cbegin(); it != split_text.cend(); ++it) {
        if (it->fg == 255 && it->bg == 255 && it->bold == 255) {
            // Tab stop
            int tabstop = tab_width * em_width;
            int tab_width = tabstop - ((ttf_width - 1) % tabstop) + 1;
            ttf_width += tab_width;
            text_width += 1;
        } else {
            cp437_to_unicode(it->text, text_unicode);
            int slice_width, slice_height;
            TTF_SizeUNICODE(font, &text_unicode[0], &slice_width, &slice_height);
            ttf_width += slice_width;
            text_width += it->text.size();
        }
    }

# 
# ceiling is set in ttf_manager::init() and is what I call pszy.
# while tile_width is pszx.
#
    ttf_height = ceiling;

    // Compute geometry
    double grid_width = double(ttf_width) / tile_width;

    double offset = just == justify_right ? text_width - grid_width : just == justify_center ? (text_width - grid_width) / 2 : 0;
    if (just == justify_center && text_width % 2)
    offset += 0.5; // Arbitrary fixup for approximate grid centering
# ewww ...
#
    switch(just) {
    case justify_right:
        offset = text_width - grid_width;  // note how this fucks up tabs: they're 1 in text_w, but possibly more in grid_w
        break;
    case justify_center:
        offset = (text_width - grid_width) / 2; // same as above. maybe not very visible with default tab separation of 2
        if (text_width % 2)
            offset += 0.5; // Arbitrary fixup for approximate grid centering (sic)
        break;
    default:
        offset = 0;
    }
#
    double fraction, integral;
    fraction = modf(offset, &integral);

    // Outputs:
# +0.001 :fp:
    const int grid_offset = int(integral + 0.001); // Tiles to move to the right in addst

# this actually makes sense, blit offset off grid cell origin
    const int pixel_offset = int(fraction * tile_width); // Black columns to add to the left of the image
    // const int full_grid_width = int(ceil(double(ttf_width) / double(tile_width) + fraction) + 0.1); // Total width of the image in grid units

# well, fuck tabs again, and arrive at some bogus pixel_width
    const int full_grid_width = text_width;
    const int pixel_width = full_grid_width * tile_width; // And pixels

# what's this? aha, that's why first attempt at full_grid got commented out
    assert(pixel_width >= ttf_width);

    // Store for later
    ttf_details ret;

    ret.handle = handle;
    ret.offset = grid_offset;
    ret.width = full_grid_width;

    handles[id] = ret;

    // We do the actual rendering in the render thread, later on.
    todo.push_back(todum(handle, split_text, ttf_height, pixel_offset, pixel_width));
    return ret;
}


# addst() contunues ...
#
    const int handle = details.handle;
    const int offset = details.offset;
    int width = details.width;

# int.. long.. who cares.. . ok this points to grid cell
# covered by the first ttf-rendered character.
#
    const int ourx = screenx + offset;
    unsigned int * const s = ((unsigned int*)screen + ourx*dimy + screeny);

# so just stuff it with the cache key up to the limit.
# 
    if (s < (unsigned int*)screen_limit)
        s[0] = (((unsigned int)GRAPHICSTYPE_TTF) << 24) | handle;

    // Also set the other tiles this text covers, but don't write past the end.
    if (width + ourx >= dimx)
        width = dimx - ourx - 1;

    for (int x = 1; x < width; ++x)
        s[x * dimy] = (((unsigned int)GRAPHICSTYPE_TTFCONT) << 24) | handle;

# update "cursor"
#
    // Clean up, prepare for next string.
    screenx = ourx + width;
    ttfstr.clear();




