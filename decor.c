#include <err.h>
#include "shod.h"
#include "decor.h"
#include "util.h"

/* get next utf8 char from s return its codepoint and set next_ret to pointer to end of character */
static FcChar32
getnextutf8char(const char *s, const char **next_ret)
{
	static const unsigned char utfbyte[] = {0x80, 0x00, 0xC0, 0xE0, 0xF0};
	static const unsigned char utfmask[] = {0xC0, 0x80, 0xE0, 0xF0, 0xF8};
	static const FcChar32 utfmin[] = {0, 0x00,  0x80,  0x800,  0x10000};
	static const FcChar32 utfmax[] = {0, 0x7F, 0x7FF, 0xFFFF, 0x10FFFF};
	/* 0xFFFD is the replacement character, used to represent unknown characters */
	static const FcChar32 unknown = 0xFFFD;
	FcChar32 ucode;         /* FcChar32 type holds 32 bits */
	size_t usize = 0;       /* n' of bytes of the utf8 character */
	size_t i;

	*next_ret = s+1;

	/* get code of first byte of utf8 character */
	for (i = 0; i < sizeof utfmask; i++) {
		if (((unsigned char)*s & utfmask[i]) == utfbyte[i]) {
			usize = i;
			ucode = (unsigned char)*s & ~utfmask[i];
			break;
		}
	}

	/* if first byte is a continuation byte or is not allowed, return unknown */
	if (i == sizeof utfmask || usize == 0)
		return unknown;

	/* check the other usize-1 bytes */
	s++;
	for (i = 1; i < usize; i++) {
		*next_ret = s+1;
		/* if byte is nul or is not a continuation byte, return unknown */
		if (*s == '\0' || ((unsigned char)*s & utfmask[0]) != utfbyte[0])
			return unknown;
		/* 6 is the number of relevant bits in the continuation byte */
		ucode = (ucode << 6) | ((unsigned char)*s & ~utfmask[0]);
		s++;
	}

	/* check if ucode is invalid or in utf-16 surrogate halves */
	if (!BETWEEN(ucode, utfmin[usize], utfmax[usize])
	    || BETWEEN (ucode, 0xD800, 0xDFFF))
		return unknown;

	return ucode;
}

/* get which font contains a given code point */
static XftFont *
getfontucode(FcChar32 ucode)
{
	FcCharSet *fccharset = NULL;
	FcPattern *fcpattern = NULL;
	FcPattern *match = NULL;
	XftFont *retfont = NULL;
	XftResult result;
	size_t i;

	for (i = 0; i < dc.nfonts; i++)
		if (XftCharExists(dpy, dc.fonts[i], ucode) == FcTrue)
			return dc.fonts[i];

	/* create a charset containing our code point */
	fccharset = FcCharSetCreate();
	FcCharSetAddChar(fccharset, ucode);

	/* create a pattern akin to the dc.pattern but containing our charset */
	if (fccharset) {
		fcpattern = FcPatternDuplicate(dc.pattern);
		FcPatternAddCharSet(fcpattern, FC_CHARSET, fccharset);
	}

	/* find pattern matching fcpattern */
	if (fcpattern) {
		FcConfigSubstitute(NULL, fcpattern, FcMatchPattern);
		FcDefaultSubstitute(fcpattern);
		match = XftFontMatch(dpy, screen, fcpattern, &result);
	}

	/* if found a pattern, open its font */
	if (match) {
		retfont = XftFontOpenPattern(dpy, match);
		if (retfont && XftCharExists(dpy, retfont, ucode) == FcTrue) {
			if ((dc.fonts = realloc(dc.fonts, dc.nfonts+1)) == NULL)
				err(1, "realloc");
			dc.fonts[dc.nfonts] = retfont;
			return dc.fonts[dc.nfonts++];
		} else {
			XftFontClose(dpy, retfont);
		}
	}

	/* in case no fount was found, return the first one */
	return dc.fonts[0];
}

/* draw text into XftDraw, return width of text glyphs */
static int
drawtext(XftDraw *draw, XftColor *color, int x, int y, unsigned h, const char *text)
{
	int textwidth = 0;

	while (*text) {
		XftFont *currfont;
		XGlyphInfo ext;
		FcChar32 ucode;
		const char *next;
		size_t len;

		ucode = getnextutf8char(text, &next);
		currfont = getfontucode(ucode);

		len = next - text;
		XftTextExtentsUtf8(dpy, currfont, (XftChar8 *)text, len, &ext);
		textwidth += ext.xOff;

		if (draw) {
			int texty;

			texty = y + (h - (currfont->ascent + currfont->descent))/2 + currfont->ascent;
			XftDrawStringUtf8(draw, color, currfont, x, texty, (XftChar8 *)text, len);
			x += ext.xOff;
		}

		text = next;
	}

	return textwidth;
}

