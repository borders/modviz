#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <ctype.h>

#include <gtk/gtk.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "draw.h"
#include "cmdline.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#define MAX_BODIES     500
#define MAX_CONNECTORS 500
#define MAX_INPUT_MAPS 100
#define MAX_GROUNDS 100

#define INIT_FRAMES_CAPACITY 1000

#define PRINT_DEBUG 1
#define PRINT_DEBUG2 1
#define PRINT_WARNINGS 1
#define PRINT_ERRORS 1


#if PRINT_DEBUG
	#define DEBUG(...) printf(__VA_ARGS__)
#else
	#define DEBUG(...) 
#endif

#if PRINT_DEBUG2
	#define DEBUG2(...) printf(__VA_ARGS__)
#else
	#define DEBUG2(...) 
#endif

#if PRINT_WARNINGS
	#define WARNING(...) fprintf(stdout, __VA_ARGS__)
#else
	#define WARNING(...) 
#endif

#if PRINT_ERRORS
	#define ERROR(...) fprintf(stderr, __VA_ARGS__)
#else
	#define ERROR(...) 
#endif

typedef enum {
	BODY_TYPE_BALL,
	BODY_TYPE_BLOCK,
	BODY_TYPE_POLYGON
} body_type_enum;

typedef struct {
	float red;
	float green;
	float blue;
} color_t;

static color_t COLOR_BLACK = {0.0, 0.0, 0.0};
static color_t COLOR_WHITE = {1.0, 1.0, 1.0};
static color_t COLOR_RED   = {1.0, 0.0, 0.0};
static color_t COLOR_GREEN = {0.0, 1.0, 0.0};
static color_t COLOR_BLUE  = {0.0, 0.0, 1.0};


typedef struct _transform_t {
	double x_offset;
	double y_offset;
	double A[2][2];
} transform_t;

typedef struct _body_t {
	body_type_enum type;
	double x;
	double y;
	double theta;
	struct _body_t *xy_parent;
	struct _body_t *theta_parent;

	double x_offset;
	double y_offset;
	double theta_offset;
	double phi;

	char *name; // user-specified name string
	int id; // user-specified id number

	bool show_shape_frame;
	bool show_body_frame;
	bool show_name;
	bool show_id;
	bool filled;
	double line_width;
	color_t color;

	transform_t trans_shape_to_gnd;
} body_t;

void transform_point(transform_t *t, double x, double y, double *x_out, double *y_out) {
	*x_out = t->x_offset + t->A[0][0] * x + t->A[0][1] * y;
	*y_out = t->y_offset + t->A[1][0] * x + t->A[1][1] * y;
}

void transform_points(transform_t *t, int count, double *x, double *y, double *x_out, double *y_out) {
	int i;
	for(i=0; i<count; i++) {
		double tmp_x, tmp_y;
		transform_point(t, x[i], y[i], &tmp_x, &tmp_y);
		x_out[i] = tmp_x;
		y_out[i] = tmp_y;
	}
}

void transform_make(transform_t *t, double x_offset, double y_offset, double theta) {
	double c,s;
	t->x_offset = x_offset;
	t->y_offset = y_offset;
	c = cos(theta);
	s = sin(theta);
	t->A[0][0] = +c;
	t->A[0][1] = -s;
	t->A[1][0] = +s;
	t->A[1][1] = +c;
} 

void transform_append(transform_t *t, transform_t *new) {
	double x_offset, y_offset;
	x_offset = new->x_offset + new->A[0][0] * t->x_offset + new->A[0][1] * t->y_offset;
	y_offset = new->y_offset + new->A[1][0] * t->x_offset + new->A[1][1] * t->y_offset;

	// The above 2 lines should be equivalent to the following line...
	//transform_point(new, t->x_offset, t->y_offset, &x_offset, &y_offset);

	t->x_offset = x_offset;
	t->y_offset = y_offset;

	double A[2][2];
	A[0][0] = new->A[0][0] * t->A[0][0] + new->A[0][1] * t->A[1][0];
	A[0][1] = new->A[0][0] * t->A[0][1] + new->A[0][1] * t->A[1][1];
	A[1][0] = new->A[1][0] * t->A[0][0] + new->A[1][1] * t->A[1][0];
	A[1][1] = new->A[1][0] * t->A[0][1] + new->A[1][1] * t->A[1][1];

	t->A[0][0] = A[0][0];
	t->A[1][0] = A[1][0];
	t->A[0][1] = A[0][1];
	t->A[1][1] = A[1][1];
}

typedef struct {
	body_t body;
	double radius;
} ball_t;

typedef struct {
	body_t body;
	double width;
	double height;
} block_t;


typedef struct {
	body_t body;
	int node_count;
	double *node_x;
	double *node_y;
	bool node_owner;
} polygon_t;

typedef enum {
	CONN_TYPE_LINE,
	CONN_TYPE_SPRING
} conn_type_enum;

typedef struct {
	conn_type_enum type;

	body_t *body_1; // first body to attach to
	double x1;      // (x,y) position of body_1 to attach to 
	double y1;

	body_t *body_2; // second body to attach to
	double x2;      // (x,y) position of body_2 to attach to 
	double y2;

	char *name;
	int id;
	double thickness;
	color_t color;
	bool show_name;
	bool show_id;
} connector_t;

typedef enum {
	GND_TYPE_LINE,
	GND_TYPE_HASH,
	GND_TYPE_PIN
} ground_type_enum;

typedef struct {
	ground_type_enum type;
	int id;
	double x1;
	double y1;
	double x2;
	double y2;
} ground_t;

typedef enum {
	DATA_TYPE_DOUBLE
} data_type_enum;

typedef struct _input_map_t {
	int field_num; // 1-based
	void *dest; // where to write the value (a field of a body_t)
	data_type_enum data_type;
	int frame_byte_offset;
} input_map_t;

typedef char *frame_ptr_t;

frame_ptr_t frame_alloc(int size) {
	frame_ptr_t fp;
	fp = malloc(size);
	if(fp == NULL) {
		ERROR("Error allocating frame\n");
		exit(-1);
	}
	return fp;
}

typedef struct {
	GtkWidget *canvas;
	GtkWidget *slider;
	draw_ptr drawer;
	GtkWidget *playback_state;
	GtkWidget *time;
} gui_t;

typedef struct {
	double min;
	double max;
} range_t;

typedef struct _app_data_t {
	body_t *bodies[MAX_BODIES];
	int num_bodies;

	connector_t *connectors[MAX_CONNECTORS];
	int num_connectors;

	ground_t *grounds[MAX_GROUNDS];
	int num_grounds;

	input_map_t *input_maps[MAX_INPUT_MAPS];
	int num_input_maps;

	frame_ptr_t *frames;	
	int num_frames;
	int frames_capacity;
	int bytes_per_frame;

	bool paused;
	
	double time;
	bool explicit_time;
	int time_map_index;
	double t_min;
	double t_max;
	double dt;

	gui_t gui;

	int active_frame_index;

	range_t x_range;
	range_t y_range;

} app_data_t;

static app_data_t app_data;

void app_data_init(app_data_t *d) {
	d->num_bodies = 0;
	d->num_connectors = 0;
	d->num_grounds = 0;
	d->num_input_maps = 0;

	d->frames = malloc(INIT_FRAMES_CAPACITY * sizeof(frame_ptr_t));
	if(d->frames == NULL) {
		ERROR("Error allocating frames\n");
		exit(-1);
	}
	d->frames_capacity = INIT_FRAMES_CAPACITY;
	d->num_frames = 0;
	d->bytes_per_frame = 0;

	d->time = 0.0;
	d->explicit_time = false;
	d->time_map_index = -1;
	d->dt = 1.0;
	d->paused = false;

	d->x_range.min = -10.0;
	d->x_range.max = +10.0;
	d->y_range.min = -10.0;
	d->y_range.max = +10.0;
}

