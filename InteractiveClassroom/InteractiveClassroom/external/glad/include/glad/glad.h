/*
 * Minimal hand-written OpenGL 3.3 core-profile function loader.
 * This purposefully mimics the public API surface of GLAD (same function
 * names, same usage pattern: gladLoadGLLoader(...)) but is a small, fully
 * self-contained implementation covering exactly the entry points this
 * project needs. Windows only (uses <windows.h> + opengl32.lib for the
 * legacy 1.1 entry points, and dynamic loading via wglGetProcAddress /
 * the loader function passed in for everything from GL 1.2 onward).
 */
#ifndef GLAD_GLAD_H_
#define GLAD_GLAD_H_

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

#include <GL/gl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void* (*GLADloadproc)(const char* name);

#ifndef APIENTRY
#define APIENTRY
#endif
#define APIENTRYP APIENTRY *

/* ---- Types that classic <GL/gl.h> (GL 1.1) does not declare ---- */
typedef char GLchar;
#ifdef _WIN64
typedef long long int GLsizeiptr;
typedef long long int GLintptr;
#else
typedef long int GLsizeiptr;
typedef long int GLintptr;
#endif

/* ---- Enums missing from GL 1.1 headers ---- */
#define GL_ARRAY_BUFFER                   0x8892
#define GL_ELEMENT_ARRAY_BUFFER           0x8893
#define GL_STATIC_DRAW                    0x88E4
#define GL_DYNAMIC_DRAW                   0x88E8
#define GL_VERTEX_SHADER                  0x8B31
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#define GL_INFO_LOG_LENGTH                0x8B84
#define GL_TEXTURE0                       0x84C0
#define GL_TEXTURE1                       0x84C1
#define GL_CLAMP_TO_EDGE                  0x812F
#define GL_MULTISAMPLE                    0x809D
#define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
#ifndef GL_BLEND_DST_RGB
#define GL_BLEND_DST_RGB                  0x80C8
#endif
#ifndef GL_BLEND_SRC_RGB
#define GL_BLEND_SRC_RGB                  0x80C9
#endif
#ifndef GL_BLEND_DST_ALPHA
#define GL_BLEND_DST_ALPHA                0x80CA
#endif
#ifndef GL_BLEND_SRC_ALPHA
#define GL_BLEND_SRC_ALPHA                0x80CB
#endif

/* ---- Framebuffer / renderbuffer object enums (GL 3.0 core) ---- */
#define GL_FRAMEBUFFER                    0x8D40
#define GL_READ_FRAMEBUFFER               0x8CA8
#define GL_DRAW_FRAMEBUFFER               0x8CA9
#define GL_RENDERBUFFER                   0x8D41
#define GL_COLOR_ATTACHMENT0              0x8CE0
#define GL_COLOR_ATTACHMENT1              0x8CE1
#define GL_DEPTH_ATTACHMENT               0x8D00
#define GL_FRAMEBUFFER_COMPLETE           0x8CD5
#define GL_DEPTH_COMPONENT24              0x81A6
#define GL_RGB8                           0x8051
#define GL_RGBA16F                        0x881A
#define GL_TEXTURE_2D_MULTISAMPLE         0x9100
#define GL_CLAMP_TO_BORDER                0x812D
#define GL_SRGB                           0x8C40
#define GL_SRGB8                          0x8C41
#define GL_SRGB_ALPHA                     0x8C42
#define GL_SRGB8_ALPHA8                   0x8C43
#ifndef GL_RED
#define GL_RED                            0x1903
#endif
#ifndef GL_RG
#define GL_RG                             0x8227
#endif

