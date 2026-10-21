// Copyright The Phorepoc Authors
// SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later

// gles1tri.cpp -- OURS (GPU plan G2/G4, 2026-10-09): the OpenGL ES 1.1 fixed-function sample -- a lit, rotating
// cube with per-vertex colours over libGLESv1_CM.dll + libEGL.dll (the 2010 ABIs; Mesa's GLES1 behind them), the
// APIs most Symbian^3 games use. "[gles1tri] N frames in T ms" every ~2 s in the log.
#include <e32std.h>
#include <e32base.h>
#include <e32debug.h>
#include <w32std.h>
#include <EGL/egl.h>
#include <GLES/gl.h>

static const GLfloat KVerts[] = {
	-1,-1, 1,  1,-1, 1,  1, 1, 1, -1, 1, 1,	// front
	-1,-1,-1, -1, 1,-1,  1, 1,-1,  1,-1,-1,	// back
	-1, 1,-1, -1, 1, 1,  1, 1, 1,  1, 1,-1,	// top
	-1,-1,-1,  1,-1,-1,  1,-1, 1, -1,-1, 1,	// bottom
	 1,-1,-1,  1, 1,-1,  1, 1, 1,  1,-1, 1,	// right
	-1,-1,-1, -1,-1, 1, -1, 1, 1, -1, 1,-1,	// left
};
static const GLfloat KNormals[] = {
	0,0,1, 0,0,1, 0,0,1, 0,0,1,   0,0,-1, 0,0,-1, 0,0,-1, 0,0,-1,
	0,1,0, 0,1,0, 0,1,0, 0,1,0,   0,-1,0, 0,-1,0, 0,-1,0, 0,-1,0,
	1,0,0, 1,0,0, 1,0,0, 1,0,0,   -1,0,0, -1,0,0, -1,0,0, -1,0,0,
};
static const GLubyte KColours[] = {
	255,80,80,255, 255,80,80,255, 255,80,80,255, 255,80,80,255,
	80,255,80,255, 80,255,80,255, 80,255,80,255, 80,255,80,255,
	80,80,255,255, 80,80,255,255, 80,80,255,255, 80,80,255,255,
	255,255,80,255, 255,255,80,255, 255,255,80,255, 255,255,80,255,
	255,80,255,255, 255,80,255,255, 255,80,255,255, 255,80,255,255,
	80,255,255,255, 80,255,255,255, 80,255,255,255, 80,255,255,255,
};
static const GLushort KIndices[] = {
	0,1,2, 0,2,3,  4,5,6, 4,6,7,  8,9,10, 8,10,11,  12,13,14, 12,14,15,  16,17,18, 16,18,19,  20,21,22, 20,22,23 };

static void MainL()
	{
	RWsSession ws;
	User::LeaveIfError(ws.Connect());
	CleanupClosePushL(ws);
	CWsScreenDevice* screen = new (ELeave) CWsScreenDevice(ws);
	CleanupStack::PushL(screen);
	User::LeaveIfError(screen->Construct());
	RWindowGroup group(ws);
	User::LeaveIfError(group.Construct(0x474c5331, ETrue));
	CleanupClosePushL(group);
	RWindow win(ws);
	User::LeaveIfError(win.Construct(group, 0x474c5332));
	CleanupClosePushL(win);
	const TSize scr = screen->SizeInPixels();
	win.SetExtent(TPoint(0, 0), scr);
	win.Activate();
	ws.Flush();

	EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	EGLint major = 0, minor = 0;
	if (!eglInitialize(dpy, &major, &minor))
		{
		RDebug::Printf("[gles1tri] eglInitialize failed 0x%x", eglGetError());
		User::Leave(KErrNotSupported);
		}
	RDebug::Printf("[gles1tri] EGL %d.%d '%s'", major, minor, eglQueryString(dpy, EGL_VERSION));
	const EGLint cfgAttr[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
		EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_NONE };
	EGLConfig cfg;
	EGLint n = 0;
	if (!eglChooseConfig(dpy, cfgAttr, &cfg, 1, &n) || n < 1)
		{
		RDebug::Printf("[gles1tri] eglChooseConfig failed 0x%x", eglGetError());
		User::Leave(KErrNotSupported);
		}
	EGLSurface surf = eglCreateWindowSurface(dpy, cfg, (EGLNativeWindowType)&win, NULL);
	eglBindAPI(EGL_OPENGL_ES_API);
	const EGLint ctxAttr[] = { EGL_CONTEXT_CLIENT_VERSION, 1, EGL_NONE };
	EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxAttr);
	if (surf == EGL_NO_SURFACE || ctx == EGL_NO_CONTEXT || !eglMakeCurrent(dpy, surf, surf, ctx))
		{
		RDebug::Printf("[gles1tri] surface/context/current failed 0x%x", eglGetError());
		User::Leave(KErrGeneral);
		}
	RDebug::Printf("[gles1tri] GL_RENDERER '%s' GL_VERSION '%s'", glGetString(GL_RENDERER), glGetString(GL_VERSION));

	glViewport(0, 0, scr.iWidth, scr.iHeight);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	const GLfloat aspect = (GLfloat)scr.iWidth / (GLfloat)scr.iHeight;
	glFrustumf(-aspect, aspect, -1.0f, 1.0f, 2.0f, 20.0f);
	glMatrixMode(GL_MODELVIEW);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);
	glEnable(GL_LIGHTING);
	glEnable(GL_LIGHT0);
	glEnable(GL_COLOR_MATERIAL);
	static const GLfloat KLightPos[] = { 2.0f, 4.0f, 6.0f, 0.0f };
	static const GLfloat KLightDiffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	static const GLfloat KLightAmbient[] = { 0.25f, 0.25f, 0.25f, 1.0f };
	glLightfv(GL_LIGHT0, GL_POSITION, KLightPos);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, KLightDiffuse);
	glLightfv(GL_LIGHT0, GL_AMBIENT, KLightAmbient);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, KVerts);
	glNormalPointer(GL_FLOAT, 0, KNormals);
	glColorPointer(4, GL_UNSIGNED_BYTE, 0, KColours);
	glClearColor(0.08f, 0.10f, 0.16f, 1.0f);

	TUint32 t0 = User::NTickCount();
	TInt frames = 0;
	for (TInt f = 0; ; ++f)
		{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glLoadIdentity();
		glTranslatef(0.0f, 0.0f, -6.0f);
		glRotatef(f * 0.9f, 1.0f, 0.0f, 0.0f);
		glRotatef(f * 1.3f, 0.0f, 1.0f, 0.0f);
		glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, KIndices);
		eglSwapBuffers(dpy, surf);
		++frames;
		const TUint32 now = User::NTickCount();
		if (now - t0 >= 2000)
			{
			RDebug::Printf("[gles1tri] %d frames in %u ms (glGetError 0x%x)", frames, now - t0, glGetError());
			frames = 0;
			t0 = now;
			}
		User::After(0);
		}
	}

GLDEF_C TInt E32Main()
	{
	CTrapCleanup* cleanup = CTrapCleanup::New();
	RDebug::Printf("[gles1tri] start");
	TRAPD(err, MainL());
	RDebug::Printf("[gles1tri] exit err=%d", err);
	delete cleanup;
	return err;
	}
