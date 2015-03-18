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

#include <X11/Xlib.h>
#include <stdlib.h>
#include <string.h>

#include "wind.h"

#define DEFAULT "-*-helvetica-medium-r-*-*-12-*-*-*-*-*-*-*"

struct fontcolor {
	GC gc;
};

struct font *ftload(const char *name)
{
	XFontSet fontset = NULL;
	char **missingcharsetlist = NULL;
	int missingcharsetcount = 0;
	char *defstring;

	if (name != NULL) {
		fontset = XCreateFontSet(dpy, name, &missingcharsetlist,
				&missingcharsetcount, &defstring);
		if (fontset == NULL)
			errorf("cannot not load fontset %s", name);
	}

	if (fontset == NULL)
		fontset = XCreateFontSet(dpy, DEFAULT, &missingcharsetlist,
				&missingcharsetcount, &defstring);

	if (fontset == NULL)
		fontset = XCreateFontSet(dpy, "fixed", &missingcharsetlist,
				&missingcharsetcount, &defstring);

	if (missingcharsetlist != NULL)
		XFreeStringList(missingcharsetlist);
 
	if (fontset == NULL)
		return NULL;

	XFontSetExtents *extents = XExtentsOfFontSet(fontset);

	struct font *f = xmalloc(sizeof *f);
	f->data = fontset;
	f->ascent = -extents->max_logical_extent.y;
	f->descent = extents->max_logical_extent.height - f->ascent;
	f->size = f->ascent + f->descent;
	return f;
}

void ftfree(struct font *f)
{
	XFreeFontSet(dpy, f->data);
	free(f);
}

struct fontcolor *ftloadcolor(const char *name)
{
	unsigned long pixel = getpixel(name);
	struct fontcolor *c = xmalloc(sizeof *c);
	c->gc = XCreateGC(dpy, root, GCForeground,
			&(XGCValues){ .foreground = pixel });
	return c;
}

void ftfreecolor(struct fontcolor *c)
{
	XFreeGC(dpy, c->gc);
	free(c);
}

void ftdrawstring(Drawable d, struct font *f, struct fontcolor *c,
		int x, int y, const char *s)
{
	XFontSet fontset = f->data;
	XmbDrawString(dpy, d, fontset, c->gc, x, y, s, strlen(s));
}

void ftdrawstring_utf8(Drawable d, struct font *f, struct fontcolor *c,
		int x, int y, const char *s)
{
#ifdef X_HAVE_UTF8_STRING
	XFontSet fontset = f->data;
	Xutf8DrawString(dpy, d, fontset, c->gc, x, y, s, strlen(s));
#else
	// This is not correct, but might be better than doing nothing.
	ftdrawstring(d, f, c, x, y, s);
#endif
}

int fttextwidth(struct font *f, const char *s)
{
	XRectangle r = { .x = 0, .y = 0, .width = 0, .height = 0 };
	XFontSet fontset = f->data;
	XmbTextExtents(fontset, s, strlen(s), &r, NULL);
	return r.x + r.width;
}

int fttextwidth_utf8(struct font *f, const char *s)
{
#ifdef X_HAVE_UTF8_STRING
	XRectangle r = { .x = 0, .y = 0, .width = 0, .height = 0 };
	XFontSet fontset = f->data;
	Xutf8TextExtents(fontset, s, strlen(s), &r, NULL);
	return r.x + r.width;
#else
	// This is not correct, but might be better than doing nothing.
	return fttextwidth(f, s);
#endif
}