int body_init(body_t *self, body_type_enum type) {
	self->type = type;

	self->x = 0.0;
	self->y = 0.0;
	self->theta = 0.0;
	self->xy_parent = NULL;
	self->theta_parent = NULL;

	self->x_offset = 0.0;
	self->y_offset = 0.0;
	self->theta_offset = 0.0;
	self->phi = 0.0;

	self->name = NULL;
	self->id = -1;

	self->show_shape_frame = false;
	self->show_body_frame = false;
	self->show_name = false;
	self->show_id = false;
	self->filled = true;
	self->line_width = 1.0;
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

void polygon_dealloc(polygon_t *self) {
	if(self) {
		free(self);
	}
}

void block_dealloc(block_t *self) {
	if(self) {
		free(self);
	}
}

void ball_dealloc(ball_t *self) {
	if(self) {
		free(self);
	}
}

int ball_init(ball_t *self) {
	body_init((body_t *)self, BODY_TYPE_BALL);
	self->radius = 1.0;
	return 0;
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
	self->width = 1.0;
	self->height = 1.0;
	return 0;
}

polygon_t *polygon_alloc(void) {
	polygon_t *poly;
	poly = malloc(sizeof(polygon_t));
	if(poly == NULL) {
		fprintf(stderr, "Error allocating polygon_t!\n");
		exit(-1);
	}
	return poly;
}

void connector_dealloc(connector_t *self) {
	if(self) {
		free(self);
	}
}

connector_t *connector_alloc(void) {
	connector_t *self;
	self = malloc(sizeof(connector_t));
	if(self == NULL) {
		fprintf(stderr, "Error allocating connector_t!\n");
		exit(-1);
	}
	return self;
}

void ground_dealloc(ground_t *self) {
	if(self) {
		free(self);
	}
}

ground_t *ground_alloc(void) {
	ground_t *self;
	self = malloc(sizeof(ground_t));
	if(self == NULL) {
		fprintf(stderr, "Error allocating ground_t!\n");
		exit(-1);
	}
	return self;
}

void ground_init(ground_t *self) {
	self->type = GND_TYPE_HASH;
	self->x1 = 0.0;
	self->y1 = 0.0;
	self->x2 = 1.0;
	self->y2 = 0.0;
}

int connector_init(connector_t *self) {
	self->type = CONN_TYPE_LINE;
	self->body_1 = NULL;
	self->body_2 = NULL;
	self->x1 = 0.0;
	self->y1 = 0.0;
	self->x2 = 0.0;
	self->y2 = 0.0;
	self->name = "";
	self->id = -1;
	return 0;
}

int polygon_init(polygon_t *self) 
{
	body_init((body_t *)self, BODY_TYPE_POLYGON);
	self->node_count = 0;
	self->node_x = NULL;
	self->node_y = NULL;
	self->node_owner = false;
	return 0;
}

static int body_auto_id(void) 
{
	int i;
	int id;
	for(id=1; ; id++) {
		bool used = false;
		for(i=0; i<app_data.num_bodies; i++) {
			if(app_data.bodies[i]->id == id) {
				used = true;
				break;
			}
		}
		if(!used) {
			return id;
		}
	}
}

int polygon_set_nodes(polygon_t *self, int node_count, double *x, double *y, bool copy) {
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

static int load_config(const char *filename, xmlDocPtr *doc_out) {
	xmlDocPtr doc; /* the resulting document tree */

	*doc_out = NULL;

	/* parse the file, activating the DTD validation option */
	doc = xmlReadFile(filename, NULL, 0);
	/* check if parsing suceeded */
	if(doc == NULL) {
		fprintf(stderr, "Failed to parse XML config file: %s\n", filename);
		return -1;
	}
	/* free up the resulting document */
	//xmlFreeDoc(doc);
	*doc_out = doc;
	return 0;
}

int open_file_nonblocking(char *fname) {
	int fd;
	if(!strcmp(fname, "-")) { // dash denotes STDIN
		// STDIN is already open - just make it nonblocking
		fd = 0; // STDIN = 0
		int flags;
		flags = fcntl(fd, F_GETFL, 0);
   	flags |= O_NONBLOCK;
   	fcntl(fd, F_SETFL, flags);
	}
	else { // regular filename (just open it in nonblock mode)
		fd = open(fname, O_RDONLY | O_NONBLOCK);
		if(fd == -1) {
			fprintf(stderr, "Error opening file: %s\n", fname);
			exit(-1);
		}
	}
	return fd;
}

static void print_element_names(xmlNode * a_node, int level) {
	xmlNode *cur_node = NULL;
	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		if (cur_node->type == XML_ELEMENT_NODE) {
			int i;
			for(i=0; i<level; i++) {
				printf("   ");
			}
			printf("node type: Element, name: %s\n", cur_node->name);
		}
		print_element_names(cur_node->children, level+1);
	}
}

static void print_all_attribs(xmlNode * node, int level) {
	struct _xmlAttr *cur_attr;
	for (cur_attr = node->properties; cur_attr != NULL; cur_attr = cur_attr->next) {
		int i;
		for(i=0; i<level; i++) {
			printf("   ");
		}
		printf("  Attribute:: %s = %s\n", cur_attr->name, xmlGetProp(node, cur_attr->name));
	}
}

static void print_all_nodes(xmlNode * a_node, int level) {
	xmlNode *cur_node = NULL;
	for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
		int i;
		if(cur_node->type == XML_TEXT_NODE) {
			continue;
		}
		for(i=0; i<level; i++) {
			printf("   ");
		}
		printf("Node type: %d; ", cur_node->type);
		printf("name: %s; ", cur_node->name);
		printf("content: %s; ", cur_node->content);
		printf("\n");
		print_all_attribs(cur_node, level);
		print_all_nodes(cur_node->children, level+1);
	}
}

int parse_int(char *str, int *val) {
	int i;
	char *end;
	*val = 0.0;
	if(isspace(*str)) {
		return -1;
	}
	i = strtol(str, &end, 0);
	if( (end - str) != strlen(str) ) {
		return -1;
	}
	*val = i;
	return 0;
}

int parse_color(char *str, color_t *val) {
	color_t c;
	if(strlen(str) < 3) {
		return -1;
	}
	if(str[0] == '#') { // RGB color string
		if(strlen(str) < 7) {
			ERROR("Too few characters in RGB color string!\n");
			return -1;
		}
		int i;
		for(i=1; i<=6; i++) {
			if( !isxdigit(str[i]) ) {
				ERROR("ERROR: All characters following '#' must be hex digits\n");
				return -1;
			}
		}
		char hex_str[3];
		int val[3];
		for(i=0; i<3; i++) {
			hex_str[0] = str[1 + 2*i];
			hex_str[1] = str[1 + 2*i + 1];
			hex_str[2] = '\0';
			val[i] = strtoul(hex_str, NULL, 16);
		}
		c.red   = val[0] / 255.0;
		c.green = val[1] / 255.0;
		c.blue  = val[2] / 255.0;
	}
	else {
		if(!strcmp(str, "red")) {
			c = COLOR_RED;
		} else if(!strcmp(str, "green")) {
			c = COLOR_GREEN;
		} else if(!strcmp(str, "blue")) {
			c = COLOR_BLUE;
		} else if(!strcmp(str, "black")) {
			c = COLOR_BLACK;
		} else if(!strcmp(str, "white")) {
			c = COLOR_WHITE;
		} else {
			ERROR("WARNING: couldn't match specified color (%s). Using BLACK instead...\n", str);
			c = COLOR_BLACK;
		}
	}
	*val = c;
	return 0;
}

int parse_double(char *str, double *val) {
	double d;
	char *end;
	*val = 0.0;
	if(isspace(*str)) {
		return -1;
	}
	d = strtod(str, &end);
	if( (end - str) != strlen(str) ) {
		return -1;
	}
	*val = d;
	return 0;
}

typedef struct _enum_map_t {
	const char *string;
	int enum_val;
} enum_map_t;

static enum_map_t conn_type_enum_map[] = {
	{"line", CONN_TYPE_LINE},
	{"spring", CONN_TYPE_SPRING},

	{0} // denotes end of array
};

static enum_map_t gnd_type_enum_map[] = {
	{"line", GND_TYPE_LINE},
	{"hash", GND_TYPE_HASH},
	{"pin", GND_TYPE_PIN},

	{0} // denotes end of array
};

typedef enum {
	INPUT_TYPE_TIME,
	INPUT_TYPE_BODY
} input_type_enum;

static enum_map_t input_fmt_enum_map[] = {
	{"time", INPUT_TYPE_TIME},
	{"body", INPUT_TYPE_BODY},

	{0} // denotes end of array
};

const char *enum_map_get_string_from_enum(enum_map_t *map, int e) {
	while(map->string) {
		if(map->enum_val == e) {
			return map->string;
		}
		map++;
	}
	return NULL;
}

int enum_map_get_enum_from_string(enum_map_t *map, char *string, int *enum_out) {
	while(map->string) {
		if(!strcmp(map->string, string)) {
			*enum_out =  map->enum_val;
			return 0;
		}
		map++;
	}
	return -1;
}

