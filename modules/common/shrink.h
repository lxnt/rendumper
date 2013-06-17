#if !defined(SHRINK_H)
#define SHRINK_H

#include <string>
#include <vector>
#include <cstdint>

extern const unsigned *codepage437;

namespace shrink {

/* well well well

    Task: text shrinker that works on both cp437 and ucs-2
    with minimal code duplication.
    
    Constructor would need to convert from cp437 to ucs-2
    in one case, but not in the other.
    
    If we do a parent class template with shrinking function,
    then we end up with double initialization of data members,
    since C++ prohibits calling parent members' constructor
    from child class constructor.
    
    If we do a single class template, then its behaviour
    will depend on template parameter's unit sizeof which
    is very ugly and incomprehensible.
  
    Thus we're left with either double initialization
    of data members or a bunch of out-of-class 
    templated implementations of methods.
    
    That's what the "friend" hack is for, now I understand.
    Only I'm not sure it'd work with templated functions :-/
    
    Maybe it's possible to invert inheritance, and make a 
    templated child of conrete base class.. oh no, I won't even
    check if it is. What a mess.
*/
    
template <class chardata_t> 
void eat_leading_article(chardata_t& chardata, std::string& attrdata) {
    if (chardata.size() < 2)
        return;

    switch (chardata[0]) {
    case 'A':
    case 'a':
        if ((chardata[1] == ' ')) {
            chardata.erase(chardata.begin(), chardata.begin() + 2);
            attrdata.erase(0, 2);
            return;
        }
        if (    (chardata.size() >= 3)
             && (chardata[1] == 'n')
             && (chardata[2] == ' ')) {
            chardata.erase(chardata.begin(), chardata.begin() + 3);
            attrdata.erase(0, 3);
            return;
        }
    case 'T':
    case 't':
        if (    (chardata.size() >= 4)
             && (chardata[1] == 'h')
             && (chardata[2] == 'e')
             && (chardata[3] == ' ')) {
            chardata.erase(chardata.begin(), chardata.begin() + 4);
            attrdata.erase(0, 4);
            return;
        }
    default:
        return;
    }
}

/* cuts off any articles off the beginning, then starts cutting vowels
   from the end. If that's not enough, just cuts off tail */
template <class chardata_t>
void the_shrink(chardata_t& chardata, std::string& attrdata, unsigned len) {
    if (chardata.size() <= len)
        return;

    eat_leading_article(chardata, attrdata);

    if (chardata.size() <= len)
        return;

    for (int32_t l = chardata.size() - 1; (l > 0) && (chardata.size() > len); --l)
        switch (chardata[l]) {
        case 'a':
        case 'e':
        case 'i':
        case 'o':
        case 'u':
        case 'A':
        case 'E':
        case 'I':
        case 'O':
        case 'U':
            chardata.erase(chardata.begin() + l, chardata.end());
            attrdata.erase(l);
            break;
        case ' ':
        default:
            break;
        }

    if (chardata.size() > len) {
        chardata.resize(len);
        attrdata.resize(len);
    }
}

struct unicode {
    std::vector<uint16_t> chardata;
    std::string attrdata;

    unicode(const std::string& str, const std::string& attrs): chardata(), attrdata(attrs) {
        chardata.resize(str.size());
        unsigned char *data = (unsigned char *)(str.data());
        for (size_t i = 0; i < str.size(); i++)
            chardata[i] = codepage437[data[i]];
    }

    unicode(const char* str, const char* attrs, const unsigned len): chardata(), attrdata(attrs, len) {
        chardata.resize(len);
        unsigned char *data = (unsigned char *)str;
        for (unsigned i = 0; i < len; i++)
            chardata[i] = codepage437[data[i]];
    }

    void shrink(unsigned len) { the_shrink(chardata, attrdata, len); }
    
    /* those for add_text_buffer_t() */
    size_t size() const { return chardata.size(); }
    const uint16_t *chars() { return chardata.data(); }
    const char *attrs() { return attrdata.data(); }
};

struct codepage {
    std::string chardata;
    std::string attrdata;

    codepage(const std::string& str, const std::string& attrs) : chardata(str), attrdata(attrs) { }
    codepage(const char* str, const char* attrs, const unsigned len) : chardata(str, len), attrdata(attrs, len) { }
    
    void shrink(unsigned len) { the_shrink(chardata, attrdata, len); }
    
    /* those for bputs()-ing it */
    size_t size() { return chardata.size(); }
    const char *chars() { return chardata.data(); }
    const char *attrs() { return attrdata.data(); }
};

}
#endif
