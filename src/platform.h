#ifndef PLATFORM_H
#define PLATFORM_H

//
// vibrate
//

// duration
extern const int PLATFORM_VIBRATE_TAP;
extern const int PLATFORM_VIBRATE_SHORT;

// api
void platform_vibrate(int ms);


//
// open links in browser
//
void platform_open_website(const char *url);

#endif

