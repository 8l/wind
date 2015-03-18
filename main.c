/*
 * Copyright 2010 Johan Veenhuizen
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>

#include "wind.h"

#include "deleven.xbm"
#include "delodd.xbm"

static int errhandler(Display *, XErrorEvent *);
static void onsignal(int);
static int waitevent(void);
static void usage(FILE *);
static struct listener *getlistener(Window);

enum runlevel runlevel = RL_STARTUP;

static DEFINE_BITMAP(deleven);
static DEFINE_BITMAP(delodd);

struct bitmap *deletebitmap;

static const char *progname;

static int exitstatus;

/*
 * If true, enable debug mode. This will enable synchronous
 * X11 transactions, and print Xlib errors to standard error.
 */
static Bool debug = False;

// The display name used in call to XOpenDisplay
const char *displayname = NULL;

// The last X error reported
const char *xerror = NULL;

Display *dpy;
unsigned scr;
Window root;
unsigned long foregroundpixel;
unsigned long backgroundpixel;
unsigned long hlforegroundpixel;
unsigned long hlbackgroundpixel;
GC foreground;
GC background;
GC hlforeground;
GC hlbackground;

int lineheight;
int halfleading;

struct font *font;
struct fontcolor *fhighlight;
struct fontcolor *fnormal;

Atom WM_CHANGE_STATE;
Atom WM_DELETE_WINDOW;
Atom WM_PROTOCOLS;
Atom WM_STATE;

static XContext listeners;

static sigset_t sigmask;

/*
 * Print formatted error message
 */
void errorf(const char *fmt, ...)
{
	va_list ap;
	fprintf(stderr, "%s: ", progname);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fprintf(stderr, "\n");
}

void setlistener(Window w, const struct listener *l)
{
	if (l == NULL)
		XDeleteContext(dpy, w, listeners);
	else
		XSaveContext(dpy, w, listeners, (XPointer)l);
}

static struct listener *getlistener(Window w)
{
	struct listener *l;
	if (XFindContext(dpy, w, listeners, (XPointer *)&l) == 0)
		return l;
	else
		return NULL;
}

int redirect(XEvent *e, Window w)
{
	struct listener *l = getlistener(w);
	if (l == NULL)
		return -1;
	l->function(l->pointer, e);
	return 0;
}

static int errhandler(Display *dpy, XErrorEvent *e)
{
	static char buf[128];
	buf[0] = '\0';
	XGetErrorText(dpy, e->error_code, buf, sizeof buf);
	if (debug)
		errorf("Xlib: %s", buf);
	xerror = buf;
	return 0;
}

static void onsignal(int signo)
{
}

static int waitevent(void)
{
	if (XPending(dpy) > 0)
		return 0;

	fd_set rfds;
	int nfds = 0;
	FD_ZERO(&rfds);

	int conn = ConnectionNumber(dpy);
	FD_SET(conn, &rfds);
	nfds = MAX(conn + 1, nfds);

	if (pselect(nfds, &rfds, NULL, NULL, NULL, &sigmask) == -1) {
		errorf("pselect: %s", strerror(errno));
		exitstatus = 1;
		return -1;
	} else if (FD_ISSET(conn, &rfds)) {
		// Normal X event
		return 0;
	} else {
		// Shouldn't happen
		errorf("BUG: unhandled pselect condition");
		exitstatus = 1;
		return -1;
	}
}

static void usage(FILE *f)
{
	fprintf(f, "usage: %s [ -v ]"
			" [ -n number ]"
			" [ -t font ]"
			" [ -f color ]"
			" [ -b color ]"
			" [ -F color ]"
			" [ -B color ]"
			" [ display ]\n", progname);
}

