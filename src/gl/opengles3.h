
// This is just a slight modification of
// SDL2/SDL_opengles2.h to provide GLES3.

// TODO: Support Windows/Visual Studio

#include "SDL_config.h"

#if !defined(_MSC_VER) && !defined(SDL_USE_BUILTIN_OPENGL_DEFINITIONS)

#ifdef __IPHONEOS__
#include <OpenGLES/ES3/gl.h>
#include <OpenGLES/ES3/glext.h>
#else
#include <GLES3/gl3platform.h>
#include <GLES3/gl3.h>
#endif

#else /* _MSC_VER */

#error "Windows is not supported yet."

/* OpenGL ES2 headers for Visual Studio */
// #include "SDL_opengles2_khrplatform.h"
// #include "SDL_opengles2_gl2platform.h"
// #include "SDL_opengles2_gl2.h"
// #include "SDL_opengles2_gl2ext.h"

#endif /* _MSC_VER */

#ifndef APIENTRY
#define APIENTRY GL_APIENTRY
#endif
