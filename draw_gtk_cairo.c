#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <math.h>

#include <gtk/gtk.h>

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
	GtkWidget *widget;
	cairo_t *cr;
} cairo_draw_t;

void *draw_create(void *canvas) {
	cairo_draw_t *d = malloc(sizeof(cairo_draw_t));
	assert(d != NULL);

	d->widget = (GtkWidget *)canvas;

	return (void *)d;
}

void draw_destroy(void *dp) {
	free(dp);
}

void draw_start(void *dp) {
	cairo_draw_t *d = (cairo_draw_t *)dp;
	d->cr = gdk_cairo_create(d->widget->window);

}

void draw_finish(void *dp) {
	cairo_draw_t *d = (cairo_draw_t *)dp;
	cairo_destroy(d->cr);
}

void draw_get_canvas_dims(void *dp, float *width_out, float *height_out) {
	cairo_draw_t *d = (cairo_draw_t *)dp;
	*width_out = (float)d->widget->allocation.width;
	*height_out = (float)d->widget->allocation.height;
}

float draw_get_canvas_width(void *dp) {
	cairo_draw_t *d = (cairo_draw_t *)dp;
	return ((float)d->widget->allocation.width);
}

float draw_get_canvas_height(void *dp) {
	cairo_draw_t *d = (cairo_draw_t *)dp;
	return ((float)d->widget->allocation.height);
}

void draw_line(void *dp, float x1, float y1, float x2, float y2) {
	cairo_draw_t *d = (cairo_draw_t *)dp;

	cairo_move_to(d->cr, x1, y1);
	cairo_line_to(d->cr, x2, y2);
	cairo_stroke(d->cr);
}


void draw_rectangle_filled(void *dp, float x1, float y1, float x2, float y2) {
	cairo_draw_t *d = (cairo_draw_t *)dp;

	float x_left, width;
	float y_upper, height;
	if(x1 < x2) {
		x_left = x1;
		width = x2 - x1;
	}
	else {
		x_left = x2;
		width = x1 - x2;
	}
	if(y1 < y2) {
		y_upper = y1;
		height = y2 - y1;
	}
	else {
		y_upper = y2;
		height = y1 - y2;
	}
	
	cairo_rectangle(d->cr, x_left, y_upper, width, height);
	cairo_fill(d->cr);
}

void draw_rectangle_outline(void *dp, float x1, float y1, float x2, float y2) {
	cairo_draw_t *d = (cairo_draw_t *)dp;

	float x_left, width;
	float y_upper, height;
	if(x1 < x2) {
		x_left = x1;
		width = x2 - x1;
	}
	else {
		x_left = x2;
		width = x1 - x2;
	}
	if(y1 < y2) {
		y_upper = y1;
		height = y2 - y1;
	}
	else {
		y_upper = y2;
		height = y1 - y2;
	}
	
	cairo_rectangle(d->cr, x_left, y_upper, width, height);
	cairo_stroke(d->cr);
}

void draw_circle_outline(void *dp, float x_c, float y_c, float radius) {
	cairo_draw_t *d = (cairo_draw_t *)dp;
	cairo_arc(d->cr, x_c, y_c, radius, 0, 2*M_PI);
	cairo_stroke(d->cr);
}

void draw_circle_filled(void *dp, float x_c, float y_c, float radius) {
	cairo_draw_t *d = (cairo_draw_t *)dp;
	cairo_arc(d->cr, x_c, y_c, radius, 0, 2*M_PI);
	cairo_fill(d->cr);
}

#define MAX_POLYGON_POINTS 2000
void draw_polygon_outline(void *dp, float *x, float *y, int num_points) {
	cairo_draw_t *d = (cairo_draw_t *)dp;
	assert(num_points > 1);

	int i;
	cairo_move_to(d->cr, x[0], y[0]);
	for(i=1; i < num_points; i++) {
		cairo_line_to(d->cr, x[i], y[i]);
	}
	cairo_stroke(d->cr);
}

void draw_polygon_filled(void *dp, float *x, float *y, int num_points) {
	cairo_draw_t *d = (cairo_draw_t *)dp;
	assert(num_points > 1);

	int i;
	cairo_move_to(d->cr, x[0], y[0]);
	for(i=1; i < num_points; i++) {
		cairo_line_to(d->cr, x[i], y[i]);
	}
	cairo_fill(d->cr);
}

void draw_get_text_dims(void *dp, char *text, float font_size, float *width_out, float *height_out) {
	cairo_draw_t *d = (cairo_draw_t *)dp;
   cairo_text_extents_t te;
   cairo_save(d->cr);
   cairo_set_font_size(d->cr, font_size);
   cairo_text_extents(d->cr, text, &te);
   cairo_restore(d->cr);

	*width_out = (float)te.width;
	*height_out = (float)te.height;
}

float draw_get_text_width(void *dp, char *text, float font_size) {
	cairo_draw_t *d = (cairo_draw_t *)dp;
	float w,h;
	draw_get_text_dims(d, text, font_size, &w, &h);
	return w;
}

float draw_get_text_height(void *dp, char *text, float font_size) {
	cairo_draw_t *d = (cairo_draw_t *)dp;
	float w,h;
	draw_get_text_dims(d, text, font_size, &w, &h);
	return h;
}

void draw_text(void *dp, char *text, float font_size, float x, float y, int anchor) {
	cairo_draw_t *d = (cairo_draw_t *)dp;

	double x_left, y_bottom;
	cairo_text_extents_t te;
	double w, h;

	cairo_save(d->cr);

	cairo_text_extents(d->cr, text, &te);
	w = te.width;
	h = te.height;
	switch(anchor) {
		case ANCHOR_TOP_LEFT:
			x_left = x;
			y_bottom = y + h;
			break;
		case ANCHOR_TOP_MIDDLE:
			x_left = x - w/2;
			y_bottom = y + h;
			break;
		case ANCHOR_TOP_RIGHT:
			x_left = x - w;
			y_bottom = y + h;
			break;
		case ANCHOR_MIDDLE_LEFT:
			x_left = x;
			y_bottom = y + h/2;
			break;
		case ANCHOR_MIDDLE_MIDDLE:
			x_left = x - w/2;
			y_bottom = y + h/2;
			break;
		case ANCHOR_MIDDLE_RIGHT:
			x_left = x - w;
			y_bottom = y + h/2;
			break;
		case ANCHOR_BOTTOM_LEFT:
			x_left = x;
			y_bottom = y;
			break;
		case ANCHOR_BOTTOM_MIDDLE:
			x_left = x - w/2;
			y_bottom = y;
			break;
		case ANCHOR_BOTTOM_RIGHT:
			x_left = x - w;
			y_bottom = y;
      	break;
		default:
			x_left = x;
			y_bottom = y;
	}
	cairo_move_to(d->cr, x_left, y_bottom);
	cairo_show_text(d->cr, text);
	cairo_restore(d->cr);
}

static uint8_t color_float_to_u8(float f) {
	if(f > 1.0)
		f = 1.0;
	if(f < 0.0)
		f = 0.0;
	uint8_t out = 255 * f;
	return out;
}

void draw_set_color(void *dp, float r, float g, float b) {
	cairo_draw_t *d = (cairo_draw_t *)dp;
	cairo_set_source_rgb (d->cr, r, g, b);
}

void draw_set_line_width(void *dp, float w) {
	cairo_draw_t *d = (cairo_draw_t *)dp;
	cairo_set_line_width(d->cr, w);
}

