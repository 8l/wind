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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/cursorfont.h>

#include "wind.h"

#define EXT_TOP (lineheight + 2)
#define EXT_BOTTOM (halfleading + 1)
#define EXT_LEFT (halfleading + 1)
#define EXT_RIGHT (halfleading + 1)

struct frame {
	struct listener listener;
	struct client *client;
	struct button *deletebutton;
	struct dragger *topleftresizer;
	struct dragger *toprightresizer;
	Pixmap pixmap;
	GC *background;
	int namewidth;
	int x;
	int y;
	int width;
	int height;
	Window window;
	int downx;	// window relative pointer x at button press
	int downy;	// window relative pointer y at button press
	Bool grabbed;
};

static void reorder(Window, Window);
static void setgrav(Window, int);
static void gravitate(int, int, int *, int *);
static void confrequest(struct frame *, XConfigureRequestEvent *);
static void repaint(struct frame *);
static void buttonpress(struct frame *, XButtonEvent *);
static void buttonrelease(struct frame *, XButtonEvent *);
static void moveresize(struct frame *, int, int, int, int);
static void motionnotify(struct frame *, XMotionEvent*);
static void maprequest(struct frame *, XMapRequestEvent *);
static void expose(struct frame *, XExposeEvent *);
static void event(void *, XEvent *);
static void delete(void *, Time);
static void resizetopleft(void *, int, int, unsigned long, Time);
static void resizetopright(void *, int, int, unsigned long, Time);

static size_t fcount;
static Cursor cursortopleft = None;
static Cursor cursortopright = None;

/*
 * XXX: We cheat here and always estimate normal frame
 * extents, even if the window is of a type that will
 * not get a frame. This is hopefully okay since most
 * clients requesting estimates of frame extents will
 * probably be interested in having a frame.
 */
struct extents estimateframeextents(Window w)
{
	return (struct extents){
			.top = EXT_TOP,
			.bottom = EXT_BOTTOM,
			.left = EXT_LEFT,
			.right = EXT_RIGHT };
}

static void reorder(Window ref, Window below)
{
	XRestackWindows(dpy, (Window[]){ ref, below }, 2);
}

static void setgrav(Window win, int grav)
{
	XChangeWindowAttributes(dpy, win, CWWinGravity,
			&(XSetWindowAttributes){ .win_gravity = grav });
}

static void gravitate(int wingrav, int borderwidth, int *dx, int *dy)
{
	switch (wingrav) {
	case NorthWestGravity:
		*dx = 0;
		*dy = 0;
		break;
	case NorthGravity:
		*dx = borderwidth - (EXT_LEFT + EXT_RIGHT) / 2;
		*dy = 0;
		break;
	case NorthEastGravity:
		*dx = (2 * borderwidth) - (EXT_LEFT + EXT_RIGHT);
		*dy = 0;
		break;

	case WestGravity:
		*dx = 0;
		*dy = borderwidth - (EXT_TOP + EXT_BOTTOM) / 2;
		break;
	case CenterGravity:
		*dx = borderwidth - (EXT_LEFT + EXT_RIGHT) / 2;
		*dy = borderwidth - (EXT_TOP + EXT_BOTTOM) / 2;
		break;
	case EastGravity:
		*dx = (2 * borderwidth) - (EXT_LEFT + EXT_RIGHT);
		*dy = borderwidth - (EXT_TOP + EXT_BOTTOM) / 2;
		break;

	case SouthWestGravity:
		*dx = 0;
		*dy = (2 * borderwidth) - (EXT_TOP + EXT_BOTTOM);
		break;
	case SouthGravity:
		*dx = borderwidth - (EXT_LEFT + EXT_RIGHT) / 2;
		*dy = (2 * borderwidth) - (EXT_TOP + EXT_BOTTOM);
		break;
	case SouthEastGravity:
		*dx = (2 * borderwidth) - (EXT_LEFT + EXT_RIGHT);
		*dy = (2 * borderwidth) - (EXT_TOP + EXT_BOTTOM);
		break;

	case StaticGravity:
		*dx = borderwidth - EXT_LEFT;
		*dy = borderwidth - EXT_TOP;;
		break;

	default:
		errorf("unknown window gravity %d", wingrav);
		*dx = 0;
		*dy = 0;
		break;
	}
}

