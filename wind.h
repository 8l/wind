#ifndef WIND_H
#define WIND_H

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define NELEM(v) (sizeof v / sizeof v[0])

// The desktop number of sticky windows
#define DESK_ALL 0xffffffffUL

typedef unsigned long Desk;

struct geometry {
	int x;
	int y;
	int width;
	int height;
	int borderwidth;
};

struct extents {
	int top;
	int bottom;
	int left;
	int right;
};

struct listener {
	void *pointer;
	void (*function)(void *, XEvent *);
};

struct bitmap {
	unsigned char *bits;
	int width;
	int height;
	Pixmap pixmap;
};

#define DEFINE_BITMAP(name) \
		struct bitmap name = { \
			.bits = name##_bits, \
			.width = name##_width, \
			.height = name##_height, \
			.pixmap = None \
		}

struct button;
struct client;
struct dragger;
struct frame;

struct font {
	int ascent;
	int descent;
	int size;
	void *data;
};

extern enum runlevel {
	RL_STARTUP, RL_NORMAL, RL_SHUTDOWN,
} runlevel;

// The display name argument used in call to XOpenDisplay
extern const char *displayname;

// The last X error reported
extern const char *xerror;

extern Display *dpy;
extern unsigned scr;
extern Window root;

// Normal colors
extern unsigned long foregroundpixel;
extern unsigned long backgroundpixel;

// Highlight colors
extern unsigned long hlforegroundpixel;
extern unsigned long hlbackgroundpixel;

// Normal colors
extern GC foreground;
extern GC background;

// Highlight colors
extern GC hlforeground;
extern GC hlbackground;

extern int lineheight;
extern int halfleading;

struct font *font;
struct fontcolor *fhighlight;
struct fontcolor *fnormal;

extern struct bitmap *deletebitmap;

extern Atom WM_CHANGE_STATE;
extern Atom WM_DELETE_WINDOW;
extern Atom WM_PROTOCOLS;
extern Atom WM_STATE;

void errorf(const char *, ...);
void setlistener(Window, const struct listener *);
int redirect(XEvent *, Window);

struct fontcolor *ftloadcolor(const char *);
void ftfreecolor(struct fontcolor *);
struct font *ftload(const char *);
void ftfree(struct font *);
void ftdrawstring(Drawable, struct font *, struct fontcolor *, int, int,
		const char *);
void ftdrawstring_utf8(Drawable, struct font *, struct fontcolor *, int, int,
		const char *);
int fttextwidth(struct font *, const char *);
int fttextwidth_utf8(struct font *, const char *);

void initroot(void);

struct frame *fcreate(struct client *);
void fdestroy(struct frame *);
void fupdate(struct frame *);
Window fgetwin(struct frame *);
struct geometry fgetgeom(struct frame *);
struct extents estimateframeextents(Window);

struct client *manage(Window);
void manageall(void);
void unmanageall(void);
void cpopapp(struct client *);
void cpushapp(struct client *);
void cdelete(struct client *, Time);
void csetdesk(struct client *, Desk);
void csetappdesk(struct client *, Desk);
Desk cgetdesk(struct client *);
void csetdock(struct client *, Bool);
void csetfull(struct client *, Bool);
void csetundecorated(struct client *, Bool);
void csetappfollowdesk(struct client *, Bool);
struct client *getfocus(void);
void cfocus(struct client *, Time);
struct client *refocus(Time);
Bool cistask(struct client *);
Bool cisvisible(struct client *);
void setndesk(Desk);
void gotodesk(Desk);
void getwindowstack(Window **, size_t *);
int namewidth(struct font *, struct client *);
void drawname(Drawable, struct font *, struct fontcolor *,
		int, int, struct client *);
Bool chaswmproto(struct client *, Atom);
void restack(void);
int cgetgrav(struct client *);
struct geometry cgetgeom(struct client *);
void csetgeom(struct client *, struct geometry);
void csendconf(struct client *);
Bool chasfocus(struct client *);
Window cgetwin(struct client *);
void csetnetwmname(struct client *, const char *);
void cignoreunmap(struct client *);
Bool cismapped(struct client *);
Bool cisurgent(struct client *);
void csetskiptaskbar(struct client *, Bool);
void chintsize(struct client *, int, int, int *, int *);

struct button *bcreate(void (*)(void *, Time), void *, struct bitmap *,
		Window, int, int, int, int, int);
void bdestroy(struct button *);

struct dragger *dcreate(Window, int, int, int, int, int, Cursor,
		void (*)(void *, int, int, unsigned long, Time), void *);
void ddestroy(struct dragger *);

void ewmh_notifyndesk(unsigned long);
void ewmh_notifycurdesk(unsigned long);
void ewmh_notifyclientdesktop(Window, unsigned long);
void ewmh_notifyframeextents(Window, struct extents);
void ewmh_startwm(void);
void ewmh_stopwm(void);
void ewmh_maprequest(struct client *);
void ewmh_notifyfull(Window, Bool);
void ewmh_manage(struct client *);
void ewmh_unmanage(struct client *);
void ewmh_withdraw(struct client *);
void ewmh_notifyfocus(Window, Window);
void ewmh_notifyrestack(void);
void ewmh_propertynotify(struct client *, XPropertyEvent *);
void ewmh_clientmessage(struct client *, XClientMessageEvent *);
void ewmh_rootclientmessage(XClientMessageEvent *);

void mwm_startwm(void);
void mwm_manage(struct client *);
void mwm_propertynotify(struct client *, XPropertyEvent *);

void *xmalloc(size_t);
void *xrealloc(const void *, size_t);
char *xstrdup(const char *);
void grabkey(int, unsigned, Window, Bool, int, int);
void ungrabkey(int, unsigned, Window);
void grabbutton(unsigned, unsigned, Window, Bool, unsigned, int, int,
		Window, Cursor);
void ungrabbutton(unsigned, unsigned, Window);
long getwmstate(Window);
void setwmstate(Window, long);
Bool ismapped(Window);
char *decodetextproperty(XTextProperty *);
void setprop(Window, Atom, Atom, int, void *, int);
void *getprop(Window, Atom, Atom, int, unsigned long *);
void drawbitmap(Drawable, GC, struct bitmap *, int, int);
unsigned long getpixel(const char *);

#endif
