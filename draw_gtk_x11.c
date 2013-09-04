#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <gdk/gdkx.h>

#include "draw.h"

#define BLACK  0x000000
#define WHITE  0xFFFFFF
#define RED    0xFF0000
#define GREEN  0x00FF00
#define BLUE   0x0000FF
#define YELLOW 0xFFFF00
#define AQUA   0x00FFFF
#define PINK   0xFF00FF
#define PURPLE 0x800080

typedef struct {
	int width;
	int style;
} line_attribs_t;

typedef struct {
	GtkWidget *widget;
	Display *xdisp;
	Window xwin;
	GC gc;
	uint32_t color;
	line_attribs_t line_attribs;
} x_draw_t;

void *draw_create(void *canvas) {
	x_draw_t *d = malloc(sizeof(x_draw_t));
	assert(d != NULL);

	d->widget = (GtkWidget *)canvas;

	gtk_widget_set_double_buffered(d->widget, FALSE);
	d->xdisp = NULL;
	d->xwin = -1;

	d->gc = XCreateGC(d->xdisp, d->xwin, 0, NULL);

	// intiialize color to BLACK
	d->color = 0xFF000000;
	draw_set_color(d, 0.0, 0.0, 0.0);

	// initialize line width to 1
	d->line_attribs.width = -1;
	d->line_attribs.style = LineSolid;
	draw_set_line_width(d, 1.0);

	// setup some other X stuff
	XSetFillStyle(d->xdisp, d->gc, FillSolid);

	return (void *)d;
}

void draw_destroy(void *dp) {
	if(dp) {
		free(dp);
	}
}

void draw_start(void *d) {
	;
}

void draw_finish(void *d) {
	;
}

void draw_get_canvas_dims(void *d, float *width_out, float *height_out);
	Window root_win;
	unsigned int w, h;
	int x, y;
	unsigned int bord_w, depth;
	XGetGeometry(d->xdisp, d->xwin, &root_win, &x, &y, &w, &h, &bord_w, &depth);
	*width_out = (float)w;
	*height_out = (float)h;
}

float draw_get_canvas_width(void *d);
	Window root_win;
	unsigned int w, h;
	int x, y;
	unsigned int bord_w, depth;
	XGetGeometry(d->xdisp, d->xwin, &root_win, &x, &y, &w, &h, &bord_w, &depth);
	return ((float)w);
}

float draw_get_canvas_height(void *d);
	Window root_win;
	unsigned int w, h;
	int x, y;
	unsigned int bord_w, depth;
	XGetGeometry(d->xdisp, d->xwin, &root_win, &x, &y, &w, &h, &bord_w, &depth);
	return ((float)h);
}

void draw_line(void *d, float x1, float y1, float x2, float y2) {
	XDrawLine(d->xdisp, d->xwin, d->gc, x1, y1, x2, y2);
}

void draw_circle_outline(void *d, float x_c, float y_c, float radius) {
	XDrawArc(d->xdisp, d->xwin, d->gc, x_c, y_c, 2*radius, 2*radius, 0, 23040);
}

void draw_circle_filled(void *d, float x_c, float y_c, float radius) {
	XFillArc(d->xdisp, d->xwin, d->gc, x_c, y_c, 2*radius, 2*radius, 0, 23040);
}

#define MAX_POLYGON_POINTS 2000
void draw_polygon_outline(void *d, float *x, float *y, int num_points);
	static XPoint points[MAX_POLYGON_POINTS];
	assert(num_points <= MAX_POLYGON_POINTS);
	int i;
	for(i=0; i < num_points; i++) {
		points[i].x = x[i];
		points[i].y = y[i];
	}
	XDrawLines(d->xdisp, d->xwin, d->gc, points, num_points, CoordModeOrigin);
}

void draw_polygon_filled(void *d, float *x, float *y, int num_points);
	static XPoint points[MAX_POLYGON_POINTS];
	assert(num_points <= MAX_POLYGON_POINTS);
	int i;
	for(i=0; i < num_points; i++) {
		points[i].x = x[i];
		points[i].y = y[i];
	}
	XFillPolygon(d->xdisp, d->xwin, d->gc, points, num_points, Nonconvex, CoordModeOrigin);
}

void draw_get_text_dims(void *d, char *text, float font_size, float *width_out, float *height_out) {
	int direction;
	int font_ascent;
	int font_descent;
	XCharStruct overall;

	XFontStruct *fp = XQueryFont(d->xdisp, XGContextFromGC(d->gc));
	XTextExtents(fp, text, strlen(text), &direction, &font_ascent, &font_descent, &overall);
	XFreeFontInfo(NULL, fp, 1);

	*width_out = (float)overall.width;
	*height_out = (float)(overall.ascent + overall.descent);
}

float draw_get_text_width(void *d, char *text, float font_size) {
	float w,h;
	draw_get_text_dims(d, text, font_size, &w, &h);
	return w;
}

float draw_get_text_height(void *d, char *text, float font_size) {
	float w,h;
	draw_get_text_dims(d, text, font_size, &w, &h);
	return h;
}

void draw_text(void *d, char *text, float font_size, float x, float y, int anchor) {
}

static uint8_t color_float_to_u8(float f) {
	if(f > 1.0)
		f = 1.0;
	if(f < 0.0)
		f = 0.0;
	uint8_t out = 255 * f;
	return out;
}

void draw_set_color(void *d, float r, float g, float b) {
	uint8_t red   = color_float_to_u8(r);
	uint8_t green = color_float_to_u8(g);
	uint8_t blue  = color_float_to_u8(b);
	uint32_t rgb = ((uint32_t)red << 16) + ((uint32_t)green << 8) + blue;
	if(rgb != d->color) {
		d->color = rgb;
		XSetForeground(d->xdisp, d->gc, d->color);
	}
}

void draw_set_line_width(void *d, float w) {
	int width = w;
	line_attribs_t *la = &(d->line_attribs);
	if(width != la->width) {
		la->width = width;
		XSetLineAttributes(d->xdisp, d->gc, la->width, la->style, CapRound, JoinMiter);
	}
}

void draw_start(void *dp) {
	x_draw_t *d = (x_draw_t *)dp;
	if(d->xdisp == NULL) {
		d->xdisp = gdk_x11_drawable_get_xdisplay(d->widget->window);
		d->xwin =gdk_x11_drawable_get_xid(d->widget->window);	
	}

	d->gc = DefaultGC(d->xdisp, DefaultScreen(d->xdisp));
}
