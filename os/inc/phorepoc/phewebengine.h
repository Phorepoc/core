/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0 */

/*
 * phewebengine.h -- the C seam between the Phorepoc browser frontend (phebrowser) and a web engine (WebKit, NetSurf, ...).
 *
 * PUBLIC header, owned by core, consumed by the frontend and by every engine repo. Plain C, POD arguments, UTF-16 strings
 * (a Symbian TDesC16 goes straight through), no e32 and no engine headers. An engine is a DLL (or an ECom implementation of
 * interface PHE_WEBENGINE_ECOM_IFACE_UID) that exports ONE function, PheWebEngineGetApi, returning a const table of
 * function pointers. The frontend never links an engine; it loads the one the user picked. See
 * docs/phorepoc/browser-frontend.md in the design repo for the rationale.
 *
 * Threading: all calls are made from the frontend UI thread and must not block (queue the work). The engine calls
 * `notify` from ANY thread whenever a view has something new; it must only complete a request on the UI thread. The UI then
 * drains events with poll(). Pixels are shared through lock_pixels/unlock_pixels.
 *
 * Versioning: `version` is bumped on an incompatible change; `size` is sizeof(PheWebEngineApi) of the engine build so the
 * frontend can ignore trailing entries it does not know and treat entries beyond `size` as absent (NULL).
 */
#ifndef PHE_WEBENGINE_H
#define PHE_WEBENGINE_H