void fupdate(struct frame *f)
{
	if (chaswmproto(f->client, WM_DELETE_WINDOW)) {
		if (f->deletebutton == NULL) {
			int sz = lineheight + 2;
			f->deletebutton = bcreate(delete, f->client,
					deletebitmap, f->window,
					f->width - 1 - font->size - sz, 0,
					sz, sz, NorthEastGravity);
		}
	} else if (f->deletebutton != NULL) {
		bdestroy(f->deletebutton);
		f->deletebutton = NULL;
	}

	Bool hasfocus = chasfocus(f->client);

	f->background = hasfocus ? &hlbackground : &background;

	if (f->pixmap != None) {
		XFreePixmap(dpy, f->pixmap);
		f->pixmap = None;
	}
	f->namewidth = namewidth(font, f->client);
	if (f->namewidth > 0) {
		f->pixmap = XCreatePixmap(dpy, root, f->namewidth,
				lineheight, DefaultDepth(dpy, scr));
		XFillRectangle(dpy, f->pixmap, *f->background,
				0, 0, f->namewidth, lineheight);
		drawname(f->pixmap, font, hasfocus ? fhighlight: fnormal,
				0, halfleading + font->ascent, f->client);

		if (cgetdesk(f->client) == DESK_ALL) {
			int y = halfleading + font->ascent + font->descent / 2;
			XDrawLine(dpy, f->pixmap,
					hasfocus ? hlforeground : foreground,
					0, y, f->namewidth, y);
		}
	}

	repaint(f);
}

static void repaint(struct frame *f)
{
	int namewidth = f->namewidth;
	namewidth = MIN(namewidth, f->width - 2 * (1 + font->size));
	namewidth = MAX(namewidth, 0);

	// Title area
	int x = 1;
	XFillRectangle(dpy, f->window, *f->background,
			x, 1, font->size, lineheight);
	x += font->size;
	if (f->pixmap != None)
		XCopyArea(dpy, f->pixmap, f->window, foreground, 0, 0,
				namewidth, lineheight, x, 1);
	x += namewidth;
	XFillRectangle(dpy, f->window, *f->background,
			x, 1, f->width - 1 - x, lineheight);

	// Border
	XDrawRectangle(dpy, f->window, foreground,
			0, 0, f->width - 1, f->height - 1);

	// Title bottom border
	XDrawLine(dpy, f->window, foreground, EXT_LEFT, EXT_TOP - 1,
			f->width - EXT_RIGHT - 1, EXT_TOP - 1);

	// Window area
	XFillRectangle(dpy, f->window, *f->background,
			1, EXT_TOP, f->width - 2, f->height - 1 - EXT_TOP);

	// Small areas to the left and right of the title bottom border
	XFillRectangle(dpy, f->window, *f->background,
			1, EXT_TOP - 1, EXT_LEFT - 1, 1);
	XFillRectangle(dpy, f->window, *f->background,
			f->width - EXT_RIGHT, EXT_TOP - 1, EXT_RIGHT - 1, 1);
}

static void confrequest(struct frame *f, XConfigureRequestEvent *e)
{
	struct geometry g = cgetgeom(f->client);

	if (e->value_mask & CWBorderWidth) {
		g.borderwidth = e->border_width;
		csetgeom(f->client, g);
	}

	int dx, dy;
	gravitate(cgetgrav(f->client), g.borderwidth, &dx, &dy);

	int x = f->x;
	int y = f->y;

	// Fetch requested geometry
	if (e->value_mask & CWX)
		x = e->x + dx;
	if (e->value_mask & CWY)
		y = e->y + dy;
	if (e->value_mask & CWWidth)
		g.width = e->width;
	if (e->value_mask & CWHeight)
		g.height = e->height;

	int width = g.width + EXT_LEFT + EXT_RIGHT;
	int height = g.height + EXT_TOP + EXT_BOTTOM;

	moveresize(f, x, y, width, height);
}

static void buttonpress(struct frame *f, XButtonEvent *e)
{
	if (e->button == Button1) {
		cpopapp(f->client);
		cfocus(f->client, e->time);

		if (e->y < EXT_TOP || (e->state & Mod1Mask) != 0) {
			f->grabbed = True;
			csetappfollowdesk(f->client, True);
			f->downx = e->x;
			f->downy = e->y;
			XGrabPointer(dpy, f->window, False,
					Button1MotionMask | ButtonReleaseMask,
					GrabModeAsync, GrabModeAsync, None,
					None, e->time);
		}
	}
}

