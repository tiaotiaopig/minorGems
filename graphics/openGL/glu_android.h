// Minimal GLU replacement for Android NDK builds.
// NDK does not provide libGLU, but OneLife only uses gluPerspective / gluLookAt / gluProject.
// Inline implementations avoid modifying core minorGems source files.

#ifndef MINORGEMS_GLU_ANDROID_H
#define MINORGEMS_GLU_ANDROID_H

#ifdef __ANDROID__

#include <GLES/gl.h>
#include <math.h>

#ifndef GLU_ANDROID_PI
#define GLU_ANDROID_PI 3.14159265358979323846
#endif

// gluPerspective: perspective projection via glFrustumf
static inline void gluPerspective(GLfloat fovy, GLfloat aspect,
                                  GLfloat zNear, GLfloat zFar) {
    GLfloat top = zNear * (GLfloat)tan(fovy * (GLU_ANDROID_PI / 360.0));
    GLfloat right = top * aspect;
    glFrustumf(-right, right, -top, top, zNear, zFar);
}

// gluLookAt: view matrix setup (multiplies current matrix)
static inline void gluLookAt(GLfloat eyeX, GLfloat eyeY, GLfloat eyeZ,
                             GLfloat centerX, GLfloat centerY, GLfloat centerZ,
                             GLfloat upX, GLfloat upY, GLfloat upZ) {
    GLfloat f[3] = { centerX - eyeX, centerY - eyeY, centerZ - eyeZ };
    GLfloat fl = (GLfloat)sqrt(f[0]*f[0] + f[1]*f[1] + f[2]*f[2]);
    if (fl > 0) { f[0]/=fl; f[1]/=fl; f[2]/=fl; }

    GLfloat up[3] = { upX, upY, upZ };
    GLfloat upl = (GLfloat)sqrt(up[0]*up[0] + up[1]*up[1] + up[2]*up[2]);
    if (upl > 0) { up[0]/=upl; up[1]/=upl; up[2]/=upl; }

    // s = f x up
    GLfloat s[3] = { f[1]*up[2] - f[2]*up[1],
                     f[2]*up[0] - f[0]*up[2],
                     f[0]*up[1] - f[1]*up[0] };
    GLfloat sl = (GLfloat)sqrt(s[0]*s[0] + s[1]*s[1] + s[2]*s[2]);
    if (sl > 0) { s[0]/=sl; s[1]/=sl; s[2]/=sl; }

    // u = s x f
    GLfloat u[3] = { s[1]*f[2] - s[2]*f[1],
                     s[2]*f[0] - s[0]*f[2],
                     s[0]*f[1] - s[1]*f[0] };

    GLfloat m[16] = {
         s[0],  u[0], -f[0], 0,
         s[1],  u[1], -f[1], 0,
         s[2],  u[2], -f[2], 0,
         0,     0,     0,    1
    };
    glMultMatrixf(m);
    glTranslatef(-eyeX, -eyeY, -eyeZ);
}

// Helper: column-major 4x4 matrix * 4-vector
static inline void glu_android_mat4_mul_vec4(const GLdouble* m, const GLdouble* v, GLdouble* out) {
    int i;
    for (i = 0; i < 4; i++) {
        out[i] = m[i] * v[0] + m[i+4] * v[1] + m[i+8] * v[2] + m[i+12] * v[3];
    }
}

// gluProject: project object coordinates to window coordinates.
// Signature matches the standard GLU double version used by minorGems.
static inline GLint gluProject(GLdouble objX, GLdouble objY, GLdouble objZ,
                               const GLdouble* model, const GLdouble* proj,
                               const GLint* viewport,
                               GLdouble* winX, GLdouble* winY, GLdouble* winZ) {
    GLdouble in4[4]  = { objX, objY, objZ, 1.0 };
    GLdouble t1[4], t2[4];
    glu_android_mat4_mul_vec4(model, in4, t1);
    glu_android_mat4_mul_vec4(proj,  t1,  t2);
    if (t2[3] == 0.0) return GL_FALSE;
    t2[0] /= t2[3]; t2[1] /= t2[3]; t2[2] /= t2[3];

    *winX = viewport[0] + (1.0 + t2[0]) * viewport[2] / 2.0;
    *winY = viewport[1] + (1.0 + t2[1]) * viewport[3] / 2.0;
    *winZ = (1.0 + t2[2]) / 2.0;
    return GL_TRUE;
}

#endif /* __ANDROID__ */
#endif /* MINORGEMS_GLU_ANDROID_H */
