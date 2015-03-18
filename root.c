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

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>

#include "wind.h"

static void fnkey(KeySym, unsigned, Time, int);
static void configurerequest(XConfigureRequestEvent *);
static void maprequest(XMapRequestEvent *);
static void keypress(XKeyEvent *);
static void clientmessage(XClientMessageEvent *);
static void unmapnotify(XUnmapEvent *);
static void enternotify(XCrossingEvent *);
static void leavenotify(XCrossingEvent *);
static void event(void *, XEvent *);

// True if the pointer is on this screen
static Bool pointerhere;

static const struct listener listener = {
	.function = event,
	.pointer = NULL,
};

static struct keybind {
	KeySym keysym;
	unsigned modifiers;
	void (*function)(KeySym key, unsigned state, Time t, int arg);
	int arg;
	KeyCode keycode;
} keymap[] = {
	{ XK_F1, Mod1Mask, fnkey, 1 },
	{ XK_F2, Mod1Mask, fnkey, 2 },
	{ XK_F3, Mod1Mask, fnkey, 3 },
	{ XK_F4, Mod1Mask, fnkey, 4 },
	{ XK_F5, Mod1Mask, fnkey, 5 },
	{ XK_F6, Mod1Mask, fnkey, 6 },
	{ XK_F7, Mod1Mask, fnkey, 7 },
	{ XK_F8, Mod1Mask, fnkey, 8 },
	{ XK_F9, Mod1Mask, fnkey, 9 },
	{ XK_F10, Mod1Mask, fnkey, 10 },
	{ XK_F11, Mod1Mask, fnkey, 11 },
	{ XK_F12, Mod1Mask, fnkey, 12 },

	{ XK_F1, ShiftMask | Mod1Mask, fnkey, 1 },
	{ XK_F2, ShiftMask | Mod1Mask, fnkey, 2 },
	{ XK_F3, ShiftMask | Mod1Mask, fnkey, 3 },
	{ XK_F4, ShiftMask | Mod1Mask, fnkey, 4 },
	{ XK_F5, ShiftMask | Mod1Mask, fnkey, 5 },
	{ XK_F6, ShiftMask | Mod1Mask, fnkey, 6 },
	{ XK_F7, ShiftMask | Mod1Mask, fnkey, 7 },
	{ XK_F8, ShiftMask | Mod1Mask, fnkey, 8 },
	{ XK_F9, ShiftMask | Mod1Mask, fnkey, 9 },
	{ XK_F10, ShiftMask | Mod1Mask, fnkey, 10 },
	{ XK_F11, ShiftMask | Mod1Mask, fnkey, 11 },
	{ XK_F12, ShiftMask | Mod1Mask, fnkey, 12 },
};

static void fnkey(KeySym keysym, unsigned state, Time time, int arg)
{
	if (state & ShiftMask)
		setndesk(arg);
	gotodesk(arg - 1);
	refocus(time);
}

static void configurerequest(XConfigureRequestEvent *e)
{
	// First try to redirect the event.
	if (redirect((XEvent *)e, e->window) == 0)
		return;

	// Nobody listens to this window so we'll just
	// do whatever it wants us to do.

	// Ignore stacking requests for now.
	e->value_mask &= ~(CWSibling | CWStackMode);

	XConfigureWindow(dpy, e->window, e->value_mask,
			&(XWindowChanges){
				.x = e->x,
				.y = e->y,
				.width = e->width,
				.height = e->height,
				.border_width = e->border_width,
				.sibling = e->above,
				.stack_mode = e->detail });
}

static void maprequest(XMapRequestEvent *e)
{
	// Already managed?
	if (redirect((XEvent *)e, e->window) == 0)
		return;

	// Try to manage it, otherwise just map it.
	if (manage(e->window) == NULL)
		XMapWindow(dpy, e->window);
}

static void keypress(XKeyEvent *e)
{
	for (int i = 0; i < NELEM(keymap); i++)
		if (keymap[i].keycode == e->keycode) {
			keymap[i].function(keymap[i].keysym, e->state,
					e->time, keymap[i].arg);
			break;
		}
}

static void clientmessage(XClientMessageEvent *e)
{
	ewmh_rootclientmessage(e);
}

/*
 * Refer to the ICCCM section 4.1.4, "Changing Window State",
 * for information on the synthetic UnmapNotify event sent
 * by clients to the root window on withdrawal.
 */
static void unmapnotify(XUnmapEvent *e)
{
	if (e->send_event)
		redirect((XEvent *)e, e->window);
}

/*
 * Refocus whenever the pointer enters our root window from
 * another screen.
 */
static void enternotify(XCrossingEvent *e)
{
	if (e->detail == NotifyNonlinear ||
			e->detail == NotifyNonlinearVirtual) {
		pointerhere = True;
		refocus(e->time);
	}
}

/*
 * Give up focus if the pointer leaves our screen.
 */
static void leavenotify(XCrossingEvent *e)
{
	if (e->detail == NotifyNonlinear ||
			e->detail == NotifyNonlinearVirtual) {
		pointerhere = False;
		XSetInputFocus(dpy, PointerRoot, RevertToPointerRoot, e->time);
	}
}

static void event(void *self, XEvent *e)
{
	switch (e->type) {
	case MapRequest:
		maprequest(&e->xmaprequest);
		break;
	case ConfigureRequest:
		configurerequest(&e->xconfigurerequest);
		break;
	case KeyPress:
		keypress(&e->xkey);
		break;
	case ClientMessage:
		clientmessage(&e->xclient);
		break;
	case UnmapNotify:
		unmapnotify(&e->xunmap);
		break;
	case EnterNotify:
		enternotify(&e->xcrossing);
		break;
	case LeaveNotify:
		leavenotify(&e->xcrossing);
		break;
	}
}

void initroot(void)
{
	setlistener(root, &listener);

	XSync(dpy, False);
	xerror = NULL;
	XSelectInput(dpy, root,
			EnterWindowMask |
			LeaveWindowMask |
			SubstructureRedirectMask |
			SubstructureNotifyMask);
	XSync(dpy, False);
	if (xerror != NULL) {
		errorf("display \"%s\" already has a window manager",
				XDisplayName(displayname));
		exit(1);
	}

	for (int i = 0; i < NELEM(keymap); i++) {
		struct keybind *k = &keymap[i];
		k->keycode = XKeysymToKeycode(dpy, k->keysym);
		grabkey(k->keycode, k->modifiers, root, True,
				GrabModeAsync, GrabModeAsync);
	}

	Window r, c;
	int rx, ry, x, y;
	unsigned m;
	XQueryPointer(dpy, root, &r, &c, &rx, &ry, &x, &y, &m);
	pointerhere = (r == root);
}
