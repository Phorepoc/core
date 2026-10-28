// Copyright The Phorepoc Authors
// SPDX-License-Identifier: Apache-2.0

// libgles_cm_front.cpp -- GENERATED (gen_gl_fronts.py) from opengles11u.def -- do not edit.
// libGLES_CM.dll: the frozen Symbian ABI front over phgl.dll (Mesa). Every function resolves its backend entry once (ordinal
// lookup in phgl.dll, else phgl's eglGetProcAddress) and calls it; without phgl.dll it returns 0 / does nothing.
#include <e32std.h>
#include <e32debug.h>
#include <GLES/gl.h>
#include <GLES/glext.h>
#include <EGL/egl.h>
#include "../phgl_front.h"

namespace
	{
	TLibraryFunction gFn[175];
	inline TLibraryFunction Fn(TInt aIdx, TInt aOrdinal, const char* aName)
		{
		return gFn[aIdx] ? gFn[aIdx] : (gFn[aIdx] = PhglResolve(aOrdinal, aName));
		}
	// egl* of a front that re-exports them (libGLES_CM): libEGL.dll's function at its libegl14U ordinal
	inline TLibraryFunction EglFn(TInt aIdx, TInt aOrdinal, const char* aName)
		{
		return gFn[aIdx] ? gFn[aIdx] : (gFn[aIdx] = PhglResolveEgl(aOrdinal, aName));
		}
	}

extern "C"
{

EGLBoolean eglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config)
	{
	typedef EGLBoolean (*F)(EGLDisplay, const EGLint *, EGLConfig *, EGLint, EGLint *);
	F f = (F)EglFn(0, 4, "eglChooseConfig");
	if (!f)
		return (EGLBoolean)0;
	return f(dpy, attrib_list, configs, config_size, num_config);
	}

EGLBoolean eglCopyBuffers(EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target)
	{
	typedef EGLBoolean (*F)(EGLDisplay, EGLSurface, EGLNativePixmapType);
	F f = (F)EglFn(1, 5, "eglCopyBuffers");
	if (!f)
		return (EGLBoolean)0;
	return f(dpy, surface, target);
	}

EGLContext eglCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list)
	{
	typedef EGLContext (*F)(EGLDisplay, EGLConfig, EGLContext, const EGLint *);
	F f = (F)EglFn(2, 6, "eglCreateContext");
	if (!f)
		return (EGLContext)0;
	return f(dpy, config, share_context, attrib_list);
	}

EGLSurface eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list)
	{
	typedef EGLSurface (*F)(EGLDisplay, EGLConfig, const EGLint *);
	F f = (F)EglFn(3, 8, "eglCreatePbufferSurface");
	if (!f)
		return (EGLSurface)0;
	return f(dpy, config, attrib_list);
	}

EGLSurface eglCreatePixmapSurface(EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint *attrib_list)
	{
	typedef EGLSurface (*F)(EGLDisplay, EGLConfig, EGLNativePixmapType, const EGLint *);
	F f = (F)EglFn(4, 9, "eglCreatePixmapSurface");
	if (!f)
		return (EGLSurface)0;
	return f(dpy, config, pixmap, attrib_list);
	}

EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list)
	{
	typedef EGLSurface (*F)(EGLDisplay, EGLConfig, EGLNativeWindowType, const EGLint *);
	F f = (F)EglFn(5, 10, "eglCreateWindowSurface");
	if (!f)
		return (EGLSurface)0;
	return f(dpy, config, win, attrib_list);
	}

EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx)
	{
	typedef EGLBoolean (*F)(EGLDisplay, EGLContext);
	F f = (F)EglFn(6, 11, "eglDestroyContext");
	if (!f)
		return (EGLBoolean)0;
	return f(dpy, ctx);
	}

EGLBoolean eglDestroySurface(EGLDisplay dpy, EGLSurface surface)
	{
	typedef EGLBoolean (*F)(EGLDisplay, EGLSurface);
	F f = (F)EglFn(7, 12, "eglDestroySurface");
	if (!f)
		return (EGLBoolean)0;
	return f(dpy, surface);
	}

EGLBoolean eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value)
	{
	typedef EGLBoolean (*F)(EGLDisplay, EGLConfig, EGLint, EGLint *);
	F f = (F)EglFn(8, 13, "eglGetConfigAttrib");
	if (!f)
		return (EGLBoolean)0;
	return f(dpy, config, attribute, value);
	}

EGLBoolean eglGetConfigs(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config)
	{
	typedef EGLBoolean (*F)(EGLDisplay, EGLConfig *, EGLint, EGLint *);
	F f = (F)EglFn(9, 14, "eglGetConfigs");
	if (!f)
		return (EGLBoolean)0;
	return f(dpy, configs, config_size, num_config);
	}

EGLContext eglGetCurrentContext(void)
	{
	typedef EGLContext (*F)(void);
	F f = (F)EglFn(10, 15, "eglGetCurrentContext");
	if (!f)
		return (EGLContext)0;
	return f();
	}

EGLDisplay eglGetCurrentDisplay(void)
	{
	typedef EGLDisplay (*F)(void);
	F f = (F)EglFn(11, 16, "eglGetCurrentDisplay");
	if (!f)
		return (EGLDisplay)0;
	return f();
	}

EGLSurface eglGetCurrentSurface(EGLint readdraw)
	{
	typedef EGLSurface (*F)(EGLint);
	F f = (F)EglFn(12, 17, "eglGetCurrentSurface");
	if (!f)
		return (EGLSurface)0;
	return f(readdraw);
	}

EGLDisplay eglGetDisplay(EGLNativeDisplayType display_id)
	{
	typedef EGLDisplay (*F)(EGLNativeDisplayType);
	F f = (F)EglFn(13, 18, "eglGetDisplay");
	if (!f)
		return (EGLDisplay)0;
	return f(display_id);
	}

EGLint eglGetError(void)
	{
	typedef EGLint (*F)(void);
	F f = (F)EglFn(14, 19, "eglGetError");
	if (!f)
		return (EGLint)0;
	return f();
	}

void (*eglGetProcAddress(const char* procname))(...)
	{
	typedef void (*(*F)(const char*))(...);
	F f = (F)EglFn(15, 20, "eglGetProcAddress");
	return f ? f(procname) : 0;
	}

