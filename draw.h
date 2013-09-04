#ifndef __DRAW_H__
#define __DRAW_H__

typedef enum {
  ANCHOR_TOP_LEFT,
  ANCHOR_TOP_MIDDLE,
  ANCHOR_TOP_RIGHT,
  ANCHOR_MIDDLE_LEFT,
  ANCHOR_MIDDLE_MIDDLE,
  ANCHOR_MIDDLE_RIGHT,
  ANCHOR_BOTTOM_LEFT,
  ANCHOR_BOTTOM_MIDDLE,
  ANCHOR_BOTTOM_RIGHT
} anchor_enum;

typedef void *draw_ptr;

draw_ptr draw_create(void *canvas);
void draw_destroy(draw_ptr dp);

void draw_start(draw_ptr); 
void draw_finish(draw_ptr); 

void draw_get_canvas_dims(draw_ptr dp, float *width_out, float *height_out);
float draw_get_canvas_width(draw_ptr dp);
float draw_get_canvas_height(draw_ptr dp);

void draw_line(draw_ptr dp, float x1, float y1, float x2, float y2);
void draw_circle_outline(draw_ptr dp, float x_c, float y_c, float radius);
void draw_circle_filled(draw_ptr dp, float x_c, float y_c, float radius);

void draw_rectangle_outline(draw_ptr dp, float x1, float y1, float x2, float y2);
void draw_rectangle_filled(draw_ptr dp, float x1, float y1, float x2, float y2);

void draw_polygon_outline(draw_ptr dp, float *x, float *y, int num_points);
void draw_polygon_filled(draw_ptr dp, float *x, float *y, int num_points);

void draw_set_color(draw_ptr dp, float r, float g, float b);
void draw_set_line_width(draw_ptr dp, float w);

void draw_text(draw_ptr dp, char *text, float font_size, float x, float y, int anchor);

void draw_get_text_dims(draw_ptr dp, char *text, float font_size, float *width_out, float *height_out);
float draw_get_text_width(draw_ptr dp, char *text, float font_size);
float draw_get_text_height(draw_ptr dp, char *text, float font_size);

#endif