static void buttonrelease(struct frame *f, XButtonEvent *e)
{
	if (e->button == Button1 && f->grabbed) {
		XUngrabPointer(dpy, e->time);
		csetappfollowdesk(f->client, False);
		f->grabbed = False;
	}
}

/*
 * Move and resize the frame, and update the client window.
 */
static void moveresize(struct frame *f, int x, int y, int w, int h)
{
	if (x == f->x && y == f->y && w == f->width && h == f->height)
		return;

	struct geometry old = cgetgeom(f->client);
	struct geometry new = {
		.x = x + EXT_LEFT,
		.y = y + EXT_TOP,
		.width = w - EXT_LEFT - EXT_RIGHT,
		.height = h - EXT_TOP - EXT_BOTTOM,
		.borderwidth = old.borderwidth,
	};
	csetgeom(f->client, new);

	XMoveResizeWindow(dpy, f->window, x, y, w, h);
	f->x = x;
	f->y = y;
	f->width = w;
	f->height = h;

	if (new.width == old.width && new.height == old.height)
		csendconf(f->client);
	else
		XResizeWindow(dpy, cgetwin(f->client), new.width, new.height);
}

static void motionnotify(struct frame *f, XMotionEvent *e)
{
	moveresize(f, e->x_root - f->downx, e->y_root - f->downy,
			f->width, f->height);
}

static void maprequest(struct frame *f, XMapRequestEvent *e)
{
	Window win = cgetwin(f->client);
	if (e->window == win)
		redirect((XEvent *)e, win);
}

static void expose(struct frame *f, XExposeEvent *e)
{
	if (e->count == 0)
		repaint(f);
}

static void event(void *self, XEvent *e)
{
	switch (e->type) {
	case Expose:
		expose(self, &e->xexpose);
		break;
	case MotionNotify:
		motionnotify(self, &e->xmotion);
		break;
	case ButtonPress:
		buttonpress(self, &e->xbutton);
		break;
	case ButtonRelease:
		buttonrelease(self, &e->xbutton);
		break;
	case ConfigureRequest:
		confrequest(self, &e->xconfigurerequest);
		break;
	case MapRequest:
		maprequest(self, &e->xmaprequest);
		break;
	}
}

struct frame *fcreate(struct client *c)
{
	if (fcount == 0) {
		cursortopleft = XCreateFontCursor(dpy, XC_top_left_corner);
		cursortopright = XCreateFontCursor(dpy, XC_top_right_corner);
	}
	fcount++;

	struct frame *f = xmalloc(sizeof *f);

	f->client = c;
	f->pixmap = None;
	f->namewidth = 0;

	struct geometry g = cgetgeom(c);
	int dx, dy;
	gravitate(cgetgrav(c), g.borderwidth, &dx, &dy);
	f->x = g.x + dx;
	f->y = g.y + dy;
	f->width = g.width + EXT_LEFT + EXT_RIGHT;
	f->height = g.height + EXT_TOP + EXT_BOTTOM;

	f->grabbed = False;

	f->window = XCreateWindow(dpy, root, f->x, f->y, f->width, f->height,
			0, CopyFromParent, InputOutput, CopyFromParent,
			CWBitGravity,
			&(XSetWindowAttributes){
				.bit_gravity = NorthWestGravity });

	Window clientwin = cgetwin(f->client);

	reorder(clientwin, f->window);

	f->listener.function = event;
	f->listener.pointer = f;
	setlistener(f->window, &f->listener);

	XSelectInput(dpy, f->window,
			SubstructureRedirectMask |
			ButtonPressMask |
			ButtonReleaseMask |
			ExposureMask);

	grabbutton(Button1, Mod1Mask, f->window, False, ButtonReleaseMask,
			GrabModeAsync, GrabModeAsync, None, None);

	/*
	 * The order in which the resizers and the delete button
	 * are created is important since it determines their
	 * stacking order. For very small windows it is important
	 * that the right resizer and the delete button are above
	 * the left resizer.
	 */

	int dw = font->size + 1;
	int dh = lineheight + 2;
	f->topleftresizer = dcreate(f->window, 0, 0, dw, dh,
			NorthWestGravity, cursortopleft, resizetopleft, f);
	f->toprightresizer = dcreate(f->window, f->width - dw, 0, dw, dh,
			NorthEastGravity, cursortopright, resizetopright, f);