EGLBoolean eglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor)
	{
	typedef EGLBoolean (*F)(EGLDisplay, EGLint *, EGLint *);
	F f = (F)EglFn(16, 21, "eglInitialize");
	if (!f)
		return (EGLBoolean)0;
	return f(dpy, major, minor);
	}

EGLBoolean eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx)
	{
	typedef EGLBoolean (*F)(EGLDisplay, EGLSurface, EGLSurface, EGLContext);
	F f = (F)EglFn(17, 22, "eglMakeCurrent");
	if (!f)
		return (EGLBoolean)0;
	return f(dpy, draw, read, ctx);
	}

EGLBoolean eglQueryContext(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value)
	{
	typedef EGLBoolean (*F)(EGLDisplay, EGLContext, EGLint, EGLint *);
	F f = (F)EglFn(18, 24, "eglQueryContext");
	if (!f)
		return (EGLBoolean)0;
	return f(dpy, ctx, attribute, value);
	}

const char * eglQueryString(EGLDisplay dpy, EGLint name)
	{
	typedef const char * (*F)(EGLDisplay, EGLint);
	F f = (F)EglFn(19, 25, "eglQueryString");
	if (!f)
		return (const char *)0;
	return f(dpy, name);
	}

EGLBoolean eglQuerySurface(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value)
	{
	typedef EGLBoolean (*F)(EGLDisplay, EGLSurface, EGLint, EGLint *);
	F f = (F)EglFn(20, 26, "eglQuerySurface");
	if (!f)
		return (EGLBoolean)0;
	EGLBoolean r = f(dpy, surface, attribute, value);
	static TInt traced;
	if (traced < 16 && value)
		{ ++traced; RDebug::Printf("PH: libGLES_CM.dll eglQuerySurface(0x%x) -> %d (r=%d)", (TInt)attribute, (TInt)*value, (TInt)r); }
	return r;
	}

EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface)
	{
	typedef EGLBoolean (*F)(EGLDisplay, EGLSurface);
	F f = (F)EglFn(21, 30, "eglSwapBuffers");
	if (!f)
		return (EGLBoolean)0;
	return f(dpy, surface);
	}

EGLBoolean eglTerminate(EGLDisplay dpy)
	{
	typedef EGLBoolean (*F)(EGLDisplay);
	F f = (F)EglFn(22, 32, "eglTerminate");
	if (!f)
		return (EGLBoolean)0;
	return f(dpy);
	}

EGLBoolean eglWaitGL(void)
	{
	typedef EGLBoolean (*F)(void);
	F f = (F)EglFn(23, 34, "eglWaitGL");
	if (!f)
		return (EGLBoolean)0;
	return f();
	}

EGLBoolean eglWaitNative(EGLint engine)
	{
	typedef EGLBoolean (*F)(EGLint);
	F f = (F)EglFn(24, 35, "eglWaitNative");
	if (!f)
		return (EGLBoolean)0;
	return f(engine);
	}

void glActiveTexture(GLenum texture)
	{
	typedef void (*F)(GLenum);
	F f = (F)Fn(25, 102, "glActiveTexture");
	if (!f)
		return;
	f(texture);
	}

void glAlphaFunc(GLenum func, GLclampf ref) __SOFTFP
	{
	typedef void (*F)(GLenum, GLclampf) __SOFTFP;
	F f = (F)Fn(26, 403, "glAlphaFunc");
	if (!f)
		return;
	f(func, ref);
	}

void glAlphaFuncx(GLenum func, GLclampx ref)
	{
	typedef void (*F)(GLenum, GLclampx);
	F f = (F)Fn(27, 404, "glAlphaFuncx");
	if (!f)
		return;
	f(func, ref);
	}

void glBindTexture(GLenum target, GLuint texture)
	{
	typedef void (*F)(GLenum, GLuint);
	F f = (F)Fn(28, 108, "glBindTexture");
	if (!f)
		return;
	f(target, texture);
	}

void glBlendFunc(GLenum sfactor, GLenum dfactor)
	{
	typedef void (*F)(GLenum, GLenum);
	F f = (F)Fn(29, 112, "glBlendFunc");
	if (!f)
		return;
	f(sfactor, dfactor);
	}

void glClear(GLbitfield mask)
	{
	typedef void (*F)(GLbitfield);
	F f = (F)Fn(30, 117, "glClear");
	if (!f)
		return;
	f(mask);
	}

void glClearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha) __SOFTFP
	{
	typedef void (*F)(GLclampf, GLclampf, GLclampf, GLclampf) __SOFTFP;
	F f = (F)Fn(31, 118, "glClearColor");
	if (!f)
		return;
	f(red, green, blue, alpha);
	}

void glClearColorx(GLclampx red, GLclampx green, GLclampx blue, GLclampx alpha)
	{
	typedef void (*F)(GLclampx, GLclampx, GLclampx, GLclampx);
	F f = (F)Fn(32, 412, "glClearColorx");
	if (!f)
		return;
	f(red, green, blue, alpha);
	}

void glClearDepthf(GLclampf depth) __SOFTFP
	{
	typedef void (*F)(GLclampf) __SOFTFP;
	F f = (F)Fn(33, 119, "glClearDepthf");
	if (!f)
		return;
	f(depth);
	}

void glClearDepthx(GLclampx depth)
	{
	typedef void (*F)(GLclampx);
	F f = (F)Fn(34, 414, "glClearDepthx");
	if (!f)
		return;
	f(depth);
	}

void glClearStencil(GLint s)
	{
	typedef void (*F)(GLint);
	F f = (F)Fn(35, 120, "glClearStencil");
	if (!f)
		return;
	f(s);
	}

void glClientActiveTexture(GLenum texture)
	{
	typedef void (*F)(GLenum);
	F f = (F)Fn(36, 416, "glClientActiveTexture");
	if (!f)
		return;
	f(texture);
	}

void glColor4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) __SOFTFP
	{
	typedef void (*F)(GLfloat, GLfloat, GLfloat, GLfloat) __SOFTFP;
	F f = (F)Fn(37, 419, "glColor4f");
	if (!f)
		return;
	f(red, green, blue, alpha);
	}