int main(int argc, char *argv[])
{
	progname = argv[0];

	// The Xmb* functions use LC_CTYPE
	setlocale(LC_CTYPE, "");

	srand((unsigned)time(NULL));

	runlevel = RL_STARTUP;

	char *ftname = NULL;

	char *fname = "rgb:00/00/00";
	char *bname = "rgb:ff/ff/ff";
	char *hlfname = "rgb:00/00/00";
	char *hlbname = "rgb:00/ff/ff";

	Desk ndesk = 0;

	int opt;
	while ((opt = getopt(argc, argv, "B:b:F:f:n:t:v")) != -1)
		switch (opt) {
		case 'B':
			hlbname = optarg;
			break;
		case 'b':
			bname = optarg;
			break;
		case 'F':
			hlfname = optarg;
			break;
		case 'f':
			fname = optarg;
			break;
		case 'n':
			errno = 0;
			char *p;
			long n = strtol(optarg, &p, 10);
			if (n < 0 || errno != 0 ||
					*optarg == '\0' || *p != '\0') {
				errorf("%s: invalid desktop count", optarg);
				exit(1);
			}
			ndesk = n;
			break;
		case 't':
			ftname = optarg;
			break;
		case 'v':
			debug = True;
			break;
		default:
			usage(stderr);
			exit(1);
		}

	if (optind < argc)
		displayname = argv[optind++];

	if (optind < argc) {
		errorf("unexpected argument -- %s", argv[optind]);
		usage(stderr);
		exit(1);
	}

	if (debug) {
		fprintf(stderr, "%s\n", PACKAGE_STRING);
		fprintf(stderr, "Synchronous DEBUG mode enabled. "
				"Printing Xlib errors on standard error.\n");
		fprintf(stderr, "Report bugs to <%s>.\n", PACKAGE_BUGREPORT);
	}

	XSetErrorHandler(errhandler);

	if ((dpy = XOpenDisplay(displayname)) == NULL) {
		errorf("cannot open display \"%s\"",
				XDisplayName(displayname));
		exit(1);
	}

	XSynchronize(dpy, debug);

	scr = DefaultScreen(dpy);
	root = DefaultRootWindow(dpy);

	font = ftload(ftname);
	if (font == NULL) {
		errorf("cannot load font");
		exit(1);
	}
	fnormal = ftloadcolor(fname);
	fhighlight = ftloadcolor(hlfname);
	if (fnormal == NULL || fhighlight == NULL) {
		errorf("cannot load font colors");
		exit(1);
	}

	halfleading = (3 * font->size / 10) / 2;
	lineheight = font->size + 2 * halfleading;

	if (lineheight % 2 == 0)
		deletebitmap = &deleven;
	else
		deletebitmap = &delodd;

	foregroundpixel = getpixel(fname);
	backgroundpixel = getpixel(bname);
	hlforegroundpixel = getpixel(hlfname);
	hlbackgroundpixel = getpixel(hlbname);

	foreground = XCreateGC(dpy, root, GCForeground | GCBackground,
			&(XGCValues){
				.foreground = foregroundpixel,
				.background = backgroundpixel });
	background = XCreateGC(dpy, root, GCForeground | GCBackground,
			&(XGCValues){
				.foreground = backgroundpixel,
				.background = foregroundpixel });
	hlforeground = XCreateGC(dpy, root, GCForeground | GCBackground,
			&(XGCValues){
				.foreground = hlforegroundpixel,
				.background = hlbackgroundpixel });
	hlbackground = XCreateGC(dpy, root, GCForeground | GCBackground,
			&(XGCValues){
				.foreground = hlbackgroundpixel,
				.background = hlforegroundpixel });

	listeners = XUniqueContext();

	sigset_t sigsafemask;
	sigprocmask(SIG_SETMASK, NULL, &sigmask);
	sigprocmask(SIG_SETMASK, NULL, &sigsafemask);

	struct sigaction sa;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = onsignal;
	struct sigaction osa;

	sigaction(SIGHUP, NULL, &osa);
	if (osa.sa_handler != SIG_IGN) {
		sigaction(SIGHUP, &sa, NULL);
		sigaddset(&sigsafemask, SIGHUP);
	}

	sigaction(SIGINT, NULL, &osa);
	if (osa.sa_handler != SIG_IGN) {
		sigaction(SIGINT, &sa, NULL);
		sigaddset(&sigsafemask, SIGINT);
	}

	sigaction(SIGTERM, NULL, &osa);
	if (osa.sa_handler != SIG_IGN) {
		sigaction(SIGTERM, &sa, NULL);
		sigaddset(&sigsafemask, SIGTERM);
	}

	sigprocmask(SIG_SETMASK, &sigsafemask, NULL);

	WM_CHANGE_STATE = XInternAtom(dpy, "WM_CHANGE_STATE", False);
	WM_DELETE_WINDOW = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	WM_PROTOCOLS = XInternAtom(dpy, "WM_PROTOCOLS", False);
	WM_STATE = XInternAtom(dpy, "WM_STATE", False);

	initroot();
	ewmh_startwm();
	mwm_startwm();

	if (ndesk != 0)
		setndesk(ndesk);

	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);
	manageall();

	refocus(CurrentTime);

	runlevel = RL_NORMAL;

	while (waitevent() != -1) {
		XEvent e;
		XNextEvent(dpy, &e);
		if (redirect(&e, e.xany.window) == -1) {
			/*
			 * EWMH specifies some root window client
			 * messages with a non-root event window,
			 * so we need to redirect those manually.
			 */
			if (e.type == ClientMessage)
				redirect(&e, root);
		}
		restack();
	}

	runlevel = RL_SHUTDOWN;

	// We make sure the focused window stays on top
	// when we map windows from other desktops, and
	// to warp the pointer so that focus is not lost.
	Window w = None;
	struct geometry g;
	struct client *c = getfocus();
	if (c != NULL) {
		cpopapp(c);
		restack();
		w = cgetwin(c);
		g = cgetgeom(c);
	}

	unmanageall();

	if (w != None)
		XWarpPointer(dpy, None, w, 0, 0, 0, 0,
				g.width / 2, g.height / 2);
	XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, CurrentTime);

	ewmh_stopwm();

	ftfreecolor(fnormal);
	ftfreecolor(fhighlight);
	ftfree(font);

	XFreeGC(dpy, foreground);
	XFreeGC(dpy, background);
	XFreeGC(dpy, hlforeground);
	XFreeGC(dpy, hlbackground);
	XCloseDisplay(dpy);

	return exitstatus;
}