/* ---- Function pointer typedefs (GL 1.2+) ---- */
typedef void      (APIENTRYP PFNGLGENVERTEXARRAYSPROC)      (GLsizei n, GLuint* arrays);
typedef void      (APIENTRYP PFNGLBINDVERTEXARRAYPROC)      (GLuint array);
typedef void      (APIENTRYP PFNGLDELETEVERTEXARRAYSPROC)   (GLsizei n, const GLuint* arrays);
typedef void      (APIENTRYP PFNGLGENBUFFERSPROC)           (GLsizei n, GLuint* buffers);
typedef void      (APIENTRYP PFNGLBINDBUFFERPROC)           (GLenum target, GLuint buffer);
typedef void      (APIENTRYP PFNGLBUFFERDATAPROC)           (GLenum target, GLsizeiptr size, const void* data, GLenum usage);
typedef void      (APIENTRYP PFNGLBUFFERSUBDATAPROC)        (GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
typedef void      (APIENTRYP PFNGLDELETEBUFFERSPROC)        (GLsizei n, const GLuint* buffers);
typedef void      (APIENTRYP PFNGLVERTEXATTRIBPOINTERPROC)  (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
typedef void      (APIENTRYP PFNGLENABLEVERTEXATTRIBARRAYPROC)  (GLuint index);
typedef void      (APIENTRYP PFNGLDISABLEVERTEXATTRIBARRAYPROC) (GLuint index);
typedef void      (APIENTRYP PFNGLVERTEXATTRIBDIVISORPROC)    (GLuint index, GLuint divisor);
typedef void      (APIENTRYP PFNGLDRAWELEMENTSINSTANCEDPROC)  (GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instancecount);
typedef GLuint    (APIENTRYP PFNGLCREATESHADERPROC)         (GLenum type);
typedef void      (APIENTRYP PFNGLSHADERSOURCEPROC)         (GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
typedef void      (APIENTRYP PFNGLCOMPILESHADERPROC)        (GLuint shader);
typedef void      (APIENTRYP PFNGLGETSHADERIVPROC)          (GLuint shader, GLenum pname, GLint* params);
typedef void      (APIENTRYP PFNGLGETSHADERINFOLOGPROC)     (GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef GLuint    (APIENTRYP PFNGLCREATEPROGRAMPROC)        (void);
typedef void      (APIENTRYP PFNGLATTACHSHADERPROC)         (GLuint program, GLuint shader);
typedef void      (APIENTRYP PFNGLLINKPROGRAMPROC)          (GLuint program);
typedef void      (APIENTRYP PFNGLGETPROGRAMIVPROC)         (GLuint program, GLenum pname, GLint* params);
typedef void      (APIENTRYP PFNGLGETPROGRAMINFOLOGPROC)    (GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef void      (APIENTRYP PFNGLDELETESHADERPROC)         (GLuint shader);
typedef void      (APIENTRYP PFNGLDELETEPROGRAMPROC)        (GLuint program);
typedef void      (APIENTRYP PFNGLUSEPROGRAMPROC)           (GLuint program);
typedef GLint     (APIENTRYP PFNGLGETUNIFORMLOCATIONPROC)   (GLuint program, const GLchar* name);
typedef void      (APIENTRYP PFNGLUNIFORM1IPROC)            (GLint location, GLint v0);
typedef void      (APIENTRYP PFNGLUNIFORM1FPROC)            (GLint location, GLfloat v0);
typedef void      (APIENTRYP PFNGLUNIFORM2FPROC)            (GLint location, GLfloat v0, GLfloat v1);
typedef void      (APIENTRYP PFNGLUNIFORM3FPROC)            (GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void      (APIENTRYP PFNGLUNIFORM3FVPROC)           (GLint location, GLsizei count, const GLfloat* value);
typedef void      (APIENTRYP PFNGLUNIFORM4FVPROC)           (GLint location, GLsizei count, const GLfloat* value);
typedef void      (APIENTRYP PFNGLUNIFORM1IVPROC)           (GLint location, GLsizei count, const GLint* value);
typedef void      (APIENTRYP PFNGLUNIFORM1FVPROC)           (GLint location, GLsizei count, const GLfloat* value);
typedef void      (APIENTRYP PFNGLUNIFORMMATRIX4FVPROC)     (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void      (APIENTRYP PFNGLACTIVETEXTUREPROC)        (GLenum texture);
typedef void      (APIENTRYP PFNGLGENERATEMIPMAPPROC)       (GLenum target);
typedef void      (APIENTRYP PFNGLBLENDFUNCSEPARATEPROC)    (GLenum a, GLenum b, GLenum c, GLenum d);

/* ---- Framebuffer / renderbuffer function pointer typedefs (GL 3.0 core) ---- */
typedef void      (APIENTRYP PFNGLGENFRAMEBUFFERSPROC)      (GLsizei n, GLuint* framebuffers);
typedef void      (APIENTRYP PFNGLBINDFRAMEBUFFERPROC)      (GLenum target, GLuint framebuffer);
typedef void      (APIENTRYP PFNGLFRAMEBUFFERTEXTURE2DPROC) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef GLenum    (APIENTRYP PFNGLCHECKFRAMEBUFFERSTATUSPROC) (GLenum target);
typedef void      (APIENTRYP PFNGLDELETEFRAMEBUFFERSPROC)   (GLsizei n, const GLuint* framebuffers);
typedef void      (APIENTRYP PFNGLGENRENDERBUFFERSPROC)     (GLsizei n, GLuint* renderbuffers);
typedef void      (APIENTRYP PFNGLBINDRENDERBUFFERPROC)     (GLenum target, GLuint renderbuffer);
typedef void      (APIENTRYP PFNGLRENDERBUFFERSTORAGEPROC)  (GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void      (APIENTRYP PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC) (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height);
typedef void      (APIENTRYP PFNGLFRAMEBUFFERRENDERBUFFERPROC) (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void      (APIENTRYP PFNGLBLITFRAMEBUFFERPROC)      (GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter);
typedef void      (APIENTRYP PFNGLDELETERENDERBUFFERSPROC)  (GLsizei n, const GLuint* renderbuffers);
typedef void      (APIENTRYP PFNGLDRAWBUFFERSPROC)            (GLsizei n, const GLenum* bufs);
typedef void      (APIENTRYP PFNGLTEXIMAGE2DMULTISAMPLEPROC)  (GLenum target, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height, GLboolean fixedsamplelocations);

/* ---- Global function pointer variables (defined in glad.c) ---- */
extern PFNGLGENVERTEXARRAYSPROC        glad_glGenVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC        glad_glBindVertexArray;
extern PFNGLDELETEVERTEXARRAYSPROC     glad_glDeleteVertexArrays;
extern PFNGLGENBUFFERSPROC             glad_glGenBuffers;
extern PFNGLBINDBUFFERPROC             glad_glBindBuffer;
extern PFNGLBUFFERDATAPROC             glad_glBufferData;
extern PFNGLBUFFERSUBDATAPROC          glad_glBufferSubData;
extern PFNGLDELETEBUFFERSPROC          glad_glDeleteBuffers;
extern PFNGLVERTEXATTRIBPOINTERPROC    glad_glVertexAttribPointer;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC  glad_glEnableVertexAttribArray;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC glad_glDisableVertexAttribArray;
extern PFNGLVERTEXATTRIBDIVISORPROC    glad_glVertexAttribDivisor;
extern PFNGLDRAWELEMENTSINSTANCEDPROC  glad_glDrawElementsInstanced;
extern PFNGLCREATESHADERPROC           glad_glCreateShader;
extern PFNGLSHADERSOURCEPROC           glad_glShaderSource;
extern PFNGLCOMPILESHADERPROC          glad_glCompileShader;
extern PFNGLGETSHADERIVPROC            glad_glGetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC       glad_glGetShaderInfoLog;
extern PFNGLCREATEPROGRAMPROC          glad_glCreateProgram;
extern PFNGLATTACHSHADERPROC           glad_glAttachShader;
extern PFNGLLINKPROGRAMPROC            glad_glLinkProgram;
extern PFNGLGETPROGRAMIVPROC           glad_glGetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC      glad_glGetProgramInfoLog;
extern PFNGLDELETESHADERPROC           glad_glDeleteShader;
extern PFNGLDELETEPROGRAMPROC          glad_glDeleteProgram;
extern PFNGLUSEPROGRAMPROC             glad_glUseProgram;
extern PFNGLGETUNIFORMLOCATIONPROC     glad_glGetUniformLocation;
extern PFNGLUNIFORM1IPROC              glad_glUniform1i;
extern PFNGLUNIFORM1FPROC              glad_glUniform1f;
extern PFNGLUNIFORM2FPROC              glad_glUniform2f;
extern PFNGLUNIFORM3FPROC              glad_glUniform3f;
extern PFNGLUNIFORM3FVPROC             glad_glUniform3fv;
extern PFNGLUNIFORM4FVPROC             glad_glUniform4fv;
extern PFNGLUNIFORM1IVPROC             glad_glUniform1iv;
extern PFNGLUNIFORM1FVPROC             glad_glUniform1fv;
extern PFNGLUNIFORMMATRIX4FVPROC       glad_glUniformMatrix4fv;
extern PFNGLACTIVETEXTUREPROC          glad_glActiveTexture;
extern PFNGLGENERATEMIPMAPPROC         glad_glGenerateMipmap;
extern PFNGLBLENDFUNCSEPARATEPROC      glad_glBlendFuncSeparate;

extern PFNGLGENFRAMEBUFFERSPROC        glad_glGenFramebuffers;
extern PFNGLBINDFRAMEBUFFERPROC        glad_glBindFramebuffer;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC   glad_glFramebufferTexture2D;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC glad_glCheckFramebufferStatus;
extern PFNGLDELETEFRAMEBUFFERSPROC     glad_glDeleteFramebuffers;
extern PFNGLGENRENDERBUFFERSPROC       glad_glGenRenderbuffers;
extern PFNGLBINDRENDERBUFFERPROC       glad_glBindRenderbuffer;
extern PFNGLRENDERBUFFERSTORAGEPROC    glad_glRenderbufferStorage;
extern PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glad_glRenderbufferStorageMultisample;
extern PFNGLFRAMEBUFFERRENDERBUFFERPROC glad_glFramebufferRenderbuffer;
extern PFNGLBLITFRAMEBUFFERPROC        glad_glBlitFramebuffer;
extern PFNGLDELETERENDERBUFFERSPROC    glad_glDeleteRenderbuffers;
extern PFNGLDRAWBUFFERSPROC            glad_glDrawBuffers;
extern PFNGLTEXIMAGE2DMULTISAMPLEPROC  glad_glTexImage2DMultisample;

/* ---- Nice names (matches how real GLAD lets you call gl* directly) ---- */
#define glGenVertexArrays           glad_glGenVertexArrays
#define glBindVertexArray           glad_glBindVertexArray
#define glDeleteVertexArrays        glad_glDeleteVertexArrays
#define glGenBuffers                glad_glGenBuffers
#define glBindBuffer                glad_glBindBuffer
#define glBufferData                glad_glBufferData
#define glBufferSubData             glad_glBufferSubData
#define glDeleteBuffers             glad_glDeleteBuffers
#define glVertexAttribPointer       glad_glVertexAttribPointer
#define glEnableVertexAttribArray   glad_glEnableVertexAttribArray
#define glDisableVertexAttribArray  glad_glDisableVertexAttribArray
#define glVertexAttribDivisor       glad_glVertexAttribDivisor
#define glDrawElementsInstanced     glad_glDrawElementsInstanced
#define glCreateShader              glad_glCreateShader
#define glShaderSource              glad_glShaderSource
#define glCompileShader             glad_glCompileShader
#define glGetShaderiv                glad_glGetShaderiv
#define glGetShaderInfoLog          glad_glGetShaderInfoLog
#define glCreateProgram             glad_glCreateProgram
#define glAttachShader              glad_glAttachShader
#define glLinkProgram                glad_glLinkProgram
#define glGetProgramiv               glad_glGetProgramiv
#define glGetProgramInfoLog          glad_glGetProgramInfoLog
#define glDeleteShader               glad_glDeleteShader
#define glDeleteProgram              glad_glDeleteProgram
#define glUseProgram                 glad_glUseProgram
#define glGetUniformLocation         glad_glGetUniformLocation
#define glUniform1i                  glad_glUniform1i
#define glUniform1f                  glad_glUniform1f
#define glUniform2f                  glad_glUniform2f
#define glUniform3f                  glad_glUniform3f
#define glUniform3fv                 glad_glUniform3fv
#define glUniform4fv                 glad_glUniform4fv
#define glUniform1iv                 glad_glUniform1iv
#define glUniform1fv                 glad_glUniform1fv
#define glUniformMatrix4fv           glad_glUniformMatrix4fv
#define glActiveTexture              glad_glActiveTexture
#define glGenerateMipmap             glad_glGenerateMipmap
#define glBlendFuncSeparate          glad_glBlendFuncSeparate

#define glGenFramebuffers            glad_glGenFramebuffers
#define glBindFramebuffer            glad_glBindFramebuffer
#define glFramebufferTexture2D       glad_glFramebufferTexture2D
#define glCheckFramebufferStatus     glad_glCheckFramebufferStatus
#define glDeleteFramebuffers         glad_glDeleteFramebuffers
#define glGenRenderbuffers           glad_glGenRenderbuffers
#define glBindRenderbuffer           glad_glBindRenderbuffer
#define glRenderbufferStorage        glad_glRenderbufferStorage
#define glRenderbufferStorageMultisample glad_glRenderbufferStorageMultisample
#define glFramebufferRenderbuffer    glad_glFramebufferRenderbuffer
#define glBlitFramebuffer            glad_glBlitFramebuffer
#define glDeleteRenderbuffers        glad_glDeleteRenderbuffers
#define glDrawBuffers                glad_glDrawBuffers
#define glTexImage2DMultisample      glad_glTexImage2DMultisample

int gladLoadGLLoader(GLADloadproc load);

#ifdef __cplusplus
}
#endif

#endif /* GLAD_GLAD_H_ */