void glColor4x(GLfixed red, GLfixed green, GLfixed blue, GLfixed alpha)
	{
	typedef void (*F)(GLfixed, GLfixed, GLfixed, GLfixed);
	F f = (F)Fn(38, 421, "glColor4x");
	if (!f)
		return;
	f(red, green, blue, alpha);
	}

void glColorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
	{
	typedef void (*F)(GLboolean, GLboolean, GLboolean, GLboolean);
	F f = (F)Fn(39, 121, "glColorMask");
	if (!f)
		return;
	f(red, green, blue, alpha);
	}

void glColorPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
	{
	typedef void (*F)(GLint, GLenum, GLsizei, const GLvoid *);
	F f = (F)Fn(40, 423, "glColorPointer");
	if (!f)
		return;
	f(size, type, stride, pointer);
	}

void glCompressedTexImage2D(GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid *data)
	{
	typedef void (*F)(GLenum, GLint, GLenum, GLsizei, GLsizei, GLint, GLsizei, const GLvoid *);
	F f = (F)Fn(41, 123, "glCompressedTexImage2D");
	if (!f)
		return;
	f(target, level, internalformat, width, height, border, imageSize, data);
	}

void glCompressedTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid *data)
	{
	typedef void (*F)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLsizei, const GLvoid *);
	F f = (F)Fn(42, 124, "glCompressedTexSubImage2D");
	if (!f)
		return;
	f(target, level, xoffset, yoffset, width, height, format, imageSize, data);
	}

