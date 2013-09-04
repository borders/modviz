#include <stdio.h>
#include <assert.h>

typdef struct {
	GtkWidget *widget;
	Display *xdisp;
	Window xwin;
} x_draw_t;

draw_ptr draw_create(void *canvas) {
	x_draw_t *d = malloc(sizeof(x_draw_t));
	assert(d);

	d->widget = (GtkWidget *)canvas;
	d->xdisp = gdk_x11_drawable_get_xdisplay(d->widget->window);
	d->xwin =gdk_x11_drawable_get_xid(d->widget->window);	

	return (void *)d;
}
