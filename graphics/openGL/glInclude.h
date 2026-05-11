#ifdef __mac__

#include <OpenGL/gl.h>
#include <OpenGL/glu.h>


#elif defined(RASPBIAN) || defined(__ANDROID__)

// GL ES 1.x (与 Raspbian 共用代码路径)
#include <GLES/gl.h>

// 一些在 GLES 中名字不同或缺失的常量/函数
#define GLdouble     GLfloat
#define GL_CLAMP     GL_CLAMP_TO_EDGE
#define glClearDepth glClearDepthf
#define glOrtho      glOrthof
#define glFrustum    glFrustumf
#define glGetDoublev glGetFloatv
#define GL_SOURCE0_RGB GL_SRC0_RGB
#define GL_SOURCE0_ALPHA GL_SRC0_ALPHA

#ifndef __ANDROID__
// Raspbian 还能用 mesa 提供的 glu
#include <GL/glu.h>
#endif




#else

#include <GL/gl.h>
#include <GL/glu.h>



#ifdef WIN32
// on Windows, some stuff that's normally in gl.h (1.2 and 1.3 stuff) is
// in glext
#include <GL/glext.h>
#endif


#endif