int parse_attrib_to_enum(
	xmlNode *xml, 
	int *dest, 
	char *attrib_name, 
	bool required, 
	int dflt, 
	enum_map_t *map) 
{
	char *val_str = xmlGetProp(xml, attrib_name);
	if(val_str == NULL) {
		if(required) {
			ERROR("Error: No \"%s\" attribute specified (it's required!)\n", attrib_name);
			return -1;
		}
		else {
			*dest = dflt;
			DEBUG2("  Didn't find \"%s\" attribute. Using default value (%s) instead...\n", 
				attrib_name, enum_map_get_string_from_enum(map, dflt));
			return 0;
		}
	}
	int e;
	if(enum_map_get_enum_from_string(map, val_str, &e)) {
		ERROR("Error: The \"%s\" attribute must be one of the following values:\n", attrib_name);
		enum_map_t *m = map;
		while(m->string) {
			ERROR("  %s\n", m->string);
			m++;
		}
		xmlFree(val_str);
		return -1;
	}
	*dest = e;
	xmlFree(val_str);
	printf("  parsed the \"%s\" attribute into an enum (%d)\n", attrib_name, e);
	return 0;
}

int parse_attrib_to_int(xmlNode *xml, int *dest, char *attrib_name, bool required, int dflt) {
	char *val_str = xmlGetProp(xml, attrib_name);
	if(val_str == NULL) {
		if(required) {
			ERROR("Error: No \"%s\" attribute specified (it's required!)\n", attrib_name);
			return -1;
		}
		else {
			*dest = dflt;
			DEBUG2("  Didn't find \"%s\" attribute. Using default value (%d) instead...\n", attrib_name, dflt);
			return 0;
		}
	}
	int i;
	if(parse_int(val_str, &i)) {
		ERROR("Error: The \"%s\" attribute must be an integer\n", attrib_name);
		xmlFree(val_str);
		return -1;
	}
	*dest = i;
	xmlFree(val_str);
	printf("  parsed the \"%s\" attribute into an integer (%d)\n", attrib_name, i);
	return 0;
}

int parse_attrib_to_string(xmlNode *xml, char **dest, char *attrib_name, bool required, char *dflt) {
	char *val_str = xmlGetProp(xml, attrib_name);
	if(val_str == NULL) {
		if(required) {
			ERROR("Error: No \"%s\" attribute specified (it's required!)\n", attrib_name);
			return -1;
		}
		else {
			char *s = malloc(strlen(dflt));
			if(s == NULL) {
				fprintf(stderr, "Error allocating string memory!\n");
				exit(-1);
			}
			strcpy(s, dflt);
			*dest = s;
			DEBUG2("  Didn't find \"%s\" attribute. Using default value (%s) instead...\n", attrib_name, dflt);
			return 0;
		}
	}
	*dest = val_str;
	/* note: I'm intentionally NOT freeing "val_str" */
	DEBUG("  parsed the \"%s\" attribute into a string (\"%s\")\n", attrib_name, *dest);
	return 0;
}

int parse_attrib_to_color(xmlNode *xml, color_t *dest, char *attrib_name, bool required, color_t *dflt) {
	char *val_str = xmlGetProp(xml, attrib_name);
	if(val_str == NULL) {
		if(required) {
			ERROR("Error: No \"%s\" attribute specified (it's required!)\n", attrib_name);
			return -1;
		}
		else {
			*dest = *dflt;
			DEBUG2("  Didn't find \"%s\" attribute. Using default value (%g,%g,%g) instead...\n", 
				attrib_name, dflt->red, dflt->green, dflt->blue);
			return 0;
		}
	}
	color_t c;
	if(parse_color(val_str, &c)) {
		ERROR("Error: The \"%s\" attribute must be a color string\n", attrib_name);
		xmlFree(val_str);
		return -1;
	}
	*dest = c;
	xmlFree(val_str);
	DEBUG("  parsed the \"%s\" attribute into a color (%g,%g,%g)\n", 
		attrib_name, c.red, c.green, c.blue);
	return 0;
}

int parse_attrib_to_double(xmlNode *xml, double *dest, char *attrib_name, bool required, double dflt) {
	char *val_str = xmlGetProp(xml, attrib_name);
	if(val_str == NULL) {
		if(required) {
			ERROR("Error: No \"%s\" attribute specified (it's required!)\n", attrib_name);
			return -1;
		}
		else {
			*dest = dflt;
			DEBUG2("  Didn't find \"%s\" attribute. Using default value (%g) instead...\n", attrib_name, dflt);
			return 0;
		}
	}
	double d;
	if(parse_double(val_str, &d)) {
		ERROR("Error: The \"%s\" attribute must be a decimal number\n", attrib_name);
		xmlFree(val_str);
		return -1;
	}
	*dest = d;
	xmlFree(val_str);
	DEBUG("  parsed the \"%s\" attribute into a double (%g)\n", attrib_name, d);
	return 0;
}

int parse_attrib_to_bool(xmlNode *xml, bool *dest, char *attrib_name, bool required, bool dflt) {
	static char *true_values[] =  {"true", "TRUE", "True", "1", "yes", "YES", "Yes"};
	static char *false_values[] = {"false", "FALSE", "False", "0", "no", "NO", "No"};

	char *val_str = xmlGetProp(xml, attrib_name);
	if(val_str == NULL) {
		if(required) {
			ERROR("Error: No \"%s\" attribute specified (it's required!)\n", attrib_name);
			return -1;
		}
		else {
			*dest = dflt;
			DEBUG2 (
				"  Didn't find \"%s\" attribute. Using default value (%s) instead...\n", 
				attrib_name, dflt ? "TRUE" : "FALSE"
			);
			return 0;
		}
	}
	bool b;
	int i;
	bool matched = false;
	for(i=0; i<sizeof(true_values)/sizeof(true_values[0]); i++) {
		if(!strcmp(val_str, true_values[i])) {
			b = true;
			matched = true;
			break;
		}
	}
	if(!matched) {
		for(i=0; i<sizeof(false_values)/sizeof(false_values[0]); i++) {
			if(!strcmp(val_str, false_values[i])) {
				b = false;
				matched = true;
				break;
			}
		}
	}
	if(!matched) {
		ERROR("Error: The \"%s\" attribute must be a boolean\n", attrib_name);
		xmlFree(val_str);
		return -1;
	}
	*dest = b;
	xmlFree(val_str);
	DEBUG(
		"  parsed the \"%s\" attribute into a boolean (%s)\n", 
		attrib_name, (b ? "TRUE" : "FALSE")
	);
	return 0;
}

body_t *lookup_body_by_id(int id) {
	int i;
	for(i=0; i < app_data.num_bodies; i++) {
		if(app_data.bodies[i]->id == id) {
			return app_data.bodies[i];
		}
	}
	return NULL;
}

int parse_input_format_xml(xmlNode *xml) {
		
	xmlNode *xnode;
	for(xnode = xml->children; xnode != NULL; xnode = xnode->next) {
		if(xnode->type == XML_ELEMENT_NODE && !strcmp(xnode->name, "map") ) {
			DEBUG("  Got <map> element\n");
			if(app_data.num_input_maps >= MAX_INPUT_MAPS) {
				ERROR("Too many input format entries!!\n");
				exit(-1);
			}

			int err = 0;
			int column;
			input_type_enum type;

			err = parse_attrib_to_int(xnode, &column, "column", true, 0);
			err = err || 
				parse_attrib_to_enum(xnode, (int *)&type, "type", true, 0, input_fmt_enum_map);
			if(err) {
				return -1;
			}
			input_map_t *map = malloc(sizeof(input_map_t));
			map->field_num = column;
			switch(type) {
				case INPUT_TYPE_TIME:
					map->dest = &app_data.time;
					map->data_type = DATA_TYPE_DOUBLE;
					app_data.bytes_per_frame += sizeof(double);
					if(app_data.explicit_time) {
						ERROR("Only 1 \"time\" type of input map is allowed!\n");
						exit(-1);
					}
					app_data.explicit_time = true;
					app_data.time_map_index = app_data.num_input_maps;
					break;
				case INPUT_TYPE_BODY: {
					int id;
					char *field_str;
					err = parse_attrib_to_int(xnode, &id, "id", true, 0);
					if(err) {
						return -1;
					}
					body_t *body = lookup_body_by_id(id);
					if(body == NULL) {
						ERROR("body ID (%d) referenced by <map> element does exist!\n", id);
						free(map);
						return -1;
					}
					err = err || parse_attrib_to_string(xnode, &field_str, "field", true, "");
					if(err) {
						xmlFree(field_str);
						free(map);
						return -1;
					}
					if(!strcmp(field_str, "x")) {
						map->dest = &body->x;
						map->data_type = DATA_TYPE_DOUBLE;
						app_data.bytes_per_frame += sizeof(double);
					}
					else if(!strcmp(field_str, "y")) {
						map->dest = &body->y;
						map->data_type = DATA_TYPE_DOUBLE;
						app_data.bytes_per_frame += sizeof(double);
					}
					else if(!strcmp(field_str, "theta")) {
						map->dest = &body->theta;
						map->data_type = DATA_TYPE_DOUBLE;
						app_data.bytes_per_frame += sizeof(double);
					}
					else {
						ERROR("Unsupported field\n");
						xmlFree(field_str);
						free(map);
						return -1;
					}
					break;
				}
			}
			app_data.input_maps[app_data.num_input_maps++] = map;
		}
	}
	return 0;
}

