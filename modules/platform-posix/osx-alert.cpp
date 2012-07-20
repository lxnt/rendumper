#include "cocoa stuff"
/* http://cocoadev.com/wiki/NSRunAlertPanel ? */

bool osx_alert(const char *text, const char *caption, bool yesno) {
    if (yesno) 
	return ( 0 == CocoaAlertPanel(caption, text, "Yes", "No", NULL) );
    
    CocoaAlertPanel(caption, text, "OK", NULL, NULL);
    return true;
}