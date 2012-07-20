#include <cstdio>
#include "iplatform.h"
#include "irenderer.h"
#include "ikeyboard.h"
#include "itextures.h"
#include "imusicsound.h"

#include "glue.h"

void test_platform() {
    iplatform *p = getplatform();
}

void test_sound() {
    imusicsound *ms = getmusicsound();
}

void test_renderer() {
    irenderer *r = getrenderer();
    isimuloop *sl = getsimuloop();
    itextures *tm = gettextures();
    ikeyboard *k = getkeyboard();
}

int main (int argc, char* argv[]) {
    if (argc != 4) {
	fprintf(stderr, "Usage: ./test platform-name sound-name renderer-name\n");
	return 1;
    }
    load_modules(argv[1], argv[2], argv[3]);
    
    test_platform();
    test_sound();
    test_renderer();
    
    return 0;
}