int parse_ground_xml(xmlNode *xml, ground_t *ground) {
	int error = 0;

	error = parse_attrib_to_enum(xml, (int *)&ground->type, "type", true, 0, gnd_type_enum_map);
	error = error || parse_attrib_to_int(xml, &ground->id, "id", false, -1);
	error = error || parse_attrib_to_double(xml, &ground->x1, "x1", true, 0.);
	error = error || parse_attrib_to_double(xml, &ground->y1, "y1", true, 0.);
	error = error || parse_attrib_to_double(xml, &ground->x2, "x2", true, 0.);
	error = error || parse_attrib_to_double(xml, &ground->y2, "y2", true, 0.);
	if(error) {
		ERROR("Error parsing <ground> XML\n");
		return -1;
	}
	return 0;
}

int parse_connector_xml(xmlNode *xml, connector_t *connect) {
	int error = 0;

	error = error || 
		parse_attrib_to_enum(xml, (int *)&connect->type, "type", true, 0, conn_type_enum_map);
	error = error || parse_attrib_to_int(xml, &connect->id, "id", true, 0);
	error = error || parse_attrib_to_color(xml, &(connect->color), "color", false, &COLOR_BLACK);
	error = error || parse_attrib_to_double(xml, &connect->thickness, "line_width", false, 2.0);

	if(error) {
		ERROR("Error parsing <connector> XML\n");
		return -1;
	}

	xmlNode *xnode;
	int attach_count = 0;
	double *x_dest[2] = {&connect->x1, &connect->x2};
	double *y_dest[2] = {&connect->y1, &connect->y2};
	body_t **body_dest[2] = {&connect->body_1, &connect->body_2};
	for(xnode = xml->children; xnode != NULL; xnode = xnode->next) {
		if(xnode->type == XML_ELEMENT_NODE && !strcmp(xnode->name, "attach") ) {
			if(attach_count >= 2) {
				ERROR("A <connector> element must only have two <attach> sub-elements!\n");
				return -1;
			}

			int err = 0;
			int id;
			err = err || parse_attrib_to_double(xnode, x_dest[attach_count], "x", true, 0);
			err = err || parse_attrib_to_double(xnode, y_dest[attach_count], "y", true, 0);
			err = err || parse_attrib_to_int(xnode, &id, "id", true, 0);
			if(err) {
				ERROR("Error parsing attributes from <attach> element!\n");
				return -1;
			}

			body_t *body = lookup_body_by_id(id);
			if(body == NULL) {
				ERROR("body ID (%d) referenced by <attach> element does exist!\n", id);
				return -1;
			}
			*body_dest[attach_count] = body;

			attach_count++;
		}
	}

	if(attach_count < 2) {
		ERROR("A <connector> element must have two <attach> sub-elements!\n");
		return -1;
	}
	return 0;
}

int parse_body_xml(xmlNode *xml, body_t *body) {
	int error = 0;

	error = error || parse_attrib_to_int(xml, &body->id, "id", false, body_auto_id());
	if(body->id == 0) {
		ERROR("0 is not an allowable ID value (it's reserved for ground)!\n");
		body->id = body_auto_id();
		DEBUG("Using automatic body id: %d\n", body->id);
	}

	char name[20];
	sprintf(name, "body_%03d", body->id);
	error = error || parse_attrib_to_string(xml, &body->name, "name", false, name);

	int i;
	error = error || parse_attrib_to_int(xml, &i, "xy_parent_id", false, 0);
	if(i != 0) {
		body_t *b = lookup_body_by_id(i);
		if(b == NULL) {
			ERROR("Coudn't find x-y parent body with id %d\n", i);
			error = 1;
		} else {
			body->xy_parent = b;
		}
	}

	error = error || parse_attrib_to_int(xml, &i, "theta_parent_id", false, 0);
	if(i != 0) {
		body_t *b = lookup_body_by_id(i);
		if(b == NULL) {
			ERROR("Coudn't find theta parent body with id %d\n", i);
			error = 1;
		} else {
			body->theta_parent = b;
		}
	}

	error = error || parse_attrib_to_bool(xml, &body->show_shape_frame, "show_shape_frame", false, false);
	error = error || parse_attrib_to_bool(xml, &body->show_body_frame, "show_body_frame", false, false);
	error = error || parse_attrib_to_bool(xml, &body->show_name, "show_name", false, false);
	error = error || parse_attrib_to_bool(xml, &body->show_id, "show_id", false, false);
	error = error || parse_attrib_to_bool(xml, &body->filled, "filled", false, true);
	error = error || parse_attrib_to_double(xml, &(body->x), "x", false , 0.);
	error = error || parse_attrib_to_double(xml, &(body->y), "y", false , 0.);
	error = error || parse_attrib_to_double(xml, &(body->theta), "theta", false , 0.);
	error = error || parse_attrib_to_double(xml, &(body->x_offset), "x_offset", false , 0.);
	error = error || parse_attrib_to_double(xml, &(body->y_offset), "y_offset", false , 0.);
	error = error || parse_attrib_to_double(xml, &(body->theta_offset), "theta_offset", false , 0.);
	error = error || parse_attrib_to_double(xml, &(body->phi), "phi", false , 0.);
	error = error || parse_attrib_to_double(xml, &body->line_width, "line_width", false, 1.0);
	error = error || parse_attrib_to_color(xml, &(body->color), "color", false, &COLOR_BLACK);
	if(error) {
		ERROR("Error parsing body XML\n");
		return -1;
	}
	return 0;
}

int parse_ball_xml(xmlNode *xml, ball_t *ball) {
	int error = 0;

	error = error || parse_body_xml(xml, (body_t *)ball);
	error = error || parse_attrib_to_double(xml, &(ball->radius), "radius", true , 0.0);
	if(error) {
		ERROR("Error parsing ball XML\n");
		return -1;
	}
	return 0;
}

int parse_block_xml(xmlNode *xml, block_t *block) {
	int error = 0;

	error = error || parse_body_xml(xml, (body_t *)block);
	error = error || parse_attrib_to_double(xml, &(block->width), "width", true, 0.);
	error = error || parse_attrib_to_double(xml, &(block->height), "height", true, 0.);
	if(error) {
		ERROR("Error parsing block XML\n");
		return -1;
	}
	return 0;
}

int parse_polygon_xml(xmlNode *xml, polygon_t *polygon) {
	int error = 0;

	error = error || parse_body_xml(xml, (body_t *)polygon);
	int node_count = 0;
	xmlNode *xnode;
	// first, loop through children, and count up number of <node> elements
	for(xnode = xml->children; xnode != NULL; xnode = xnode->next) {
		if(xnode->type == XML_ELEMENT_NODE && !strcmp(xnode->name, "node") ) {
			node_count++;
		}
	}

	if(node_count < 2) {
		ERROR("Error: only found %d (x,y) nodes in polygon element. Must specify at least 2\n", node_count);
		return -1;
	}

	// allocated memory for the (x,y) node data
	polygon->node_x = malloc(node_count * sizeof(double));
	polygon->node_y = malloc(node_count * sizeof(double));
	polygon->node_owner = true;
	polygon->node_count = node_count;

	// now loop through children again, this time parsing the (x,y) elmnts into arrays
	node_count = 0;
	int err = 0;
	for(xnode = xml->children; xnode != NULL; xnode = xnode->next) {
		if(xnode->type == XML_ELEMENT_NODE && !strcmp(xnode->name, "node") ) {
			err = 0;
			err = err || parse_attrib_to_double(xnode, &(polygon->node_x[node_count]), "x", true, 0.);
			err = err || parse_attrib_to_double(xnode, &(polygon->node_y[node_count]), "y", true, 0.);
			if(err) {
				ERROR("Error parsing polygon body's x,y node\n");
				break;
			}
			node_count++;
		}
	}
	if(err) {
		free(polygon->node_x);
		free(polygon->node_y);
		polygon->node_owner = false;
		error = 1;
	}

	if(error) {
		ERROR("Error parsing polygon XML\n");
		return -1;
	}
	return 0;
}