#ifdef __cplusplus
extern "C" {
#endif

#define PHE_WEBENGINE_VERSION 1
/* ECom interface implemented by engine packages (the implementation uid is chosen per engine) */
#define PHE_WEBENGINE_ECOM_IFACE_UID 0xE7016B01

typedef int PheView;   /* chosen by the frontend (new_view_id), 1.. ; 0 = no view */

/* ---- events (poll) ------------------------------------------------------------------------------------------------- */
enum PheEventKind {
    PHE_EV_NONE = 0,
    PHE_EV_START,         /* a main-frame navigation began; a = 1 when the page asked for it, 0 when the UI did */
    PHE_EV_COMMIT,        /* the new document exists: PHE_STR_URL is current; b = security level (PheSecurity) */
    PHE_EV_TITLE,         /* PHE_STR_TITLE changed */
    PHE_EV_PROGRESS,      /* a = 0..100 */
    PHE_EV_FINISHED,      /* load done */
    PHE_EV_FAILED,        /* a = error code, b = 1 HTTP/engine, 0 other; text in PHE_STR_ERROR */
    PHE_EV_REPAINT,       /* new pixels (lock_pixels); the engine repaints the whole view buffer */
    PHE_EV_HISTORY,       /* can_back / can_forward changed (get_state) */
    PHE_EV_SECURITY,      /* b = security level changed without a navigation (mixed content appeared, ...) */
    PHE_EV_FOCUS,         /* a = focused element kind: 0 none, 1 link, 2 text input (the frontend shows the keyboard) */
    PHE_EV_NEW_VIEW,      /* the page wants a new window/tab: URL in PHE_STR_NEWVIEW; a = 1 background */
    PHE_EV_FAVICON,       /* PHE_STR_FAVICON_URL is a new icon URL (optional) */
    PHE_EV_FIND_RESULT,   /* a = match count, b = current match (1-based, 0 none) */
    PHE_EV_DIALOG,        /* alert/confirm/prompt/auth/cert: a = PheDialogKind, id = request id; answer with dialog_reply */
    PHE_EV_DOWNLOAD,      /* a navigation became a download: id = download id; strings PHE_STR_DL_*; accept or cancel it */
    PHE_EV_DOWNLOAD_PROGRESS, /* id = download id, a = percent (-1 unknown), b = bytes done / 1024 */
    PHE_EV_DOWNLOAD_DONE, /* id = download id, a = 0 ok / error code */
    PHE_EV_DISCARDED,     /* the engine freed this (background) view under memory pressure; state kept for restore */
    PHE_EV_CRASHED        /* the view or engine died; the frontend offers Reload */
};

typedef struct PheEvent {
    int kind;
    PheView view;
    int a;
    int b;
    unsigned id;
} PheEvent;

enum PheString {
    PHE_STR_URL = 0, PHE_STR_TITLE, PHE_STR_ERROR, PHE_STR_NEWVIEW, PHE_STR_FAVICON_URL, PHE_STR_FOCUS_URL /* link under finger */,
    PHE_STR_DLG_TEXT, PHE_STR_DLG_DEFAULT, PHE_STR_DLG_HOST,   /* for PHE_EV_DIALOG (id selects the request) */
    PHE_STR_DL_URL, PHE_STR_DL_NAME, PHE_STR_DL_MIME,          /* for PHE_EV_DOWNLOAD (id selects the download) */
    PHE_STR_CERT_SUMMARY                                       /* issuer/subject/error text for a PHE_DLG_CERT dialog */
};

enum PheSecurity { PHE_SEC_NONE = 0 /* http, file */, PHE_SEC_SECURE = 1 /* valid https */, PHE_SEC_MIXED = 2, PHE_SEC_BROKEN = 3 /* cert error continued */ };
enum PheDialogKind { PHE_DLG_ALERT = 0, PHE_DLG_CONFIRM, PHE_DLG_PROMPT, PHE_DLG_AUTH /* user + password */, PHE_DLG_CERT /* continue? */ };

typedef struct PheViewState {
    int can_back, can_forward, loading, progress /* 0..100 */, security;
    int scroll_x, scroll_y;          /* content pixels */
    int content_w, content_h;        /* content size, content pixels (zoom applied) */
    int zoom_permille;               /* 1000 = 100% */
} PheViewState;

enum PheCommand { PHE_CMD_STOP = 0, PHE_CMD_RELOAD, PHE_CMD_BACK, PHE_CMD_FORWARD, PHE_CMD_ACTIVATE /* focused link */, PHE_CMD_RELOAD_BYPASS_CACHE };
enum PhePointer { PHE_PTR_DOWN = 0, PHE_PTR_MOVE, PHE_PTR_UP, PHE_PTR_LONG_PRESS };
enum PheSetting {
    PHE_SET_SCRIPT = 0,   /* 0/1 */
    PHE_SET_IMAGES,       /* 0/1 */
    PHE_SET_FONTSCALE,    /* permille */
    PHE_SET_COOKIES,      /* 0 off, 1 first-party, 2 all */
    PHE_SET_DESKTOP_MODE, /* 0 mobile UA/viewport, 1 desktop */
    PHE_SET_DNT           /* send Do-Not-Track / GPC */
};
enum PheCap {             /* PheWebEngineApi.caps */
    PHE_CAP_SCRIPT = 1, PHE_CAP_TLS13 = 2, PHE_CAP_FIND = 4, PHE_CAP_SNAPSHOT = 8, PHE_CAP_ZOOM = 16, PHE_CAP_DOWNLOAD = 32,
    PHE_CAP_PRIVATE = 64, PHE_CAP_SUSPEND = 128, PHE_CAP_STATE = 256, PHE_CAP_REFLOW = 512 /* text reflows at any zoom */
};
enum PheMemLevel { PHE_MEM_OK = 0, PHE_MEM_LOW = 1 /* drop caches */, PHE_MEM_CRITICAL = 2 /* discard background views */ };
#define PHE_FIND_BACKWARDS 1
#define PHE_FIND_CASE 2

typedef void (*PheNotify)(void* ctx, PheView view);   /* any thread; view 0 = engine-wide */

typedef struct PheWebEngineApi {
    unsigned size;                 /* sizeof(PheWebEngineApi) in the engine build */
    unsigned version;              /* PHE_WEBENGINE_VERSION */
    const char* name;              /* "WebKit", "NetSurf": shown in settings */
    const char* engine_version;    /* "2.54.1" */
    unsigned caps;                 /* PheCap bits */
    int max_views;                 /* 0 = no fixed limit (memory decides) */

    /* lifecycle: init once per process, shutdown on exit. Returns 0 or an error. */
    int  (*init)(PheNotify notify, void* ctx);
    void (*shutdown)(void);
    void (*set_useragent)(const unsigned short* s, int len);      /* len 0 = engine default */
    void (*set_iap)(const char* utf8_name);                       /* network interface (Open C name), optional */
    void (*memory_pressure)(int level);                           /* PheMemLevel, engine-wide */

    /* views (= tabs); asynchronous, ids chosen by the caller */
    PheView (*new_view_id)(void);
    void (*view_create)(PheView v, int w, int h, int is_private, const unsigned char* state, int state_len);
    void (*view_destroy)(PheView v);                              /* synchronous: no notify after return */
    void (*view_resize)(PheView v, int w, int h);
    void (*view_visible)(PheView v, int visible);                 /* 0 = background: stop painting and timers */
    void (*view_suspend)(PheView v);                              /* free render buffers + caches, keep history (cheap tabs) */
    int  (*view_save_state)(PheView v, unsigned char* buf, int cap); /* opaque session blob (history, scroll); returns full length */

    /* navigation */
    void (*load)(PheView v, const unsigned short* url, int len);
    void (*cmd)(PheView v, int cmd);
    void (*get_state)(PheView v, PheViewState* out);
    int  (*get_str)(PheView v, int which, unsigned short* buf, int cap);   /* returns the full length (may exceed cap) */
    int  (*poll)(PheView v, PheEvent* out);                       /* v = 0: any view. 1 = one event copied, 0 = none */
    void (*dialog_reply)(PheView v, unsigned id, int ok, const unsigned short* a, int alen, const unsigned short* b, int blen);

    /* view manipulation + input */
    void (*scroll)(PheView v, int dx, int dy);
    void (*zoom)(PheView v, int zoom_permille, int cx, int cy);   /* about a view point; engines without PHE_CAP_ZOOM ignore */
    void (*pointer)(PheView v, int type, int x, int y);
    void (*key)(PheView v, int scan_code, int unicode, int down);
    void (*text)(PheView v, const unsigned short* s, int len);    /* committed text from the VKB/IME */
    void (*setting)(PheView v, int id, int value);                /* v = 0: all views / default for new ones */

    /* pixels: BGRA 8888 premultiplied, stride in bytes. lock..unlock brackets the blit. */
    int  (*lock_pixels)(PheView v, const unsigned int** px, int* w, int* h, int* stride);
    void (*unlock_pixels)(PheView v);
    /* a scaled copy of the current page for the tab grid; optional (PHE_CAP_SNAPSHOT); returns 0 on success */
    int  (*snapshot)(PheView v, unsigned int* dst, int w, int h, int stride);

    /* find in page (PHE_CAP_FIND): results arrive as PHE_EV_FIND_RESULT */
    void (*find)(PheView v, const unsigned short* text, int len, int flags);
    void (*find_next)(PheView v, int backwards);
    void (*find_close)(PheView v);

    /* downloads (PHE_CAP_DOWNLOAD): the engine transfers, the frontend owns the UI and the destination */
    void (*download_accept)(PheView v, unsigned id, const unsigned short* path, int len);
    void (*download_cancel)(PheView v, unsigned id);

    /* privacy: bits 1 cache, 2 cookies, 4 history/session, 8 passwords, 16 site data */
    void (*clear_data)(int what_bits);
} PheWebEngineApi;

/* the one export of an engine DLL (ordinal 1), or what an ECom implementation factory returns */
typedef const PheWebEngineApi* (*PheWebEngineGetApiFn)(unsigned wanted_version);
const PheWebEngineApi* PheWebEngineGetApi(unsigned wanted_version);

#ifdef __cplusplus
}
#endif
#endif