void glCopyTexImage2D(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
	{
	typedef void (*F)(GLenum, GLint, GLenum, GLint, GLint, GLsizei, GLsizei, GLint);
	F f = (F)Fn(43, 125, "glCopyTexImage2D");
	if (!f)
		return;
	f(target, level, internalformat, x, y, width, height, border);
	}

void glCopyTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
	{
	typedef void (*F)(GLenum, GLint, GLint, GLint, GLint, GLint, GLsizei, GLsizei);
	F f = (F)Fn(44, 126, "glCopyTexSubImage2D");
	if (!f)
		return;
	f(target, level, xoffset, yoffset, x, y, width, height);
	}

void glCullFace(GLenum mode)
	{
	typedef void (*F)(GLenum);
	F f = (F)Fn(45, 129, "glCullFace");
	if (!f)
		return;
	f(mode);
	}

void glDeleteTextures(GLsizei n, const GLuint *textures)
	{
	typedef void (*F)(GLsizei, const GLuint *);
	F f = (F)Fn(46, 135, "glDeleteTextures");
	if (!f)
		return;
	f(n, textures);
	}

void glDepthFunc(GLenum func)
	{
	typedef void (*F)(GLenum);
	F f = (F)Fn(47, 136, "glDepthFunc");
	if (!f)
		return;
	f(func);
	}

void glDepthMask(GLboolean flag)
	{
	typedef void (*F)(GLboolean);
	F f = (F)Fn(48, 137, "glDepthMask");
	if (!f)
		return;
	f(flag);
	}

void glDepthRangef(GLclampf zNear, GLclampf zFar) __SOFTFP
	{
	typedef void (*F)(GLclampf, GLclampf) __SOFTFP;
	F f = (F)Fn(49, 138, "glDepthRangef");
	if (!f)
		return;
	f(zNear, zFar);
	}

void glDepthRangex(GLclampx zNear, GLclampx zFar)
	{
	typedef void (*F)(GLclampx, GLclampx);
	F f = (F)Fn(50, 435, "glDepthRangex");
	if (!f)
		return;
	f(zNear, zFar);
	}

void glDisable(GLenum cap)
	{
	typedef void (*F)(GLenum);
	F f = (F)Fn(51, 140, "glDisable");
	if (!f)
		return;
	f(cap);
	}

void glDisableClientState(GLenum array)
	{
	typedef void (*F)(GLenum);
	F f = (F)Fn(52, 437, "glDisableClientState");
	if (!f)
		return;
	f(array);
	}

void glDrawArrays(GLenum mode, GLint first, GLsizei count)
	{
	typedef void (*F)(GLenum, GLint, GLsizei);
	F f = (F)Fn(53, 142, "glDrawArrays");
	if (!f)
		return;
	f(mode, first, count);
	}

void glDrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
	{
	typedef void (*F)(GLenum, GLsizei, GLenum, const GLvoid *);
	F f = (F)Fn(54, 143, "glDrawElements");
	if (!f)
		return;
	f(mode, count, type, indices);
	}

void glEnable(GLenum cap)
	{
	typedef void (*F)(GLenum);
	F f = (F)Fn(55, 144, "glEnable");
	if (!f)
		return;
	f(cap);
	}

void glEnableClientState(GLenum array)
	{
	typedef void (*F)(GLenum);
	F f = (F)Fn(56, 449, "glEnableClientState");
	if (!f)
		return;
	f(array);
	}

void glFinish(void)
	{
	typedef void (*F)(void);
	F f = (F)Fn(57, 146, "glFinish");
	if (!f)
		return;
	f();
	}

void glFlush(void)
	{
	typedef void (*F)(void);
	F f = (F)Fn(58, 147, "glFlush");
	if (!f)
		return;
	f();
	}

void glFogf(GLenum pname, GLfloat param) __SOFTFP
	{
	typedef void (*F)(GLenum, GLfloat) __SOFTFP;
	F f = (F)Fn(59, 452, "glFogf");
	if (!f)
		return;
	f(pname, param);
	}

void glFogfv(GLenum pname, const GLfloat *params)
	{
	typedef void (*F)(GLenum, const GLfloat *);
	F f = (F)Fn(60, 453, "glFogfv");
	if (!f)
		return;
	f(pname, params);
	}

void glFogx(GLenum pname, GLfixed param)
	{
	typedef void (*F)(GLenum, GLfixed);
	F f = (F)Fn(61, 454, "glFogx");
	if (!f)
		return;
	f(pname, param);
	}

void glFogxv(GLenum pname, const GLfixed *params)
	{
	typedef void (*F)(GLenum, const GLfixed *);
	F f = (F)Fn(62, 455, "glFogxv");
	if (!f)
		return;
	f(pname, params);
	}

void glFrontFace(GLenum mode)
	{
	typedef void (*F)(GLenum);
	F f = (F)Fn(63, 150, "glFrontFace");
	if (!f)
		return;
	f(mode);
	}

void glFrustumf(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat zNear, GLfloat zFar) __SOFTFP
	{
	typedef void (*F)(GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat) __SOFTFP;
	F f = (F)Fn(64, 457, "glFrustumf");
	if (!f)
		return;
	f(left, right, bottom, top, zNear, zFar);
	}

void glFrustumx(GLfixed left, GLfixed right, GLfixed bottom, GLfixed top, GLfixed zNear, GLfixed zFar)
	{
	typedef void (*F)(GLfixed, GLfixed, GLfixed, GLfixed, GLfixed, GLfixed);
	F f = (F)Fn(65, 458, "glFrustumx");
	if (!f)
		return;
	f(left, right, bottom, top, zNear, zFar);
	}

void glGenTextures(GLsizei n, GLuint *textures)
	{
	typedef void (*F)(GLsizei, GLuint *);
	F f = (F)Fn(66, 154, "glGenTextures");
	if (!f)
		return;
	f(n, textures);
	}

GLenum glGetError(void)
	{
	typedef GLenum (*F)(void);
	F f = (F)Fn(67, 162, "glGetError");
	if (!f)
		return (GLenum)0;
	return f();
	}

void glGetIntegerv(GLenum pname, GLint *params)
	{
	typedef void (*F)(GLenum, GLint *);
	F f = (F)Fn(68, 165, "glGetIntegerv");
	if (!f)
		return;
	f(pname, params);
	}

const GLubyte * glGetString(GLenum name)
	{
	typedef const GLubyte * (*F)(GLenum);
	F f = (F)Fn(69, 173, "glGetString");
	if (!f)
		return (const GLubyte *)0;
	return f(name);
	}

void glHint(GLenum target, GLenum mode)
	{
	typedef void (*F)(GLenum, GLenum);
	F f = (F)Fn(70, 182, "glHint");
	if (!f)
		return;
	f(target, mode);
	}

void glLightModelf(GLenum pname, GLfloat param) __SOFTFP
	{
	typedef void (*F)(GLenum, GLfloat) __SOFTFP;
	F f = (F)Fn(71, 487, "glLightModelf");
	if (!f)
		return;
	f(pname, param);
	}

void glLightModelfv(GLenum pname, const GLfloat *params)
	{
	typedef void (*F)(GLenum, const GLfloat *);
	F f = (F)Fn(72, 488, "glLightModelfv");
	if (!f)
		return;
	f(pname, params);
	}

void glLightModelx(GLenum pname, GLfixed param)
	{
	typedef void (*F)(GLenum, GLfixed);
	F f = (F)Fn(73, 489, "glLightModelx");
	if (!f)
		return;
	f(pname, param);
	}

void glLightModelxv(GLenum pname, const GLfixed *params)
	{
	typedef void (*F)(GLenum, const GLfixed *);
	F f = (F)Fn(74, 490, "glLightModelxv");
	if (!f)
		return;
	f(pname, params);
	}

void glLightf(GLenum light, GLenum pname, GLfloat param) __SOFTFP
	{
	typedef void (*F)(GLenum, GLenum, GLfloat) __SOFTFP;
	F f = (F)Fn(75, 485, "glLightf");
	if (!f)
		return;
	f(light, pname, param);
	}

void glLightfv(GLenum light, GLenum pname, const GLfloat *params)
	{
	typedef void (*F)(GLenum, GLenum, const GLfloat *);
	F f = (F)Fn(76, 486, "glLightfv");
	if (!f)
		return;
	f(light, pname, params);
	}

void glLightx(GLenum light, GLenum pname, GLfixed param)
	{
	typedef void (*F)(GLenum, GLenum, GLfixed);
	F f = (F)Fn(77, 491, "glLightx");
	if (!f)
		return;
	f(light, pname, param);
	}

void glLightxv(GLenum light, GLenum pname, const GLfixed *params)
	{
	typedef void (*F)(GLenum, GLenum, const GLfixed *);
	F f = (F)Fn(78, 492, "glLightxv");
	if (!f)
		return;
	f(light, pname, params);
	}

void glLineWidth(GLfloat width) __SOFTFP
	{
	typedef void (*F)(GLfloat) __SOFTFP;
	F f = (F)Fn(79, 190, "glLineWidth");
	if (!f)
		return;
	f(width);
	}

void glLineWidthx(GLfixed width)
	{
	typedef void (*F)(GLfixed);
	F f = (F)Fn(80, 494, "glLineWidthx");
	if (!f)
		return;
	f(width);
	}

void glLoadIdentity(void)
	{
	typedef void (*F)(void);
	F f = (F)Fn(81, 495, "glLoadIdentity");
	if (!f)
		return;
	f();
	}

void glLoadMatrixf(const GLfloat *m)
	{
	typedef void (*F)(const GLfloat *);
	F f = (F)Fn(82, 496, "glLoadMatrixf");
	if (!f)
		return;
	f(m);
	}

void glLoadMatrixx(const GLfixed *m)
	{
	typedef void (*F)(const GLfixed *);
	F f = (F)Fn(83, 497, "glLoadMatrixx");
	if (!f)
		return;
	f(m);
	}

void glLogicOp(GLenum opcode)
	{
	typedef void (*F)(GLenum);
	F f = (F)Fn(84, 499, "glLogicOp");
	if (!f)
		return;
	f(opcode);
	}

void glMaterialf(GLenum face, GLenum pname, GLfloat param) __SOFTFP
	{
	typedef void (*F)(GLenum, GLenum, GLfloat) __SOFTFP;
	F f = (F)Fn(85, 500, "glMaterialf");
	if (!f)
		return;
	f(face, pname, param);
	}

void glMaterialfv(GLenum face, GLenum pname, const GLfloat *params)
	{
	typedef void (*F)(GLenum, GLenum, const GLfloat *);
	F f = (F)Fn(86, 501, "glMaterialfv");
	if (!f)
		return;
	f(face, pname, params);
	}

void glMaterialx(GLenum face, GLenum pname, GLfixed param)
	{
	typedef void (*F)(GLenum, GLenum, GLfixed);
	F f = (F)Fn(87, 502, "glMaterialx");
	if (!f)
		return;
	f(face, pname, param);
	}

void glMaterialxv(GLenum face, GLenum pname, const GLfixed *params)
	{
	typedef void (*F)(GLenum, GLenum, const GLfixed *);
	F f = (F)Fn(88, 503, "glMaterialxv");
	if (!f)
		return;
	f(face, pname, params);
	}

void glMatrixMode(GLenum mode)
	{
	typedef void (*F)(GLenum);
	F f = (F)Fn(89, 505, "glMatrixMode");
	if (!f)
		return;
	f(mode);
	}

void glMultMatrixf(const GLfloat *m)
	{
	typedef void (*F)(const GLfloat *);
	F f = (F)Fn(90, 508, "glMultMatrixf");
	if (!f)
		return;
	f(m);
	}

void glMultMatrixx(const GLfixed *m)
	{
	typedef void (*F)(const GLfixed *);
	F f = (F)Fn(91, 509, "glMultMatrixx");
	if (!f)
		return;
	f(m);
	}

void glMultiTexCoord4f(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q) __SOFTFP
	{
	typedef void (*F)(GLenum, GLfloat, GLfloat, GLfloat, GLfloat) __SOFTFP;
	F f = (F)Fn(92, 506, "glMultiTexCoord4f");
	if (!f)
		return;
	f(target, s, t, r, q);
	}

void glMultiTexCoord4x(GLenum target, GLfixed s, GLfixed t, GLfixed r, GLfixed q)
	{
	typedef void (*F)(GLenum, GLfixed, GLfixed, GLfixed, GLfixed);
	F f = (F)Fn(93, 507, "glMultiTexCoord4x");
	if (!f)
		return;
	f(target, s, t, r, q);
	}

void glNormal3f(GLfloat nx, GLfloat ny, GLfloat nz) __SOFTFP
	{
	typedef void (*F)(GLfloat, GLfloat, GLfloat) __SOFTFP;
	F f = (F)Fn(94, 510, "glNormal3f");
	if (!f)
		return;
	f(nx, ny, nz);
	}

void glNormal3x(GLfixed nx, GLfixed ny, GLfixed nz)
	{
	typedef void (*F)(GLfixed, GLfixed, GLfixed);
	F f = (F)Fn(95, 511, "glNormal3x");
	if (!f)
		return;
	f(nx, ny, nz);
	}

void glNormalPointer(GLenum type, GLsizei stride, const GLvoid *pointer)
	{
	typedef void (*F)(GLenum, GLsizei, const GLvoid *);
	F f = (F)Fn(96, 512, "glNormalPointer");
	if (!f)
		return;
	f(type, stride, pointer);
	}

void glOrthof(GLfloat left, GLfloat right, GLfloat bottom, GLfloat top, GLfloat zNear, GLfloat zFar) __SOFTFP
	{
	typedef void (*F)(GLfloat, GLfloat, GLfloat, GLfloat, GLfloat, GLfloat) __SOFTFP;
	F f = (F)Fn(97, 513, "glOrthof");
	if (!f)
		return;
	f(left, right, bottom, top, zNear, zFar);
	}

void glOrthox(GLfixed left, GLfixed right, GLfixed bottom, GLfixed top, GLfixed zNear, GLfixed zFar)
	{
	typedef void (*F)(GLfixed, GLfixed, GLfixed, GLfixed, GLfixed, GLfixed);
	F f = (F)Fn(98, 514, "glOrthox");
	if (!f)
		return;
	f(left, right, bottom, top, zNear, zFar);
	}

void glPixelStorei(GLenum pname, GLint param)
	{
	typedef void (*F)(GLenum, GLint);
	F f = (F)Fn(99, 192, "glPixelStorei");
	if (!f)
		return;
	f(pname, param);
	}

void glPointSize(GLfloat size) __SOFTFP
	{
	typedef void (*F)(GLfloat) __SOFTFP;
	F f = (F)Fn(100, 520, "glPointSize");
	if (!f)
		return;
	f(size);
	}

void glPointSizex(GLfixed size)
	{
	typedef void (*F)(GLfixed);
	F f = (F)Fn(101, 522, "glPointSizex");
	if (!f)
		return;
	f(size);
	}

void glPolygonOffset(GLfloat factor, GLfloat units) __SOFTFP
	{
	typedef void (*F)(GLfloat, GLfloat) __SOFTFP;
	F f = (F)Fn(102, 193, "glPolygonOffset");
	if (!f)
		return;
	f(factor, units);
	}

void glPolygonOffsetx(GLfixed factor, GLfixed units)
	{
	typedef void (*F)(GLfixed, GLfixed);
	F f = (F)Fn(103, 524, "glPolygonOffsetx");
	if (!f)
		return;
	f(factor, units);
	}

void glPopMatrix(void)
	{
	typedef void (*F)(void);
	F f = (F)Fn(104, 525, "glPopMatrix");
	if (!f)
		return;
	f();
	}

void glPushMatrix(void)
	{
	typedef void (*F)(void);
	F f = (F)Fn(105, 526, "glPushMatrix");
	if (!f)
		return;
	f();
	}

GLbitfield glQueryMatrixxOES(GLfixed mantissa[16], GLint exponent[16])
	{
	typedef GLbitfield (*F)(GLfixed [16], GLint [16]);
	F f = (F)Fn(106, 0, "glQueryMatrixxOES");
	if (!f)
		return (GLbitfield)0;
	return f(mantissa, exponent);
	}

void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid *pixels)
	{
	typedef void (*F)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, GLvoid *);
	F f = (F)Fn(107, 194, "glReadPixels");
	if (!f)
		return;
	f(x, y, width, height, format, type, pixels);
	}

void glRotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z) __SOFTFP
	{
	typedef void (*F)(GLfloat, GLfloat, GLfloat, GLfloat) __SOFTFP;
	F f = (F)Fn(108, 528, "glRotatef");
	if (!f)
		return;
	f(angle, x, y, z);
	}

void glRotatex(GLfixed angle, GLfixed x, GLfixed y, GLfixed z)
	{
	typedef void (*F)(GLfixed, GLfixed, GLfixed, GLfixed);
	F f = (F)Fn(109, 529, "glRotatex");
	if (!f)
		return;
	f(angle, x, y, z);
	}

void glSampleCoverage(GLclampf value, GLboolean invert) __SOFTFP
	{
	typedef void (*F)(GLclampf, GLboolean) __SOFTFP;
	F f = (F)Fn(110, 197, "glSampleCoverage");
	if (!f)
		return;
	f(value, invert);
	}

void glSampleCoveragex(GLclampx value, GLboolean invert)
	{
	typedef void (*F)(GLclampx, GLboolean);
	F f = (F)Fn(111, 531, "glSampleCoveragex");
	if (!f)
		return;
	f(value, invert);
	}

void glScalef(GLfloat x, GLfloat y, GLfloat z) __SOFTFP
	{
	typedef void (*F)(GLfloat, GLfloat, GLfloat) __SOFTFP;
	F f = (F)Fn(112, 532, "glScalef");
	if (!f)
		return;
	f(x, y, z);
	}