#define DIE_IF_TOO_MANY_BODIES() \
	if(app_data.num_bodies >= MAX_BODIES) { \
		ERROR("Maximum number of bodies (%d) exceeded!\n", MAX_BODIES); \
		exit(-1); \
	}

#define DIE_IF_TOO_MANY_CONNECTORS() \
	if(app_data.num_connectors >= MAX_CONNECTORS) { \
		ERROR("Maximum number of connectors (%d) exceeded!\n", MAX_CONNECTORS); \
		exit(-1); \
	}

#define DIE_IF_TOO_MANY_GROUNDS() \
	if(app_data.num_grounds >= MAX_GROUNDS) { \
		ERROR("Maximum number of grounds (%d) exceeded!\n", MAX_GROUNDS); \
		exit(-1); \
	}

int parse_root_attribs(xmlNode *xml) {
	int error = 0;
	error = error || parse_attrib_to_double(xml, &(app_data.x_range.min), "x_min", false, -10.0);
	error = error || parse_attrib_to_double(xml, &(app_data.x_range.max), "x_max", false, +10.0);
	error = error || parse_attrib_to_double(xml, &(app_data.y_range.min), "y_min", false, -10.0);
	error = error || parse_attrib_to_double(xml, &(app_data.y_range.max), "y_max", false, +10.0);
	if(error) {
		return -1;
	}
	return 0;
}

int parse_config_xml(xmlNode *xml) {
	printf("parsing config XML...\n");

	if(parse_root_attribs(xml)) {
		ERROR("*** Error parsing top level attributes\n");
	}

	xmlNode *curNode;
	for(curNode = xml->children; curNode != NULL; curNode = curNode->next) {
		if(curNode->type != XML_ELEMENT_NODE) {
			//printf("skipping non-Element node...\n");
			continue;
		}
		if(!strcmp(curNode->name, "ball")) {
			DIE_IF_TOO_MANY_BODIES();
			DEBUG("Got <ball> element!\n");
			ball_t *ball = ball_alloc();
			ball_init(ball);
			if(parse_ball_xml(curNode, ball)) {
				ERROR("*** Error parsing \"ball\" XML into ball_t struct!\n");
				ball_dealloc(ball);
				continue;
			}
			app_data.bodies[app_data.num_bodies++] = (body_t *)ball;
		}
		else if(!strcmp(curNode->name, "block")) {
			DIE_IF_TOO_MANY_BODIES();
			DEBUG("Got <block> element!\n");
			block_t *block = block_alloc();
			block_init(block);
			if(parse_block_xml(curNode, block)) {
				ERROR("*** Error parsing \"block\" XML into block_t struct!\n");
				block_dealloc(block);
				continue;
			}
			app_data.bodies[app_data.num_bodies++] = (body_t *)block;
		}
		else if(!strcmp(curNode->name, "polygon")) {
			DIE_IF_TOO_MANY_BODIES();
			DEBUG("Got <polygon> element!\n");
			polygon_t *poly = polygon_alloc();
			polygon_init(poly);
			if(parse_polygon_xml(curNode, poly)) {
				ERROR("*** Error parsing \"polygon\" XML into polygon_t struct!\n");
				polygon_dealloc(poly);
				continue;
			}
			app_data.bodies[app_data.num_bodies++] = (body_t *)poly;
		}
		else if(!strcmp(curNode->name, "connector")) {
			DIE_IF_TOO_MANY_CONNECTORS();
			DEBUG("Got <connector> element!\n");
			connector_t *connect = connector_alloc();
			connector_init(connect);
			if(parse_connector_xml(curNode, connect)) {
				ERROR("*** Error parsing <connector> XML into connector_t struct!\n");
				connector_dealloc(connect);
				continue;
			}
			app_data.connectors[app_data.num_connectors++] = connect;
		}
		else if(!strcmp(curNode->name, "ground")) {
			DIE_IF_TOO_MANY_GROUNDS();
			DEBUG("Got <ground> element!\n");
			ground_t *ground = ground_alloc();
			ground_init(ground);
			if(parse_ground_xml(curNode, ground)) {
				ERROR("*** Error parsing <ground> XML into ground_t struct!\n");
				ground_dealloc(ground);
				continue;
			}
			app_data.grounds[app_data.num_grounds++] = ground;
		}
		else if(!strcmp(curNode->name, "input_format")) {
			DEBUG("Got <input_format> element!\n");
			if(parse_input_format_xml(curNode)) {
				ERROR("*** Error parsing <input_format> XML!\n");
				continue;
			}
		}
		else {
			ERROR("Unsupported element! (%s)\n", curNode->name);
		}
	}

	DEBUG("**************************\n");
	DEBUG("Got %d bodies\n", app_data.num_bodies);
	DEBUG("Got %d connectors\n", app_data.num_connectors);
	DEBUG("Got %d grounds\n", app_data.num_grounds);
	DEBUG("Got %d input_field entries\n", app_data.num_input_maps);
	DEBUG("Number of bytes per frame: %d\n", app_data.bytes_per_frame);
	return 0;
}

void print_usage(FILE *stream, char *prog_name) {
	fprintf(stream, "Usage: %s XML_CONFIG_FILE [DATAFILE]\n", prog_name);
	fprintf(stream, "\n");
	fprintf(stream, "DATAFILE may be either a file name/path or \"-\" to denote STDIN.\n");
	fprintf(stream, "If DATAFILE is not given, STDIN will be used.\n");
}
typedef struct {
	char *fname;
	int fd;
	char *line_buf;
	int line_capacity;
	int line_length;
	bool line_complete;
} input_data_t;


int split_line_into_fields(char *line, char *fields[], int max_fields) {
	char *p;
	int field_count;
	bool in_field = false;
	for(p=line, field_count=0; *p != '\0'; p++) {
		if(isspace(*p)) {
			*p = '\0'; // replace all whitespace characters with NULL
			in_field = false;
		}
		else { // non-whitespace character
			if(!in_field) {
				if(field_count >= max_fields) {
					ERROR("Too many fields in line!!\n");
					return -1;
				}
				fields[field_count++] = p;
				in_field = true;
			}
		}
	}
	return field_count;
}

void print_connector_info(connector_t *connect) {
	printf("Connector id %-4d: Attach_1=(%d, %g, %g) Attach_2=(%d, %g, %g) \n", 
		connect->id, 
		connect->body_1->id, connect->x1, connect->y1,
		connect->body_2->id, connect->x2, connect->y2
	);
}

void print_body_info(body_t *body) {
	printf("Body id %-4d: (%6g,%6g,%6g)\n", 
		body->id, body->x, body->y, body->theta);
}



static double body_theta_to_ground(body_t *body) {
	body_t *b;
	double theta = body->theta;
	for(b=body; b->theta_parent != NULL; b=b->theta_parent) {
		theta += b->theta_parent->theta;
	}
	return theta;
}


static void body_transform_shape_to_ground(body_t *body, transform_t *t_out) {
	transform_t T;

	// first step is transform from shape frame to body frame
	transform_make(&T, body->x_offset, body->y_offset, body->phi);

	body_t *b;
	for(b=body; b != NULL; b = b->xy_parent) {
		double qpar_theta = 0.0;
		if(b->theta_parent != NULL) {
			qpar_theta = body_theta_to_ground(b->theta_parent);
		}
		double xypar_theta = 0.0;
		if(b->xy_parent != NULL) {
			xypar_theta = body_theta_to_ground(b->xy_parent);
		}
		transform_t t_new;
		transform_make(&t_new, b->x, b->y, b->theta + qpar_theta - xypar_theta);
		transform_append(&T, &t_new);
	}

	*t_out = T;
}

#define X_USER_TO_PX(x) (x_m * (x) + x_b)
#define Y_USER_TO_PX(y) (y_m * (y) + y_b)
#define L_USER_TO_PX(l) fabs(x_m * (l))
#define L_PX_TO_USER(l) fabs((l) / x_m)
#define FRAME_SIZE_PX (20)