Window
decor_createwin(struct Client *c)
{
	XSetWindowAttributes swa;
	Window ret;

	swa.event_mask = EnterWindowMask | SubstructureNotifyMask
	               | ExposureMask | SubstructureRedirectMask
	               | ButtonPressMask | FocusChangeMask;
	swa.background_pixel = config.focused;
	ret = XCreateWindow(dpy, root, c->ux, c->uy, c->uw, c->uh, 0,
	                    CopyFromParent, CopyFromParent, CopyFromParent,
	                    CWEventMask | CWBackPixel, &swa);
	XReparentWindow(dpy, c->win, ret, c->x, c->y);
	return ret;
}

/* add title and border to decoration */
void
decor_borderadd(struct Client *c)
{
	c->border = config.border_width;
	c->x = c->border;
	c->y = c->border;
	if (HASTITLE(c))
		c->y += config.title_height;
}

/* remove title and border from decoration */
void
decor_borderdel(struct Client *c)
{
	c->border = 0;
	c->x = 0;
	c->y = 0;
}

/* draw unfocus decoration */
void
decor_draw(struct Client *c, int state)
{
	Pixmap pm, titlepm; 
	XftDraw *titledraw;
	XftColor *color;
	int w, h;
	int titlex = 0, titlew;

	if (state == DecorFocused)
		color = dc.focused;
	else if (state == DecorUrgent)
		color = dc.urgent;
	else
		color = dc.unfocused;
	getgeom(c, NULL, NULL, &w, &h);
	pm = XCreatePixmap(dpy, root, w, h, DefaultDepth(dpy, screen));
	XSetWindowBackground(dpy, c->dec, color[ColorBG].pixel);
	XSetForeground(dpy, dc.gc, color[ColorBG].pixel);
	XFillRectangle(dpy, pm, dc.gc, 0, 0, w, h);
	if (HASTITLE(c)) {
		titlepm = XCreatePixmap(dpy, root, w - c->border * 2, config.title_height, DefaultDepth(dpy, screen));
		XFillRectangle(dpy, titlepm, dc.gc, 0, 0, w - c->border * 2, config.title_height);
		titledraw = XftDrawCreate(dpy, titlepm, visual, colormap);
		titlew = drawtext(NULL, NULL, 0, 0, 0, c->name);
		if (config.titlealign == TitleCenter)
			titlex = (w - c->border * 2 - titlew) / 2;
		else if (config.titlealign == TitleRight)
			titlex = w - c->border * 2 - titlew - dc.fonts[0]->height;
		else
			titlex = w - c->border * 2 + dc.fonts[0]->height;
		drawtext(titledraw, &(color[ColorFG]), titlex, 0, config.title_height, c->name);
		XCopyArea(dpy, titlepm, pm, dc.gc, 0, 0, w - c->border * 2, config.title_height, c->border, c->border);
		XftDrawDestroy(titledraw);
		XFreePixmap(dpy, titlepm);
	}
	XCopyArea(dpy, pm, c->dec, dc.gc, 0, 0, w, h, 0, 0);
	XFreePixmap(dpy, pm);
}

/* return nonzero if pointer is in title */
int
decor_istitle(struct Client *c, int x, int y)
{
	int w;

	if (config.ignoretitle || c->isfullscreen || !HASTITLE(c))
		return 0;
	getgeom(c, NULL, NULL, &w, NULL);
	if (x >= c->border && x <= w - c->border * 2 &&
	    y >= c->border && y <= c->border + config.title_height)
		return 1;
	return 0;
}

/* return nonzero if pointer is in border */
int
decor_isborder(struct Client *c, int x, int y)
{
	int w, h;

	if (c->isfullscreen)
		return 0;
	getgeom(c, NULL, NULL, &w, &h);
	if (x <= c->border || x >= w - c->border ||
	    y <= c->border || y >= h - c->border)
		return 1;
	return 0;
}

/* add title to decoration */
void
decor_titleadd(struct Client *c)
{
	c->border = config.border_width;
	c->x = c->border;
	c->y = c->border;
	c->y += config.title_height;
}

/* remove title from decoration */
void
decor_titledel(struct Client *c)
{
	c->border = config.border_width;
	c->x = c->border;
	c->y = c->border;
}
