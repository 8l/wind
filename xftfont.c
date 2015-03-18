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
#include <X11/Xft/Xft.h>
#include <string.h>

#include "wind.h"

#define DEFAULT "sans-serif:size=10"

struct font *ftload(const char *name)
{
	XftFont  *font = NULL;

	if (name != NULL) {
		font = XftFontOpenXlfd(dpy, scr, name);
		if (font == NULL)
			font = XftFontOpenName(dpy, scr, name);
		if (font == NULL)
			errorf("cannot not load font %s", name);
	}

	if (font == NULL)
		font = XftFontOpenName(dpy, scr, DEFAULT);

	if (font == NULL)
		return NULL;

	struct font *f = xmalloc(sizeof *f);
	f->size = font->ascent + font->descent;
	f->ascent = font->ascent;
	f->descent = font->descent;
	f->data = font;

	return f;
}

struct fontcolor {
	XftColor color;
	XftDraw *draw;
	Visual *visual;
	Colormap colormap;
};

struct fontcolor *ftloadcolor(const char *name)
{
	XftDraw *draw;
	XftColor color;
	Visual *visual = DefaultVisual(dpy, scr);
	Colormap colormap = DefaultColormap(dpy, scr);

	if ((draw = XftDrawCreate(dpy, root, visual, colormap)) == NULL)
		return NULL;

	if (!XftColorAllocName(dpy, visual, colormap, name, &color)) {
		XftDrawDestroy(draw);
		return NULL;
	}

	struct fontcolor *c = xmalloc(sizeof *c);
	c->draw = draw;
	c->color = color;
	c->visual = visual;
	c->colormap = colormap;

	return c;
}

void ftfree(struct font *f)
{
	XftFont *font = f->data;
	XftFontClose(dpy, font);
	free(f);
}

void ftfreecolor(struct fontcolor *fc)
{
	XftColorFree(dpy, fc->visual, fc->colormap, &fc->color);
	XftDrawDestroy(fc->draw);
	free(fc);
}

void ftdrawstring(Drawable d, struct font *f, struct fontcolor *c,
		int x, int y, const char *s)
{
	XftFont *font = f->data;
	XftDrawChange(c->draw, d);
	XftDrawString8(c->draw, &c->color, font, x, y,
			(XftChar8 *)s, strlen(s));
}

void ftdrawstring_utf8(Drawable d, struct font *f, struct fontcolor *c,
		int x, int y, const char *s)
{
	XftFont *font = f->data;
	XftDrawChange(c->draw, d);
	XftDrawStringUtf8(c->draw, &c->color, font, x, y,
			(XftChar8 *)s, strlen(s));
}

int fttextwidth(struct font *f, const char *s)
{
	XftFont *font = f->data;
	XGlyphInfo info;
	XftTextExtents8(dpy, font, (XftChar8 *)s, strlen(s), &info);
	return info.width - info.x; // [sic]
}

int fttextwidth_utf8(struct font *f, const char *s)
{
	XftFont *font = f->data;
	XGlyphInfo info;
	XftTextExtentsUtf8(dpy, font, (XftChar8 *)s, strlen(s), &info);
	return info.width - info.x; // [sic]
}