gboolean draw_canvas(GtkWidget *widget, GdkEventExpose *event, gpointer data) {
	draw_ptr dp = app_data.gui.drawer;
	draw_start(dp); 
	int i;

	float width, height;
	draw_get_canvas_dims(dp, &width, &height);

	// first, fill with background color
	draw_set_color(dp, 1,1,1);
	draw_set_line_width(dp, 1);
	draw_rectangle_filled(dp, 0, 0, width, height);

	float xmin = app_data.x_range.min;
	float xmax = app_data.x_range.max;
	float ymin = app_data.y_range.min;
	float ymax = app_data.y_range.max;

	/* These are used to convert from user coordinates to pixel coordinates
	*  Specifically: 
	*    x_px = x_m * x_user + x_b
	*    y_px = y_m * y_user + y_b
	*/
	float x_m, x_b;
	float y_m, y_b;
	x_m = width/(xmax-xmin);
	y_m = height/(ymin-ymax);
	if(fabs(x_m) < fabs(y_m)) {
		y_m = -x_m;
		x_b = -x_m * xmin;
		y_b = height/2.0 - y_m * (ymin+ymax)/2.0;
	}
	else {
		x_m = -y_m;
		y_b = -y_m * ymax;
		x_b = width/2.0 - x_m * (xmin+xmax)/2.0;
	}
	
	// now draw the ground coordinate system ***************************
	draw_set_color(dp, 0.5,0.5,0.5);
	draw_line(dp, X_USER_TO_PX(0), Y_USER_TO_PX(ymin), X_USER_TO_PX(0), Y_USER_TO_PX(ymax));
	draw_line(dp, X_USER_TO_PX(xmin), Y_USER_TO_PX(0), X_USER_TO_PX(xmax), Y_USER_TO_PX(0));

	//draw_set_color(dp, 0, 0, 0);	
	//draw_text(dp, "hello world", 10, X_USER_TO_PX(0), Y_USER_TO_PX(0), ANCHOR_MIDDLE_MIDDLE);

	// draw all bodies *************************************************
	for(i=0; i < app_data.num_bodies; i++) {
		body_t *body = app_data.bodies[i];
		switch(body->type) {
		case BODY_TYPE_BALL: {
			draw_set_color(dp, body->color.red, body->color.green, body->color.blue);
			float r = ((ball_t *)body)->radius;
			double x_c, y_c;
			transform_point(&(body->trans_shape_to_gnd), 0.0, 0.0, &x_c, &y_c);

			if(body->filled) 
				draw_circle_filled(dp, X_USER_TO_PX(x_c), Y_USER_TO_PX(y_c), L_USER_TO_PX(r));
			else {
				draw_set_line_width(dp, body->line_width);
				draw_circle_outline(dp, X_USER_TO_PX(x_c), Y_USER_TO_PX(y_c), L_USER_TO_PX(r));
			}
			break;
		}
		case BODY_TYPE_BLOCK: {
			block_t *block = (block_t *)body;
			draw_set_color(dp, body->color.red, body->color.green, body->color.blue);
			float w_2 = block->width/2.0;
			float h_2 = block->height/2.0;
			double x[4] = {-w_2, +w_2, +w_2, -w_2};
			double y[4] = {-h_2, -h_2, +h_2, +h_2};
			transform_points(&(body->trans_shape_to_gnd), 4, x, y, x, y);
			int i;
			float x_px[4], y_px[4];
			for(i=0; i<4; i++) {
				x_px[i] = X_USER_TO_PX(x[i]);
				y_px[i] = Y_USER_TO_PX(y[i]);
			}
			if(body->filled) 
				draw_polygon_filled(dp, x_px, y_px, 4);
			else {
				draw_set_line_width(dp, body->line_width);
				draw_polygon_outline(dp, x_px, y_px, 4);
			}
			break;
		}
		case BODY_TYPE_POLYGON: {
			polygon_t *poly = (polygon_t *)body;
			draw_set_color(dp, body->color.red, body->color.green, body->color.blue);
			float *x = malloc(poly->node_count * sizeof(float));
			float *y = malloc(poly->node_count * sizeof(float));
			assert(x && y);

			int i;
			for(i=0; i<poly->node_count; i++) {
				double tmp_x, tmp_y;
				transform_point(&(body->trans_shape_to_gnd), poly->node_x[i], poly->node_y[i], &tmp_x, &tmp_y);
				x[i] = tmp_x;
				y[i] = tmp_y;
				x[i] = X_USER_TO_PX(x[i]);
				y[i] = Y_USER_TO_PX(y[i]);
			}
			if(body->filled) 
				draw_polygon_filled(dp, x, y, poly->node_count);
			else {
				draw_set_line_width(dp, body->line_width);
				draw_polygon_outline(dp, x, y, poly->node_count);
			}

			free(x);
			free(y);
			break;
		}
		default:
			ERROR("Unsupported body type!\n");
			exit(-1);
		} // end switch(body->type)

		if(body->show_body_frame) {
			double x[3] = {L_PX_TO_USER(FRAME_SIZE_PX), 0, 0};
			double y[3] = {0, 0, L_PX_TO_USER(FRAME_SIZE_PX)};
			transform_points(&(body->trans_shape_to_gnd), 3, x, y, x, y);
			int i;
			float x_px[3], y_px[3];
			for(i=0; i<3; i++) {
				x_px[i] = X_USER_TO_PX(x[i]);
				y_px[i] = Y_USER_TO_PX(y[i]);
			}
			draw_set_color(dp, 0,0,0);
			draw_polygon_outline(dp, x_px, y_px, 3);
		}
		if(body->show_shape_frame) {
			double x[3] = {L_PX_TO_USER(FRAME_SIZE_PX), 0, 0};
			double y[3] = {0, 0, L_PX_TO_USER(FRAME_SIZE_PX)};
			transform_points(&(body->trans_shape_to_gnd), 3, x, y, x, y);
			int i;
			float x_px[3], y_px[3];
			for(i=0; i<3; i++) {
				x_px[i] = X_USER_TO_PX(x[i]);
				y_px[i] = Y_USER_TO_PX(y[i]);
			}
			draw_set_color(dp, 0,0,0);
			draw_polygon_outline(dp, x_px, y_px, 3);
		}
	}

	// draw all the connectors ************************************************
	for(i=0; i < app_data.num_connectors; i++) {
		connector_t *connect = app_data.connectors[i];
		switch(connect->type) {
			case CONN_TYPE_SPRING:
				draw_set_color(dp, connect->color.red, connect->color.green, connect->color.blue);
				draw_set_line_width(dp, connect->thickness);
			
				double x1, x2, y1, y2;
				transform_point(&(connect->body_1->trans_shape_to_gnd), connect->x1, connect->y1, &x1, &y1);
				transform_point(&(connect->body_2->trans_shape_to_gnd), connect->x2, connect->y2, &x2, &y2);

				float dx = x2 - x1;
				float dy = y2 - y1;
				float L = sqrt(dx * dx + dy * dy);
				float h = 0.2 * L;
				float theta = atan2(dy, dx);
				float sin_, cos_;
				sin_ = sin(theta);
				cos_ = cos(theta);
				float x[9] = {0.0, 0.2*L, 0.26*L, 0.38*L, 0.50*L, 0.62*L, 0.74*L, 0.8*L,  L};
				float y[9] = {0.0, 0.0,   +h/2.0, -h/2.0, +h/2.0, -h/2.0, +h/2.0, 0.0,  0.0};
				int i;
				float x_px[9], y_px[9];
				for(i=0; i<9; i++) {
					x_px[i] = X_USER_TO_PX(x1 + x[i] * cos_ - y[i] * sin_);
					y_px[i] = Y_USER_TO_PX(y1 + x[i] * sin_ + y[i] * cos_);
				}
				draw_polygon_outline(dp, x_px, y_px, 9);
				break;
			case CONN_TYPE_LINE: {
				draw_set_color(dp, connect->color.red, connect->color.green, connect->color.blue);
				draw_set_line_width(dp, connect->thickness);
				double x1, y1, x2, y2;
				transform_point(&(connect->body_1->trans_shape_to_gnd), connect->x1, connect->y1, &x1, &y1);
				transform_point(&(connect->body_2->trans_shape_to_gnd), connect->x2, connect->y2, &x2, &y2);
				draw_line ( dp,
					X_USER_TO_PX(x1), Y_USER_TO_PX(y1),
					X_USER_TO_PX(x2), Y_USER_TO_PX(y2)
				);
				break;
			}
			default:
				ERROR("Unsupported connector type!\n");
				exit(-1);
		}
	}

	// draw all the grounds ************************************************
	for(i=0; i < app_data.num_grounds; i++) {
		ground_t *gnd = app_data.grounds[i];
		switch(gnd->type) {
		case GND_TYPE_LINE: {
			draw_set_color(dp, 0,0,0);
			draw_set_line_width(dp, 2.0);
			draw_line ( dp,
				X_USER_TO_PX(gnd->x1), Y_USER_TO_PX(gnd->y1),
				X_USER_TO_PX(gnd->x2), Y_USER_TO_PX(gnd->y2)
			);
			break;
		}
		case GND_TYPE_HASH: {
			draw_set_color(dp, 0,0,0);
			draw_set_line_width(dp, 2.0);
			float x1_px = X_USER_TO_PX(gnd->x1);
			float y1_px = Y_USER_TO_PX(gnd->y1);
			float x2_px = X_USER_TO_PX(gnd->x2);
			float y2_px = Y_USER_TO_PX(gnd->y2);
			draw_line(dp, x1_px, y1_px, x2_px, y2_px);
			float dx = x2_px-x1_px;
			float dy = y2_px-y1_px;
			float theta = atan2(dy,dx);
			float L = sqrt(dx * dx + dy * dy);
			int num_hashes = L / 10;
			float hash_len = 10;
			float hash_rads = 0.7;
			float sin_ = sin(theta + hash_rads);
			float cos_ = cos(theta + hash_rads);
			int i;
			for(i=0; i < num_hashes; i++) {
				float fract = (float)i/num_hashes;
				draw_line ( dp,
					x1_px + fract*dx, y1_px + fract*dy,
					x1_px + fract*dx + hash_len*cos_, 
					y1_px + fract*dy + hash_len*sin_
				);
			}
			break;
		}
		default:
			//ERROR("Unsupported ground type!\n");
			//exit(-1);
			;
		}
	}

	draw_finish(dp); 
	return TRUE;
}

