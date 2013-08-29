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
} ball_t;

typedef struct {
	body_t body;
	double x1;
	double x2;
	double y1;
	double y2;
} block_t;


typedef struct {
	body_t body;
	int node_count;
	double *node_x;
	double *node_y;
	bool node_owner;
} custom_t;

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

ball_t *ball_alloc(void) {
	ball_t *ball;
	ball = malloc(sizeof(ball_t));
	if(ball == NULL) {
		fprintf(stderr, "Error allocating ball_t!\n");
		exit(-1);
	}
	return ball;
}

int ball_init(ball_t *self) {
	body_init((body_t *)self, BODY_TYPE_BALL);
	self->radius = 1.0;
}


block_t *block_alloc(void) {
	block_t *block;
	block = malloc(sizeof(block_t));
	if(block == NULL) {
		fprintf(stderr, "Error allocating block_t!\n");
		exit(-1);
	}
	return block;
}

int block_init(block_t *self) {
	body_init((body_t *)self, BODY_TYPE_BLOCK);
	self->x1 = -1.0;
	self->x2 = +1.0;
	self->y1 = -1.0;
	self->y2 = +1.0;
}

custom_t *custom_alloc(void) {
	custom_t *cust;
	cust = malloc(sizeof(custom_t));
	if(cust == NULL) {
		fprintf(stderr, "Error allocating custom_t!\n");
		exit(-1);
	}
	return cust;
}

int custom_init(custom_t *self) {
	body_init((body_t *)self, BODY_TYPE_CUSTOM);
	self->node_count = 0;
	self->node_x = NULL;
	self->node_y = NULL;
	self->node_owner = false;
}

int custom_set_nodes(custom_t *self, int node_count, double *x, double *y, bool copy) {
	if(self->node_owner) {
		if(self->node_x != NULL) {
			free(self->node_x);
		}
		if(self->node_y != NULL) {
			free(self->node_y);
		}
		self->node_owner = false;
		self->node_count = 0;
	}
	if(copy) {
		self->node_x = malloc(node_count * sizeof(self->node_x[0]));
		self->node_y = malloc(node_count * sizeof(self->node_y[0]));
		if(self->node_x == NULL || self->node_y == NULL) {
			fprintf(stderr, "Error allocating nodes!\n");
			exit(-1);
		}
		int i;
		for(i=0; i<node_count; i++) {
			self->node_x[i] = x[i];
			self->node_y[i] = y[i];
		}
	}
	else {
		self->node_x = x;
		self->node_y = y;
	}
	self->node_count = node_count;
	return 0;
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

int validate_args(int argc, char *argv[]) {
	int i;
	if(argc == 1) {
		return 0;
	}
	for(i=1; i < argc; i++) {
		FILE *fp;
		// dash (-) represents stdin -> skip it
		if(!strcmp(argv[i], "-")) {
			continue;
		}
		fp = fopen(argv[i], "r");
		if(fp == NULL) {
			fprintf(stderr, "Unable to read from input file: %s\n", argv[i]);
			return -1;
		}
		fclose(fp);
	}
	return 0;
}

int main(int argc, char *argv[]) {

	ball_t ball_1;
	ball_init(&ball_1);
	body_set_name((body_t *)&ball_1, "hello_world");

	custom_t *pcust_1;
	pcust_1 = custom_alloc();
	custom_init(pcust_1);
	body_set_name((body_t *)pcust_1, "custom_1");

	printf("Hello World\n");
	printf("ball_1's name is: \"%s\"\n", ball_1.body.name);
	printf("ball_1's name is: \"%s\"\n", ((body_t *)&ball_1)->name);
	printf("cust_1's name is: \"%s\"\n", ((body_t *)pcust_1)->name);

	if(validate_args(argc, argv)) {
		return -1;
	}

	return 0;
}