void glScalex(GLfixed x, GLfixed y, GLfixed z)
	{
	typedef void (*F)(GLfixed, GLfixed, GLfixed);
	F f = (F)Fn(113, 533, "glScalex");
	if (!f)
		return;
	f(x, y, z);
	}

void glScissor(GLint x, GLint y, GLsizei width, GLsizei height)
	{
	static TInt traced;
	if (traced < 16)
		{ ++traced; RDebug::Printf("PH: libGLES_CM.dll glScissor(%d,%d,%d,%d)", (TInt)x, (TInt)y, (TInt)width, (TInt)height); }
	typedef void (*F)(GLint, GLint, GLsizei, GLsizei);
	F f = (F)Fn(114, 198, "glScissor");
	if (!f)
		return;
	f(x, y, width, height);
	}

void glShadeModel(GLenum mode)
	{
	typedef void (*F)(GLenum);
	F f = (F)Fn(115, 535, "glShadeModel");
	if (!f)
		return;
	f(mode);
	}

void glStencilFunc(GLenum func, GLint ref, GLuint mask)
	{
	typedef void (*F)(GLenum, GLint, GLuint);
	F f = (F)Fn(116, 201, "glStencilFunc");
	if (!f)
		return;
	f(func, ref, mask);
	}

void glStencilMask(GLuint mask)
	{
	typedef void (*F)(GLuint);
	F f = (F)Fn(117, 203, "glStencilMask");
	if (!f)
		return;
	f(mask);
	}

void glStencilOp(GLenum fail, GLenum zfail, GLenum zpass)
	{
	typedef void (*F)(GLenum, GLenum, GLenum);
	F f = (F)Fn(118, 205, "glStencilOp");
	if (!f)
		return;
	f(fail, zfail, zpass);
	}

void glTexCoordPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
	{
	typedef void (*F)(GLint, GLenum, GLsizei, const GLvoid *);
	F f = (F)Fn(119, 539, "glTexCoordPointer");
	if (!f)
		return;
	f(size, type, stride, pointer);
	}

void glTexEnvf(GLenum target, GLenum pname, GLfloat param) __SOFTFP
	{
	typedef void (*F)(GLenum, GLenum, GLfloat) __SOFTFP;
	F f = (F)Fn(120, 540, "glTexEnvf");
	if (!f)
		return;
	f(target, pname, param);
	}

void glTexEnvfv(GLenum target, GLenum pname, const GLfloat *params)
	{
	typedef void (*F)(GLenum, GLenum, const GLfloat *);
	F f = (F)Fn(121, 541, "glTexEnvfv");
	if (!f)
		return;
	f(target, pname, params);
	}

void glTexEnvx(GLenum target, GLenum pname, GLfixed param)
	{
	typedef void (*F)(GLenum, GLenum, GLfixed);
	F f = (F)Fn(122, 544, "glTexEnvx");
	if (!f)
		return;
	f(target, pname, param);
	}

void glTexEnvxv(GLenum target, GLenum pname, const GLfixed *params)
	{
	typedef void (*F)(GLenum, GLenum, const GLfixed *);
	F f = (F)Fn(123, 545, "glTexEnvxv");
	if (!f)
		return;
	f(target, pname, params);
	}

void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
	{
	typedef void (*F)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid *);
	F f = (F)Fn(124, 207, "glTexImage2D");
	if (!f)
		return;
	f(target, level, internalformat, width, height, border, format, type, pixels);
	}

void glTexParameterf(GLenum target, GLenum pname, GLfloat param) __SOFTFP
	{
	typedef void (*F)(GLenum, GLenum, GLfloat) __SOFTFP;
	F f = (F)Fn(125, 208, "glTexParameterf");
	if (!f)
		return;
	f(target, pname, param);
	}

void glTexParameterx(GLenum target, GLenum pname, GLfixed param)
	{
	typedef void (*F)(GLenum, GLenum, GLfixed);
	F f = (F)Fn(126, 551, "glTexParameterx");
	if (!f)
		return;
	f(target, pname, param);
	}

void glTexSubImage2D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid *pixels)
	{
	typedef void (*F)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const GLvoid *);
	F f = (F)Fn(127, 212, "glTexSubImage2D");
	if (!f)
		return;
	f(target, level, xoffset, yoffset, width, height, format, type, pixels);
	}

void glTranslatef(GLfloat x, GLfloat y, GLfloat z) __SOFTFP
	{
	typedef void (*F)(GLfloat, GLfloat, GLfloat) __SOFTFP;
	F f = (F)Fn(128, 554, "glTranslatef");
	if (!f)
		return;
	f(x, y, z);
	}

void glTranslatex(GLfixed x, GLfixed y, GLfixed z)
	{
	typedef void (*F)(GLfixed, GLfixed, GLfixed);
	F f = (F)Fn(129, 555, "glTranslatex");
	if (!f)
		return;
	f(x, y, z);
	}

void glVertexPointer(GLint size, GLenum type, GLsizei stride, const GLvoid *pointer)
	{
	typedef void (*F)(GLint, GLenum, GLsizei, const GLvoid *);
	F f = (F)Fn(130, 556, "glVertexPointer");
	if (!f)
		return;
	f(size, type, stride, pointer);
	}

