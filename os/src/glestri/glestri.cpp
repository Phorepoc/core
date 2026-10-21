// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// glestri.cpp -- OURS (2026-10-09, GPU plan G2): the first OpenGL ES 2.0 program on our Symbian -- Mesa
// (softpipe, no GPU) behind EGL, statically linked (emitter/components.py `glestri`). A full-screen RWindow, a
// rotating colour triangle over a slowly cycling background, "[glestri] N frames in T ms" every ~2 s in the log.
// Proves: EGL display/config/context/window surface, the GLSL compiler, the GLES2 draw path and eglSwapBuffers.
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <w32std.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <math.h>

static const char KVs[] =
	"attribute vec2 aPos;\n"
	"attribute vec3 aCol;\n"
	"uniform float uAngle;\n"
	"uniform vec2 uAspect;\n"
	"varying vec3 vCol;\n"
	"void main() {\n"
	"  float c = cos(uAngle), s = sin(uAngle);\n"
	"  vec2 p = vec2(c * aPos.x - s * aPos.y, s * aPos.x + c * aPos.y);\n"
	"  gl_Position = vec4(p * uAspect, 0.0, 1.0);\n"
	"  vCol = aCol;\n"
	"}\n";
static const char KFs[] =
	"precision mediump float;\n"
	"varying vec3 vCol;\n"
	"void main() { gl_FragColor = vec4(vCol, 1.0); }\n";

static GLuint Compile(GLenum aType, const char* aSrc)
	{
	GLuint s = glCreateShader(aType);
	glShaderSource(s, 1, &aSrc, NULL);
	glCompileShader(s);
	GLint ok = 0;
	glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok)
		{
		char log[256];
		glGetShaderInfoLog(s, sizeof(log), NULL, log);
		RDebug::Printf("[glestri] shader compile failed: %s", log);
		}
	return s;
	}

static void MainL()
	{
	RWsSession ws;
	User::LeaveIfError(ws.Connect());
	CleanupClosePushL(ws);
	CWsScreenDevice* screen = new (ELeave) CWsScreenDevice(ws);
	CleanupStack::PushL(screen);
	User::LeaveIfError(screen->Construct());
	RWindowGroup group(ws);
	User::LeaveIfError(group.Construct(0x474c5431, ETrue));
	CleanupClosePushL(group);
	RWindow win(ws);
	User::LeaveIfError(win.Construct(group, 0x474c5432));
	CleanupClosePushL(win);
	const TSize scr = screen->SizeInPixels();
	win.SetExtent(TPoint(0, 0), scr);
	win.Activate();
	ws.Flush();

	EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	EGLint major = 0, minor = 0;
	if (!eglInitialize(dpy, &major, &minor))
		{
		RDebug::Printf("[glestri] eglInitialize failed 0x%x", eglGetError());
		User::Leave(KErrNotSupported);
		}
	RDebug::Printf("[glestri] EGL %d.%d vendor '%s' apis '%s'", major, minor, eglQueryString(dpy, EGL_VENDOR),
		eglQueryString(dpy, EGL_CLIENT_APIS));
	const EGLint cfgAttr[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
		EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_NONE };
	EGLConfig cfg;
	EGLint n = 0;
	if (!eglChooseConfig(dpy, cfgAttr, &cfg, 1, &n) || n < 1)
		{
		RDebug::Printf("[glestri] eglChooseConfig failed 0x%x n=%d", eglGetError(), n);
		User::Leave(KErrNotSupported);
		}
	EGLint cfgId = 0, depth = 0, alpha = 0;
	eglGetConfigAttrib(dpy, cfg, EGL_CONFIG_ID, &cfgId);
	eglGetConfigAttrib(dpy, cfg, EGL_DEPTH_SIZE, &depth);
	eglGetConfigAttrib(dpy, cfg, EGL_ALPHA_SIZE, &alpha);
	RDebug::Printf("[glestri] config %d (of %d) depth %d alpha %d", cfgId, n, depth, alpha);
	EGLSurface surf = eglCreateWindowSurface(dpy, cfg, (EGLNativeWindowType)&win, NULL);
	if (surf == EGL_NO_SURFACE)
		{
		RDebug::Printf("[glestri] eglCreateWindowSurface failed 0x%x", eglGetError());
		User::Leave(KErrGeneral);
		}
	const EGLint ctxAttr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
	eglBindAPI(EGL_OPENGL_ES_API);
	EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxAttr);
	if (ctx == EGL_NO_CONTEXT)
		{
		RDebug::Printf("[glestri] eglCreateContext failed 0x%x", eglGetError());
		User::Leave(KErrGeneral);
		}
	if (!eglMakeCurrent(dpy, surf, surf, ctx))
		{
		RDebug::Printf("[glestri] eglMakeCurrent failed 0x%x", eglGetError());
		User::Leave(KErrGeneral);
		}
	RDebug::Printf("[glestri] GL_RENDERER '%s' GL_VERSION '%s'", glGetString(GL_RENDERER), glGetString(GL_VERSION));
	RDebug::Printf("[glestri] GLSL '%s'", glGetString(GL_SHADING_LANGUAGE_VERSION));

	GLuint prog = glCreateProgram();
	glAttachShader(prog, Compile(GL_VERTEX_SHADER, KVs));
	glAttachShader(prog, Compile(GL_FRAGMENT_SHADER, KFs));
	glBindAttribLocation(prog, 0, "aPos");
	glBindAttribLocation(prog, 1, "aCol");
	glLinkProgram(prog);
	GLint linked = 0;
	glGetProgramiv(prog, GL_LINK_STATUS, &linked);
	RDebug::Printf("[glestri] program linked=%d", linked);
	glUseProgram(prog);
	const GLint uAngle = glGetUniformLocation(prog, "uAngle");
	const GLint uAspect = glGetUniformLocation(prog, "uAspect");
	glUniform2f(uAspect, 1.0f, (GLfloat)scr.iWidth / (GLfloat)scr.iHeight);

	static const GLfloat KPos[] = { 0.0f, 0.8f, -0.7f, -0.45f, 0.7f, -0.45f };
	static const GLfloat KCol[] = { 1.0f, 0.2f, 0.2f, 0.2f, 1.0f, 0.2f, 0.2f, 0.4f, 1.0f };
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, KPos);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, KCol);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glViewport(0, 0, scr.iWidth, scr.iHeight);

	TUint32 t0 = User::NTickCount();
	TInt frames = 0;
	for (TInt f = 0; ; ++f)
		{
		const float t = f * 0.03f;
		glClearColor(0.10f + 0.08f * (float)sin(t * 0.7f), 0.12f, 0.20f + 0.08f * (float)cos(t * 0.5f), 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glUniform1f(uAngle, t);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		eglSwapBuffers(dpy, surf);
		++frames;
		const TUint32 now = User::NTickCount();
		if (now - t0 >= 2000)
			{
			RDebug::Printf("[glestri] %d frames in %u ms (glGetError 0x%x)", frames, now - t0, glGetError());
			frames = 0;
			t0 = now;
			}
		User::After(0);   // let the window server and other threads in
		}
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	RDebug::Printf("[glestri] start");
	TRAPD(err, MainL());
	RDebug::Printf("[glestri] exit err=%d", err);
	delete cleanup;
	return err;
	}