static void update_body_transforms(void) {
	int i;
	for(i=0; i<app_data.num_bodies; i++) {
		body_transform_shape_to_ground(app_data.bodies[i], &(app_data.bodies[i]->trans_shape_to_gnd));
	}
}

static void update_bodies(void) {
	int j;
	frame_ptr_t pframe = app_data.frames[app_data.active_frame_index];

	/* loop over all input maps, stuffing the data
	 * in the frame into the proper destination location */
	for(j=0; j < app_data.num_input_maps; j++) {
		input_map_t *map = app_data.input_maps[j];
		switch(map->data_type) {
			case DATA_TYPE_DOUBLE:
				*((double *)(map->dest)) = *((double *)(&pframe[map->frame_byte_offset]));
				break;
			default:
				ERROR("Unhandled data type!!!\n");
				exit(-1);
		}
	}

	update_body_transforms();

}

static double get_time_from_frame(frame_ptr_t pframe) {
	assert(app_data.explicit_time);
	input_map_t *map = app_data.input_maps[app_data.time_map_index];
	double t = *((double *)(&pframe[map->frame_byte_offset]));
	return t;
}

gboolean update_func(gpointer data) {

	if(app_data.paused) {
		return TRUE;
	}

	//printf("frame #%05d:\n", app_data.active_frame_index);
	update_bodies();

	// set the slider value
	if(app_data.explicit_time) {
		app_data.time = 
			get_time_from_frame(app_data.frames[app_data.active_frame_index]);
	} else {
		app_data.time = app_data.dt * app_data.active_frame_index;
	}
	gtk_range_set_value((GtkRange *)app_data.gui.slider, app_data.time);
	char str[50];
	sprintf(str, "t=%g", app_data.time);
	gtk_label_set_text((GtkLabel*)app_data.gui.time, str);

	// request a redraw of the canvas, which will redraw everything
  gtk_widget_queue_draw(app_data.gui.canvas);

	// advance frame counter
	// If we get to the end (last frame), start over at the beginning
	app_data.active_frame_index++;
	if(app_data.active_frame_index >= app_data.num_frames) {
		//app_data.active_frame_index = app_data.num_frames - 1;
		app_data.active_frame_index = 0;
	}

	return TRUE;
}

void button_activate(GtkButton *b, gpointer data) {
	app_data.paused = !app_data.paused;
	if(app_data.paused) {
		GtkWidget *im = gtk_button_get_image(b);
		gtk_image_set_from_stock(
			(GtkImage *)im, 
			GTK_STOCK_MEDIA_PLAY, 
			GTK_ICON_SIZE_SMALL_TOOLBAR
		);
	}
	else {
		GtkWidget *im = gtk_button_get_image(b);
		gtk_image_set_from_stock(
			(GtkImage *)im, 
			GTK_STOCK_MEDIA_PAUSE, 
			GTK_ICON_SIZE_SMALL_TOOLBAR
		);
	}
	return;
}

void slider_changed_cb(GtkRange *range, gpointer  user_data) {
	//printf("slider changed!\n");
}

gboolean slider_changed2_cb (
		GtkRange *range,
		GtkScrollType scroll,
		gdouble value, 
		gpointer user_data) 
{
	//printf("slider changed 2!\n");

	// if NOT paused, prevent user from moving the slider
	if(!app_data.paused) {
		return TRUE;
	}

	// Otherwise (we must be paused)
	//printf("scroll type: %d\n", scroll);
	switch(scroll) {
	case GTK_SCROLL_JUMP:
		// set the active frame index and time based on slider position
		if(app_data.explicit_time) {
			int frame_index = (value - app_data.t_min)/(app_data.t_max - app_data.t_min) * app_data.num_frames;
			if (frame_index < 0)
				frame_index = 0;
			else if( frame_index >= app_data.num_frames)
				frame_index = app_data.num_frames - 1;
			double t = get_time_from_frame(app_data.frames[frame_index]);
			double delta= fabs(t - value);
			if(t < value) {
				while(1) {
					if( (frame_index+1) >= app_data.num_frames) {
						break;
					}
					double t_next = get_time_from_frame(app_data.frames[frame_index + 1]);
					double delta_next = fabs(t_next - value);
					if(delta_next > delta) {
						break;
					}
					t = t_next;
					delta = delta_next;
					frame_index++;
				}
			}
			else if(t > value) {
				while(t > value) {
					if(frame_index <= 0) {
						break;
					}
					double t_next = get_time_from_frame(app_data.frames[frame_index - 1]);
					double delta_next = fabs(t_next - value);
					if(delta_next > delta) {
						break;
					}
					t = t_next;
					delta = delta_next;
					frame_index--;
				}
			}
			app_data.active_frame_index = frame_index;
			gtk_range_set_value((GtkRange *)app_data.gui.slider, t);
		}
		else {
			int frame_index = (value - app_data.t_min)/(app_data.t_max - app_data.t_min) * app_data.num_frames;
			if (frame_index < 0)
				frame_index = 0;
			else if( frame_index >= app_data.num_frames)
				frame_index = app_data.num_frames - 1;
			app_data.active_frame_index = frame_index;
			gtk_range_set_value((GtkRange *)app_data.gui.slider, app_data.active_frame_index * app_data.dt);
		}
		break;
	case GTK_SCROLL_STEP_FORWARD:
		{
			app_data.active_frame_index++;
			if(app_data.active_frame_index >= app_data.num_frames) {
				app_data.active_frame_index = app_data.num_frames - 1;
			}
			double t;
			if(app_data.explicit_time) {
				t = get_time_from_frame(app_data.frames[app_data.active_frame_index]);
			} else {
				t = app_data.active_frame_index * app_data.dt;
			}
			gtk_range_set_value((GtkRange *)app_data.gui.slider, t);
			break;
		}
	case GTK_SCROLL_STEP_BACKWARD:
		{
			app_data.active_frame_index--;
			if(app_data.active_frame_index < 0) {
				app_data.active_frame_index = 0;
			}
			double t;
			if(app_data.explicit_time) {
				t = get_time_from_frame(app_data.frames[app_data.active_frame_index]);
			} else {
				t = app_data.active_frame_index * app_data.dt;
			}
			gtk_range_set_value((GtkRange *)app_data.gui.slider, t);
			break;
		}
	case GTK_SCROLL_PAGE_FORWARD:
		{
			app_data.active_frame_index += 10;
			if(app_data.active_frame_index >= app_data.num_frames) {
				app_data.active_frame_index = app_data.num_frames - 1;
			}
			double t;
			if(app_data.explicit_time) {
				t = get_time_from_frame(app_data.frames[app_data.active_frame_index]);
			} else {
				t = app_data.active_frame_index * app_data.dt;
			}
			gtk_range_set_value((GtkRange *)app_data.gui.slider, t);
			break;
		}
	case GTK_SCROLL_PAGE_BACKWARD:
		{
			app_data.active_frame_index -= 10;
			if(app_data.active_frame_index < 0) {
				app_data.active_frame_index = 0;
			}
			double t;
			if(app_data.explicit_time) {
				t = get_time_from_frame(app_data.frames[app_data.active_frame_index]);
			} else {
				t = app_data.active_frame_index * app_data.dt;
			}
			gtk_range_set_value((GtkRange *)app_data.gui.slider, t);
			break;
		}
	case GTK_SCROLL_START:
		{
			app_data.active_frame_index = 0;
			double t;
			if(app_data.explicit_time) {
				t = get_time_from_frame(app_data.frames[app_data.active_frame_index]);
			} else {
				t = app_data.active_frame_index * app_data.dt;
			}
			gtk_range_set_value((GtkRange *)app_data.gui.slider, t);
			break;
		}
	case GTK_SCROLL_END:
		{
			app_data.active_frame_index = app_data.num_frames - 1;
			double t;
			if(app_data.explicit_time) {
				t = get_time_from_frame(app_data.frames[app_data.active_frame_index]);
			} else {
				t = app_data.active_frame_index * app_data.dt;
			}
			gtk_range_set_value((GtkRange *)app_data.gui.slider, t);
			break;
		}
	default:
		return TRUE;
	}
	update_bodies();
	gtk_widget_queue_draw(app_data.gui.canvas);
	char str[50];
	sprintf(str, "t=%g", app_data.time);
	gtk_label_set_text((GtkLabel*)app_data.gui.time, str);

	return TRUE;
}


