#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum {
	BODY_BALL,
	BODY_BLOCK,
	BODY_CUSTOM
} body_type_enum;

typedef struct {
	double red;
	double green;
	double blue;
} color_t;

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

int main(int argc, char *argv[]) {
	printf("Hello World\n");
	return 0;
}

