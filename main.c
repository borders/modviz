#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
	BODY_TYPE_BALL,
	BODY_TYPE_BLOCK,
	BODY_TYPE_CUSTOM
} body_type_enum;

typedef struct {
	double red;
	double green;
	double blue;
} color_t;

static color_t COLOR_BLACK = {0.0, 0.0, 0.0};

typedef struct {
	body_type_enum type;
	double x;
	double y;
	double theta;

	char *name; // user-specified name string
	int id; // user-specified id number

	bool show_cs;
	bool show_name;
	bool show_id;
	bool filled;
	color_t color;
} body_t;

typedef struct {
	body_t body;
	double radius;
} body_ball_t;

typedef struct {
	body_t body;
	double x1;
	double x2;
	double y1;
	double y2;
} body_block_t;

typedef struct {
	double x;
	double y;
} node_t;

typedef struct {
	body_t body;
	int node_count;
	node_t *nodes;
} body_custom_t;

typedef enum {
	CONN_VISUAL_LINE,
	CONN_VISUAL_SPRING
} conn_visual_enum;

typedef struct {
	conn_visual_enum visual;

	body_t *body_1; // first body to attach to
	double x1;      // (x,y) position of body_1 to attach to 
	double y1;

	body_t *body_2; // second body to attach to
	double x2;      // (x,y) position of body_2 to attach to 
	double y2;

	double thickness;
	color_t color;
	bool show_name;
	bool show_id;
} connector_t;

int body_init(body_t *self, body_type_enum type) {
	self->type = type;

	self->x = 0.0;
	self->y = 0.0;
	self->theta = 0.0;

	self->name = NULL;
	self->id = -1;

	self->show_cs = false;
	self->show_name = false;
	self->show_id = false;
	self->filled = true;
	self->color = COLOR_BLACK;
	return 0;
}

int body_ball_init(body_ball_t *self) {
	body_init((body_t *)self, BODY_TYPE_BALL);
	self->radius = 1.0;
}

int body_block_init(body_block_t *self) {
	body_init((body_t *)self, BODY_TYPE_BLOCK);
	self->x1 = -1.0;
	self->x2 = +1.0;
	self->y1 = -1.0;
	self->y2 = +1.0;
}

int body_set_name(body_t *self, char *name) {
	if(self->name != NULL) {
		free(self->name);
		self->name = NULL;
	}
	if(name == NULL) {
		return 0;
	}
	self->name = malloc(strlen(name)+1);
	if(self->name == NULL) {
		return -1;
	}
	strcpy(self->name, name);
	return 0;
}

int main(int argc, char *argv[]) {

	body_ball_t ball_1;
	body_ball_init(&ball_1);
	body_set_name((body_t *)&ball_1, "hello_world");

	printf("Hello World\n");
	printf("ball_1's name is: \"%s\"\n", ball_1.body.name);
	printf("ball_1's name is: \"%s\"\n", ((body_t *)&ball_1)->name);
	return 0;
}