void init_gui(void) {
	GtkWidget *window;
	GtkWidget *v_box;
	GtkWidget *vcr_hbox;
	GtkWidget *button;

	gtk_init (NULL, NULL);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	v_box = gtk_vbox_new(FALSE, 1);
	gtk_container_add (GTK_CONTAINER (window), v_box);

	app_data.gui.canvas = gtk_drawing_area_new();
	gtk_widget_set_size_request(app_data.gui.canvas, 500,400);
	gtk_box_pack_start (GTK_BOX(v_box), app_data.gui.canvas, TRUE, TRUE, 0);
	g_signal_connect(app_data.gui.canvas, "expose_event", G_CALLBACK(draw_canvas), NULL);

	app_data.gui.drawer = draw_create(app_data.gui.canvas);

	vcr_hbox = gtk_hbox_new(FALSE, 10);
	GtkWidget *button_v_box = gtk_vbox_new(FALSE, 10);

	button = gtk_button_new();
	gtk_box_pack_start (GTK_BOX(v_box), vcr_hbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX(vcr_hbox), button_v_box, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX(button_v_box), button, FALSE, FALSE, 0);
	GtkWidget *button_image = gtk_image_new_from_stock(
		GTK_STOCK_MEDIA_PAUSE, GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_button_set_image((GtkButton *)button, button_image);
	g_signal_connect(button, "clicked", G_CALLBACK(button_activate), NULL);

	app_data.gui.slider = 
		gtk_hscale_new_with_range(
			app_data.t_min, 
			app_data.t_max, 
			(app_data.explicit_time ? 0.05 : app_data.dt) 
		);
	gtk_scale_set_draw_value((GtkScale *)app_data.gui.slider, FALSE);
	//g_signal_connect(app_data.gui.slider, "value-changed", G_CALLBACK(slider_changed_cb), NULL);
	g_signal_connect(app_data.gui.slider, "change-value", G_CALLBACK(slider_changed2_cb), NULL);
	char str[20];
	snprintf(str, sizeof(str), "%g", app_data.t_min);
	gtk_scale_add_mark(
		(GtkScale *)app_data.gui.slider,
		app_data.t_min,
		GTK_POS_BOTTOM,
		str
	);
	snprintf(str, sizeof(str), "%g", app_data.t_max);
	gtk_scale_add_mark(
		(GtkScale *)app_data.gui.slider,
		app_data.t_max,
		GTK_POS_BOTTOM,
		str
	);
	gtk_scale_set_digits((GtkScale *)app_data.gui.slider, 5);
	gtk_box_pack_start (GTK_BOX(vcr_hbox), app_data.gui.slider, TRUE, TRUE, 0);

	gtk_range_set_value((GtkRange *)app_data.gui.slider, 0.05);

	GtkWidget *status_hbox = gtk_hbox_new(FALSE, 10);
	gtk_box_pack_start (GTK_BOX(v_box), status_hbox, FALSE, FALSE, 0);
	app_data.gui.playback_state = gtk_label_new("Playing...");
	gtk_box_pack_start (GTK_BOX(status_hbox), app_data.gui.playback_state, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX(status_hbox), gtk_vseparator_new(), FALSE, FALSE, 0);
	app_data.gui.time = gtk_label_new("t=0.0");
	gtk_box_pack_start (GTK_BOX(status_hbox), app_data.gui.time, FALSE, FALSE, 0);

	g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
	gtk_widget_show_all (window);
	g_timeout_add(30, update_func, NULL);
}

#define MAX_FIELDS 30
int main(int argc, char *argv[]) {
	struct gengetopt_args_info args_info;
	cmdline_parser(argc, argv, &args_info);

	if(argc < 2) {
		print_usage(stdout, argv[0]);
		return 0;
	}

	app_data_init(&app_data);

	/* this initializes the library and check potential ABI mismatches
	 * between the version it was compiled for and the actual shared
	 * library used. */
	LIBXML_TEST_VERSION

	xmlDocPtr doc;
	xmlNodePtr root;
	if(load_config(argv[1], &doc)) {
		printf("Error loading or valdiating XML config file!\n");
		exit(-1);
	}
	root = xmlDocGetRootElement(doc);
	//print_element_names(root, 0 );
	//print_all_nodes(root, 0);
	parse_config_xml(root);
	xmlFreeDoc(doc);

	int i;
	for(i=0; i<app_data.num_bodies; i++) {
		print_body_info(app_data.bodies[i]);
	}
	for(i=0; i<app_data.num_connectors; i++) {
		print_connector_info(app_data.connectors[i]);
	}

	char *infile = "-";
	FILE *fp;
	if(argc > 2) {
		infile = argv[2];
		fp = fopen(infile, "r");
		if(fp==NULL) {
			ERROR("Error opening datafile: %s\n", infile);
			exit(-1);
		}
	}
	else {
		fp = stdin;
	}
	char line[2000];
	unsigned int line_num = 0;
	while(fgets(line, sizeof(line), fp) != NULL) {
		//printf("got line (%d chars long): %s\n", (int)strlen(line), line);
		line_num++;
		//printf("here %d\n", line_num);
		int i;
		char *fields[MAX_FIELDS];


		int field_count = split_line_into_fields(line, fields, MAX_FIELDS);
		if(field_count < 0) {
			continue;
		}

		frame_ptr_t pframe = frame_alloc(app_data.bytes_per_frame);
		int offset = 0;
		for(i=0; i < app_data.num_input_maps; i++) {
			input_map_t *map = app_data.input_maps[i];
			if(map->field_num > field_count) {
				ERROR("Not enough fields!!\n");
				exit(-1);
			}
			int field_index = map->field_num - 1;
			switch(map->data_type) {
				case DATA_TYPE_DOUBLE: {
					double d;
					if(parse_double(fields[field_index], &d)) {
						ERROR("Error parsing double from field (\"%s\")\n", fields[field_index]);
						ERROR("line: %s\n", line);
						exit(-1);
					}
					//printf("input_map #%d: column=%d, type=double, value=%g\n", i+1, map->field_num, d);
					*((double *)(&pframe[offset])) = d;
					map->frame_byte_offset = offset;
					offset += sizeof(double);

					// ensure that timestamp is monotonic, and keep track of min/max timestamps
					if(i == app_data.time_map_index) {
						if(app_data.num_frames == 0) { // first frame
							app_data.t_min = d;
							app_data.t_max = d;
						}
						else if(d < app_data.t_max) {
							ERROR("Non-monotonic timestamp detected!!!\n");
							exit(-1);
						}
						else {
							app_data.t_max = d;
						}
					}
					break;
				}
				default:
					ERROR("Unknown data type!\n");
			}
		}
		if(app_data.num_frames >= app_data.frames_capacity) {
			app_data.frames_capacity *= 3;
			app_data.frames = realloc(app_data.frames, app_data.frames_capacity * sizeof(app_data.frames[0]));
			if(app_data.frames == NULL) {
				ERROR("Error expanding size of 'frames'.\n");
				exit(-1);
			}
		}
		app_data.frames[app_data.num_frames++] = pframe;
	
	}

	if(!feof(fp)) {
		ERROR("Error while reading datafile!!\n");
		exit(-1);
	}

	// set min/max time for implicit time case
	if(!app_data.explicit_time) {
		app_data.t_min = 0.0;
		app_data.t_max = (app_data.num_frames - 1) * app_data.dt;
	}

	printf("Got %d frames\n", app_data.num_frames);
	app_data.active_frame_index = 0;

	init_gui();
	gtk_main();

	return 0;
}