void glViewport(GLint x, GLint y, GLsizei width, GLsizei height)
	{
	static TInt traced;
	if (traced < 16)
		{ ++traced; RDebug::Printf("PH: libGLES_CM.dll glViewport(%d,%d,%d,%d)", (TInt)x, (TInt)y, (TInt)width, (TInt)height); }
	typedef void (*F)(GLint, GLint, GLsizei, GLsizei);
	F f = (F)Fn(131, 243, "glViewport");
	if (!f)
		return;
	f(x, y, width, height);
	}

EGLBoolean eglSwapInterval(EGLDisplay dpy, EGLint interval)
	{
	typedef EGLBoolean (*F)(EGLDisplay, EGLint);
	F f = (F)EglFn(132, 31, "eglSwapInterval");
	if (!f)
		return (EGLBoolean)0;
	return f(dpy, interval);
	}

void glBindBuffer(GLenum target, GLuint buffer)
	{
	typedef void (*F)(GLenum, GLuint);
	F f = (F)Fn(133, 105, "glBindBuffer");
	if (!f)
		return;
	f(target, buffer);
	}

void glBufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage)
	{
	typedef void (*F)(GLenum, GLsizeiptr, const GLvoid *, GLenum);
	F f = (F)Fn(134, 114, "glBufferData");
	if (!f)
		return;
	f(target, size, data, usage);
	}

void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid *data)
	{
	typedef void (*F)(GLenum, GLintptr, GLsizeiptr, const GLvoid *);
	F f = (F)Fn(135, 115, "glBufferSubData");
	if (!f)
		return;
	f(target, offset, size, data);
	}

void glClipPlanef(GLenum plane, const GLfloat *equation)
	{
	typedef void (*F)(GLenum, const GLfloat *);
	F f = (F)Fn(136, 417, "glClipPlanef");
	if (!f)
		return;
	f(plane, equation);
	}

void glClipPlanex(GLenum plane, const GLfixed *equation)
	{
	typedef void (*F)(GLenum, const GLfixed *);
	F f = (F)Fn(137, 418, "glClipPlanex");
	if (!f)
		return;
	f(plane, equation);
	}