	f->deletebutton = NULL;

	XSetWindowBorderWidth(dpy, clientwin, 0);
	setgrav(clientwin, NorthWestGravity);
	if (cismapped(f->client))
		cignoreunmap(f->client);
	XReparentWindow(dpy, clientwin, f->window, EXT_LEFT, EXT_TOP);

	g.x += EXT_LEFT;
	g.y += EXT_TOP;
	csetgeom(f->client, g);
	csendconf(f->client);

	ewmh_notifyframeextents(clientwin, (struct extents){
			.top = EXT_TOP,
			.bottom = EXT_BOTTOM,
			.right = EXT_RIGHT,
			.left = EXT_LEFT });

	fupdate(f);

	if (cismapped(f->client))
		XMapWindow(dpy, f->window);
	return f;
}

void fdestroy(struct frame *f)
{
	Bool hadfocus = chasfocus(f->client);

	XUnmapWindow(dpy, f->window);

	struct geometry g = cgetgeom(f->client);
	Window clientwin = cgetwin(f->client);

	XSetWindowBorderWidth(dpy, clientwin, g.borderwidth);
	int grav = cgetgrav(f->client);
	setgrav(clientwin, grav);
	int dx, dy;
	gravitate(grav, g.borderwidth, &dx, &dy);
	if (cismapped(f->client))
		cignoreunmap(f->client);
	g.x = f->x - dx;
	g.y = f->y - dy;
	csetgeom(f->client, g);
	XReparentWindow(dpy, clientwin, root, g.x, g.y);

	ewmh_notifyframeextents(clientwin, (struct extents){
			.top = 0,
			.bottom = 0,
			.left = 0,
			.right = 0 });

	reorder(f->window, clientwin);
	if (hadfocus)
		cfocus(f->client, CurrentTime);
	setlistener(f->window, NULL);
	ddestroy(f->topleftresizer);
	ddestroy(f->toprightresizer);
	if (f->deletebutton != NULL)
		bdestroy(f->deletebutton);
	if (f->pixmap != None)
		XFreePixmap(dpy, f->pixmap);
	XDestroyWindow(dpy, f->window);
	free(f);

	assert(fcount > 0);
	fcount--;
	if (fcount == 0) {
		XFreeCursor(dpy, cursortopleft);
		XFreeCursor(dpy, cursortopright);
	}
}

Window fgetwin(struct frame *f)
{
	return f->window;
}

struct geometry fgetgeom(struct frame *f)
{
	return (struct geometry){
			.x = f->x,
			.y = f->y,
			.width = f->width,
			.height = f->height,
			.borderwidth = 0 };
}

static void delete(void *client, Time t)
{
	cdelete(client, t);
}

static void resizetopleft(void *self, int xdrag, int ydrag,
		unsigned long counter, Time t)
{
	struct frame *f = self;

	int w = f->width - (xdrag - f->x);
	int h = f->height - (ydrag - f->y);

	w -= EXT_LEFT + EXT_RIGHT;
	h -= EXT_TOP + EXT_BOTTOM;
	chintsize(f->client, w, h, &w, &h);
	w += EXT_LEFT + EXT_RIGHT;
	h += EXT_TOP + EXT_BOTTOM;

	int x = f->x + f->width - w;
	int y = f->y + f->height - h;
	if (counter == 0) {
		cpopapp(f->client);
		cfocus(f->client, t);
	}
	moveresize(f, x, y, w, h);
}

static void resizetopright(void *self, int xdrag, int ydrag,
		unsigned long counter, Time t)
{
	struct frame *f = self;

	int w = xdrag + 1 - f->x;
	int h = f->height - (ydrag - f->y);

	w -= EXT_LEFT + EXT_RIGHT;
	h -= EXT_TOP + EXT_BOTTOM;
	chintsize(f->client, w, h, &w, &h);
	w += EXT_LEFT + EXT_RIGHT;
	h += EXT_TOP + EXT_BOTTOM;

	int x = f->x;
	int y = f->y + f->height - h;
	if (counter == 0) {
		cpopapp(f->client);
		cfocus(f->client, t);
	}
	moveresize(f, x, y, w, h);
}
