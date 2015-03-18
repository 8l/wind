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

struct button {
	struct listener listener;
	void (*function)(void *, Time);
	void *arg;
	struct bitmap *bitmap;
	Pixmap pixmap;
	int width;
	int height;
	Window window;
	Bool pressed;
	Bool entered;
};

static void update(struct button *);
static void buttonpress(struct button *, XButtonEvent *);
static void buttonrelease(struct button *, XButtonEvent *);
static void enternotify(struct button *, XCrossingEvent *);
static void leavenotify(struct button *, XCrossingEvent *);
static void expose(struct button *, XExposeEvent *);
static void unmapnotify(struct button *, XUnmapEvent *);
static void event(void *, XEvent *);

static void update(struct button *b)
{
	Bool invert = b->pressed && b->entered;
	GC fg = invert ? background : foreground;
	GC bg = invert ? foreground : background;

	XFillRectangle(dpy, b->pixmap, bg, 0, 0, b->width, b->height);

	drawbitmap(b->pixmap, fg, b->bitmap,
			(b->width - b->bitmap->width) / 2,
			(b->height - b->bitmap->height) / 2);

	if (!invert) {
		XSetLineAttributes(dpy, fg,
				b->entered ? 1 + 2 * halfleading : 0,
				LineSolid, CapButt, JoinMiter);
		XDrawRectangle(dpy, b->pixmap, fg,
				0, 0, b->width - 1, b->height - 1);
		XSetLineAttributes(dpy, fg, 0, LineSolid, CapButt, JoinMiter);
	}

	XCopyArea(dpy, b->pixmap, b->window, fg,
			0, 0, b->width, b->height, 0, 0);
}

static void buttonpress(struct button *b, XButtonEvent *e)
{
	b->pressed = True;
	update(b);
}

static void buttonrelease(struct button *b, XButtonEvent *e)
{
	if (e->button == Button1) {
		if (b->pressed && b->entered)
			b->function(b->arg, e->time);
		b->pressed = False;
		update(b);
	}
}

static void enternotify(struct button *b, XCrossingEvent *e)
{
	b->entered = True;
	update(b);
}

static void leavenotify(struct button *b, XCrossingEvent *e)
{
	if (b->entered) {
		b->entered = False;
		update(b);
	}
}

static void unmapnotify(struct button *b, XUnmapEvent *e)
{
	if (b->pressed) {
		b->pressed = False;
		update(b);
	}
}

static void expose(struct button *b, XExposeEvent *e)
{
	XCopyArea(dpy, b->pixmap, b->window, foreground,
			e->x, e->y, e->width, e->height, e->x, e->y);
}

static void event(void *self, XEvent *e)
{
	switch (e->type) {
	case Expose:
		expose(self, &e->xexpose);
		break;
	case EnterNotify:
		enternotify(self, &e->xcrossing);
		break;
	case LeaveNotify:
		leavenotify(self, &e->xcrossing);
		break;
	case ButtonPress:
		buttonpress(self, &e->xbutton);
		break;
	case ButtonRelease:
		buttonrelease(self, &e->xbutton);
		break;
	case UnmapNotify:
		unmapnotify(self, &e->xunmap);
		break;
	}
}

struct button *bcreate(void (*function)(void *, Time),
		void *arg, struct bitmap *bitmap,
		Window parent, int x, int y, int width,
		int height, int gravity)
{
	struct button *b = xmalloc(sizeof *b);
	b->function = function;
	b->arg = arg;
	b->bitmap = bitmap;
	b->width = width;
	b->height = height;
	b->pixmap = XCreatePixmap(dpy, root, width, height,
			DefaultDepth(dpy, scr));
	b->pressed = False;
	b->entered = False;
	b->window = XCreateWindow(dpy, parent, x, y, width, height, 0,
			CopyFromParent, InputOutput, CopyFromParent,
			CWWinGravity,
			&(XSetWindowAttributes){
				.win_gravity = gravity });
	b->listener.function = event;
	b->listener.pointer = b;
	setlistener(b->window, &b->listener);
	XGrabButton(dpy, Button1, AnyModifier, b->window, False,
			EnterWindowMask | LeaveWindowMask | ButtonReleaseMask,
			GrabModeAsync, GrabModeAsync, None, None);
	XSelectInput(dpy, b->window,
			EnterWindowMask | LeaveWindowMask |
			StructureNotifyMask | ExposureMask);
	update(b);
	XMapWindow(dpy, b->window);
	return b;
}

void bdestroy(struct button *b)
{
	setlistener(b->window, NULL);
	XFreePixmap(dpy, b->pixmap);
	XDestroyWindow(dpy, b->window);
	free(b);
}