void glColor4ub(GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
	{
	typedef void (*F)(GLubyte, GLubyte, GLubyte, GLubyte);
	F f = (F)Fn(138, 420, "glColor4ub");
	if (!f)
		return;
	f(red, green, blue, alpha);
	}

void glDeleteBuffers(GLsizei n, const GLuint *buffers)
	{
	typedef void (*F)(GLsizei, const GLuint *);
	F f = (F)Fn(139, 130, "glDeleteBuffers");
	if (!f)
		return;
	f(n, buffers);
	}

void glGenBuffers(GLsizei n, GLuint *buffers)
	{
	typedef void (*F)(GLsizei, GLuint *);
	F f = (F)Fn(140, 151, "glGenBuffers");
	if (!f)
		return;
	f(n, buffers);
	}

void glGetBooleanv(GLenum pname, GLboolean *params)
	{
	typedef void (*F)(GLenum, GLboolean *);
	F f = (F)Fn(141, 160, "glGetBooleanv");
	if (!f)
		return;
	f(pname, params);
	}

void glGetBufferParameteriv(GLenum target, GLenum pname, GLint *params)
	{
	typedef void (*F)(GLenum, GLenum, GLint *);
	F f = (F)Fn(142, 161, "glGetBufferParameteriv");
	if (!f)
		return;
	f(target, pname, params);
	}

void glGetClipPlanef(GLenum pname, GLfloat eqn[4])
	{
	typedef void (*F)(GLenum, GLfloat [4]);
	F f = (F)Fn(143, 463, "glGetClipPlanef");
	if (!f)
		return;
	f(pname, eqn);
	}

void glGetClipPlanex(GLenum pname, GLfixed eqn[4])
	{
	typedef void (*F)(GLenum, GLfixed [4]);
	F f = (F)Fn(144, 464, "glGetClipPlanex");
	if (!f)
		return;
	f(pname, eqn);
	}

void glGetFixedv(GLenum pname, GLfixed *params)
	{
	typedef void (*F)(GLenum, GLfixed *);
	F f = (F)Fn(145, 466, "glGetFixedv");
	if (!f)
		return;
	f(pname, params);
	}

void glGetFloatv(GLenum pname, GLfloat *params)
	{
	typedef void (*F)(GLenum, GLfloat *);
	F f = (F)Fn(146, 163, "glGetFloatv");
	if (!f)
		return;
	f(pname, params);
	}

void glGetLightfv(GLenum light, GLenum pname, GLfloat *params)
	{
	typedef void (*F)(GLenum, GLenum, GLfloat *);
	F f = (F)Fn(147, 469, "glGetLightfv");
	if (!f)
		return;
	f(light, pname, params);
	}

void glGetLightxv(GLenum light, GLenum pname, GLfixed *params)
	{
	typedef void (*F)(GLenum, GLenum, GLfixed *);
	F f = (F)Fn(148, 470, "glGetLightxv");
	if (!f)
		return;
	f(light, pname, params);
	}

void glGetMaterialfv(GLenum face, GLenum pname, GLfloat *params)
	{
	typedef void (*F)(GLenum, GLenum, GLfloat *);
	F f = (F)Fn(149, 471, "glGetMaterialfv");
	if (!f)
		return;
	f(face, pname, params);
	}

void glGetMaterialxv(GLenum face, GLenum pname, GLfixed *params)
	{
	typedef void (*F)(GLenum, GLenum, GLfixed *);
	F f = (F)Fn(150, 472, "glGetMaterialxv");
	if (!f)
		return;
	f(face, pname, params);
	}

void glGetPointerv(GLenum pname, void **params)
	{
	typedef void (*F)(GLenum, void **);
	F f = (F)Fn(151, 473, "glGetPointerv");
	if (!f)
		return;
	f(pname, params);
	}

void glGetTexEnvfv(GLenum env, GLenum pname, GLfloat *params)
	{
	typedef void (*F)(GLenum, GLenum, GLfloat *);
	F f = (F)Fn(152, 475, "glGetTexEnvfv");
	if (!f)
		return;
	f(env, pname, params);
	}

void glGetTexEnviv(GLenum env, GLenum pname, GLint *params)
	{
	typedef void (*F)(GLenum, GLenum, GLint *);
	F f = (F)Fn(153, 476, "glGetTexEnviv");
	if (!f)
		return;
	f(env, pname, params);
	}

void glGetTexEnvxv(GLenum env, GLenum pname, GLfixed *params)
	{
	typedef void (*F)(GLenum, GLenum, GLfixed *);
	F f = (F)Fn(154, 477, "glGetTexEnvxv");
	if (!f)
		return;
	f(env, pname, params);
	}

void glGetTexParameterfv(GLenum target, GLenum pname, GLfloat *params)
	{
	typedef void (*F)(GLenum, GLenum, GLfloat *);
	F f = (F)Fn(155, 174, "glGetTexParameterfv");
	if (!f)
		return;
	f(target, pname, params);
	}

void glGetTexParameteriv(GLenum target, GLenum pname, GLint *params)
	{
	typedef void (*F)(GLenum, GLenum, GLint *);
	F f = (F)Fn(156, 175, "glGetTexParameteriv");
	if (!f)
		return;
	f(target, pname, params);
	}

void glGetTexParameterxv(GLenum target, GLenum pname, GLfixed *params)
	{
	typedef void (*F)(GLenum, GLenum, GLfixed *);
	F f = (F)Fn(157, 480, "glGetTexParameterxv");
	if (!f)
		return;
	f(target, pname, params);
	}

GLboolean glIsBuffer(GLuint buffer)
	{
	typedef GLboolean (*F)(GLuint);
	F f = (F)Fn(158, 183, "glIsBuffer");
	if (!f)
		return (GLboolean)0;
	return f(buffer);
	}

GLboolean glIsEnabled(GLenum cap)
	{
	typedef GLboolean (*F)(GLenum);
	F f = (F)Fn(159, 184, "glIsEnabled");
	if (!f)
		return (GLboolean)0;
	return f(cap);
	}

GLboolean glIsTexture(GLuint texture)
	{
	typedef GLboolean (*F)(GLuint);
	F f = (F)Fn(160, 189, "glIsTexture");
	if (!f)
		return (GLboolean)0;
	return f(texture);
	}

void glPointParameterf(GLenum pname, GLfloat param) __SOFTFP
	{
	typedef void (*F)(GLenum, GLfloat) __SOFTFP;
	F f = (F)Fn(161, 516, "glPointParameterf");
	if (!f)
		return;
	f(pname, param);
	}

void glPointParameterfv(GLenum pname, const GLfloat *params)
	{
	typedef void (*F)(GLenum, const GLfloat *);
	F f = (F)Fn(162, 517, "glPointParameterfv");
	if (!f)
		return;
	f(pname, params);
	}

void glPointParameterx(GLenum pname, GLfixed param)
	{
	typedef void (*F)(GLenum, GLfixed);
	F f = (F)Fn(163, 518, "glPointParameterx");
	if (!f)
		return;
	f(pname, param);
	}

void glPointParameterxv(GLenum pname, const GLfixed *params)
	{
	typedef void (*F)(GLenum, const GLfixed *);
	F f = (F)Fn(164, 519, "glPointParameterxv");
	if (!f)
		return;
	f(pname, params);
	}

void glPointSizePointerOES(GLenum type, GLsizei stride, const GLvoid *pointer)
	{
	typedef void (*F)(GLenum, GLsizei, const GLvoid *);
	F f = (F)Fn(165, 521, "glPointSizePointerOES");
	if (!f)
		return;
	f(type, stride, pointer);
	}

void glTexEnvi(GLenum target, GLenum pname, GLint param)
	{
	typedef void (*F)(GLenum, GLenum, GLint);
	F f = (F)Fn(166, 542, "glTexEnvi");
	if (!f)
		return;
	f(target, pname, param);
	}

void glTexEnviv(GLenum target, GLenum pname, const GLint *params)
	{
	typedef void (*F)(GLenum, GLenum, const GLint *);
	F f = (F)Fn(167, 543, "glTexEnviv");
	if (!f)
		return;
	f(target, pname, params);
	}

void glTexParameterfv(GLenum target, GLenum pname, const GLfloat *params)
	{
	typedef void (*F)(GLenum, GLenum, const GLfloat *);
	F f = (F)Fn(168, 209, "glTexParameterfv");
	if (!f)
		return;
	f(target, pname, params);
	}

void glTexParameteri(GLenum target, GLenum pname, GLint param)
	{
	typedef void (*F)(GLenum, GLenum, GLint);
	F f = (F)Fn(169, 210, "glTexParameteri");
	if (!f)
		return;
	f(target, pname, param);
	}

void glTexParameteriv(GLenum target, GLenum pname, const GLint *params)
	{
	typedef void (*F)(GLenum, GLenum, const GLint *);
	F f = (F)Fn(170, 211, "glTexParameteriv");
	if (!f)
		return;
	f(target, pname, params);
	}

void glTexParameterxv(GLenum target, GLenum pname, const GLfixed *params)
	{
	typedef void (*F)(GLenum, GLenum, const GLfixed *);
	F f = (F)Fn(171, 552, "glTexParameterxv");
	if (!f)
		return;
	f(target, pname, params);
	}

EGLBoolean eglBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
	{
	typedef EGLBoolean (*F)(EGLDisplay, EGLSurface, EGLint);
	F f = (F)EglFn(172, 3, "eglBindTexImage");
	if (!f)
		return (EGLBoolean)0;
	return f(dpy, surface, buffer);
	}

EGLBoolean eglReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
	{
	typedef EGLBoolean (*F)(EGLDisplay, EGLSurface, EGLint);
	F f = (F)EglFn(173, 27, "eglReleaseTexImage");
	if (!f)
		return (EGLBoolean)0;
	return f(dpy, surface, buffer);
	}

EGLBoolean eglSurfaceAttrib(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value)
	{
	typedef EGLBoolean (*F)(EGLDisplay, EGLSurface, EGLint, EGLint);
	F f = (F)EglFn(174, 29, "eglSurfaceAttrib");
	if (!f)
		return (EGLBoolean)0;
	return f(dpy, surface, attribute, value);
	}

}
