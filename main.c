#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>

#include <gtk/gtk.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "draw.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#define MAX_BODIES     500
#define MAX_CONNECTORS 500
#define MAX_INPUT_MAPS 100
#define MAX_GROUNDS 100

#define INIT_FRAMES_CAPACITY 1000

#define PRINT_DEBUG 1
#define PRINT_DEBUG2 1
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

#if PRINT_ERRORS
	#define ERROR(...) fprintf(stderr, __VA_ARGS__)
#else
	#define ERROR(...) 
#endif

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
	void *dest;
	data_type_enum data_type;
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
	GtkWidget *dt_scale;
	draw_ptr drawer;
} gui_t;

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

	double time;

	gui_t gui;

	int active_frame_index;

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
}

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

void custom_dealloc(custom_t *self) {
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
	self->x1 = -1.0;
	self->x2 = +1.0;
	self->y1 = -1.0;
	self->y2 = +1.0;
	return 0;
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

int custom_init(custom_t *self) {
	body_init((body_t *)self, BODY_TYPE_CUSTOM);
	self->node_count = 0;
	self->node_x = NULL;
	self->node_y = NULL;
	self->node_owner = false;
	return 0;
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
			strcpy(*dest, dflt);
			DEBUG2("  Didn't find \"%s\" attribute. Using default value (%s) instead...\n", attrib_name, dflt);
			return 0;
		}
	}
	*dest = val_str;
	/* note: I'm intentionally NOT freeing "val_str" */
	DEBUG("  parsed the \"%s\" attribute into a string (\"%s\")\n", attrib_name, *dest);
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
		if(xnode->type == XML_ELEMENT_NODE && !strcmp(xnode->name, "entry") ) {
			DEBUG("  Got <entry> element\n");
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
						ERROR("body ID (%d) referenced by <entry> element does exist!\n", id);
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
			double x, y;
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

	error = error || parse_attrib_to_int(xml, &body->id, "id", true, -1);
	error = error || parse_attrib_to_string(xml, &body->name, "name", false, "body_rock");
	error = error || parse_attrib_to_bool(xml, &body->show_cs, "show_cs", false, false);
	error = error || parse_attrib_to_bool(xml, &body->show_name, "show_name", false, false);
	error = error || parse_attrib_to_bool(xml, &body->show_id, "show_id", false, false);
	error = error || parse_attrib_to_bool(xml, &body->filled, "filled", false, true);
	error = error || parse_attrib_to_double(xml, &(body->x), "x", false , 0.);
	error = error || parse_attrib_to_double(xml, &(body->y), "y", false , 0.);
	error = error || parse_attrib_to_double(xml, &(body->theta), "theta", false , 0.);
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
	error = error || parse_attrib_to_double(xml, &(block->x1), "x1", true, 0.);
	error = error || parse_attrib_to_double(xml, &(block->x2), "x2", true, 0.);
	error = error || parse_attrib_to_double(xml, &(block->y1), "y1", true, 0.);
	error = error || parse_attrib_to_double(xml, &(block->y2), "y2", true, 0.);
	if(error) {
		ERROR("Error parsing block XML\n");
		return -1;
	}
	return 0;
}

int parse_custom_xml(xmlNode *xml, custom_t *custom) {
	int error = 0;

	error = error || parse_body_xml(xml, (body_t *)custom);
	int node_count = 0;
	xmlNode *xnode;
	// first, loop through children, and count up number of <node> elements
	for(xnode = xml->children; xnode != NULL; xnode = xnode->next) {
		if(xnode->type == XML_ELEMENT_NODE && !strcmp(xnode->name, "node") ) {
			node_count++;
		}
	}

	if(node_count < 2) {
		ERROR("Error: only found %d (x,y) nodes in custom element. Must specify at least 2\n", node_count);
		return -1;
	}

	// allocated memory for the (x,y) node data
	custom->node_x = malloc(node_count * sizeof(double));
	custom->node_y = malloc(node_count * sizeof(double));
	custom->node_owner = true;
	custom->node_count = node_count;

	// now loop through children again, this time parsing the (x,y) elmnts into arrays
	node_count = 0;
	int err = 0;
	for(xnode = xml->children; xnode != NULL; xnode = xnode->next) {
		if(xnode->type == XML_ELEMENT_NODE && !strcmp(xnode->name, "node") ) {
			err = 0;
			err = err || parse_attrib_to_double(xnode, &(custom->node_x[node_count]), "x", true, 0.);
			err = err || parse_attrib_to_double(xnode, &(custom->node_y[node_count]), "y", true, 0.);
			if(err) {
				ERROR("Error parsing custom body's x,y node\n");
				break;
			}
			node_count++;
		}
	}
	if(err) {
		free(custom->node_x);
		free(custom->node_y);
		custom->node_owner = false;
		error = 1;
	}

	if(error) {
		ERROR("Error parsing custom XML\n");
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

int parse_config_xml(xmlNode *xml) {
	xmlNode *curNode;
	printf("parsing config XML...\n");
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
		else if(!strcmp(curNode->name, "custom")) {
			DIE_IF_TOO_MANY_BODIES();
			DEBUG("Got <custom> element!\n");
			custom_t *cust = custom_alloc();
			custom_init(cust);
			if(parse_custom_xml(curNode, cust)) {
				ERROR("*** Error parsing \"custom\" XML into custom_t struct!\n");
				custom_dealloc(cust);
				continue;
			}
			app_data.bodies[app_data.num_bodies++] = (body_t *)cust;
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


int read_line(input_data_t *input) {
	char c;
	int ret;

	while( (ret = read(input->fd, &c, 1)) > 0) {
		if(c == '\n') { // end of line
			input->line_buf[input->line_length] = '\0';
			input->line_complete = true;
			break;
		}
		if(input->line_length >= input->line_capacity) {
			input->line_capacity = 2 * input->line_capacity + 1;
			input->line_buf = realloc(input->line_buf, input->line_capacity);
			if(input->line_buf == NULL) {
				ERROR("Error expanding input line buffer\n");
				exit(-1);
			}
		}
		input->line_buf[input->line_length++] = c;
	}
	if(ret == -1) { // nothing to read yet
		return 0;
	}
	if(ret == 0) { //EOF
		close(input->fd);
		DEBUG("End of input file reached\n");
		return -1;
	}
	if(input->line_complete) {
	}
	
}

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

void print_body_info(body_t *body) {
	printf("Body id %-4d: (%6g,%6g,%6g)\n", 
		body->id, body->x, body->y, body->theta);
}

#define X_USER_TO_PX(x) (x_m * (x) + x_b)
#define Y_USER_TO_PX(y) (y_m * (y) + y_b)
#define L_USER_TO_PX(l) fabs(x_m * (l))

gboolean draw_canvas(GtkWidget *widget, GdkEventExpose *event, gpointer data) {
	draw_ptr dp = app_data.gui.drawer;
	draw_start(dp); 
	int i, j;

	float width, height;
	draw_get_canvas_dims(dp, &width, &height);

	// first, fill with background color
	draw_set_color(dp, 1,1,1);
	draw_rectangle_filled(dp, 0, 0, width, height);

	float xmin = -10.0;
	float xmax = +10.0;
	float ymin = -10.0;
	float ymax = +10.0;

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
	
	// now draw the ground coordinate system
	draw_set_color(dp, 0.5,0.5,0.5);
	draw_line(dp, X_USER_TO_PX(0), Y_USER_TO_PX(ymin), X_USER_TO_PX(0), Y_USER_TO_PX(ymax));
	draw_line(dp, X_USER_TO_PX(xmin), Y_USER_TO_PX(0), X_USER_TO_PX(xmax), Y_USER_TO_PX(0));

	//draw_set_color(dp, 0, 0, 0);	
	//draw_text(dp, "hello world", 10, X_USER_TO_PX(0), Y_USER_TO_PX(0), ANCHOR_MIDDLE_MIDDLE);

	// draw all bodies
	for(i=0; i < app_data.num_bodies; i++) {
		body_t *body = app_data.bodies[i];
		float sin_ = sin(body->theta);
		float cos_ = cos(body->theta);
		switch(body->type) {
			case BODY_TYPE_BALL: {
				draw_set_color(dp, 1,0,0);
				float r = ((ball_t *)body)->radius;
				// draw the ball itself
				draw_circle_filled (
					dp, 
					X_USER_TO_PX(body->x), 
					Y_USER_TO_PX(body->y), 
					L_USER_TO_PX(r)
				);
				// draw a radial line on it to indicate rotation angle
				draw_set_color(dp, 0,0,0);
				draw_line (
					dp,
					X_USER_TO_PX(body->x),
					Y_USER_TO_PX(body->y),
					X_USER_TO_PX(body->x + r * cos_ ),
					Y_USER_TO_PX(body->y + r * sin_ )
				);
				break;
			}
			case BODY_TYPE_BLOCK: {
				block_t *block = (block_t *)body;
				draw_set_color(dp, 0,0,1);
				float x[4], y[4];
				x[0] = X_USER_TO_PX(body->x + block->x1 * cos_ - block->y1 * sin_);
				y[0] = Y_USER_TO_PX(body->y + block->x1 * sin_ + block->y1 * cos_);

				x[1] = X_USER_TO_PX(body->x + block->x2 * cos_ - block->y1 * sin_);
				y[1] = Y_USER_TO_PX(body->y + block->x2 * sin_ + block->y1 * cos_);

				x[2] = X_USER_TO_PX(body->x + block->x2 * cos_ - block->y2 * sin_);
				y[2] = Y_USER_TO_PX(body->y + block->x2 * sin_ + block->y2 * cos_);

				x[3] = X_USER_TO_PX(body->x + block->x1 * cos_ - block->y2 * sin_);
				y[3] = Y_USER_TO_PX(body->y + block->x1 * sin_ + block->y2 * cos_);
				draw_polygon_filled(dp, x, y, 4);
				break;
			}
			case BODY_TYPE_CUSTOM: {
				custom_t *cust = (custom_t *)body;
				draw_set_color(dp, 0,1,0);
				float *x = malloc(cust->node_count);
				float *y = malloc(cust->node_count);
				assert(x && y);

				int i;
				for(i=0; i<cust->node_count; i++) {
					x[i] = X_USER_TO_PX(body->x + cust->node_x[i] * cos_ - cust->node_y[i] * sin_);
					y[i] = Y_USER_TO_PX(body->y + cust->node_x[i] * sin_ + cust->node_y[i] * cos_);
				}
				draw_polygon_filled(dp, x, y, cust->node_count);

				free(x);
				free(y);
				break;
			}
			default:
				ERROR("Unsupported body type!\n");
				exit(-1);
		}
	}

	draw_finish(dp); 
	return TRUE;
}

gboolean update_func(gpointer data) {
	//printf("running update_func()\n");

	printf("frame #%05d:\n", app_data.active_frame_index);
	int j;
	frame_ptr_t p = app_data.frames[app_data.active_frame_index];

	/* loop over all input maps, stuffing the data
	 * in the frame into the proper destination location */
	for(j=0; j < app_data.num_input_maps; j++) {
		input_map_t *map = app_data.input_maps[j];
		switch(map->data_type) {
			case DATA_TYPE_DOUBLE:
				*((double *)(map->dest)) = *((double *)p);
				p += sizeof(double);
				break;
			default:
				ERROR("Unhandled data type!!!\n");
				exit(-1);
		}
	}

	// request a redraw of the canvas, which will redraw everything
  gtk_widget_queue_draw(app_data.gui.canvas);

	// advance frame counter
	app_data.active_frame_index++;
	if(app_data.active_frame_index >= app_data.num_frames) {
		app_data.active_frame_index = app_data.num_frames - 1;
	}

	return TRUE;
}

void init_gui(void) {
  GtkWidget *window;
  GtkWidget *v_box;
  GtkWidget *button;
  GtkWidget *save_button;

  gtk_init (NULL, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  v_box = gtk_vbox_new(FALSE, 10);
  gtk_container_add (GTK_CONTAINER (window), v_box);

  button = gtk_button_new_with_label("Pause");
  gtk_box_pack_start (GTK_BOX(v_box), button, FALSE, FALSE, 0);
  //g_signal_connect(button, "clicked", G_CALLBACK(button_activate), NULL);

  save_button = gtk_button_new_with_label("Capture Plot to File (PNG)");
  gtk_box_pack_start (GTK_BOX(v_box), save_button, FALSE, FALSE, 0);
  //g_signal_connect(save_button, "clicked", G_CALLBACK(save_button_activate), NULL);

	app_data.gui.dt_scale = gtk_hscale_new_with_range(0.00025, 0.05, 0.00025);
	gtk_scale_set_digits((GtkScale *)app_data.gui.dt_scale, 5);
	gtk_box_pack_start (GTK_BOX(v_box), app_data.gui.dt_scale, FALSE, FALSE, 0);

	gtk_range_set_value((GtkRange *)app_data.gui.dt_scale, 0.05);

  app_data.gui.canvas = gtk_drawing_area_new();
  gtk_widget_set_size_request(app_data.gui.canvas, 500,400);
  gtk_box_pack_start (GTK_BOX(v_box), app_data.gui.canvas, TRUE, TRUE, 0);
  g_signal_connect(app_data.gui.canvas, "expose_event", G_CALLBACK(draw_canvas), NULL);

	app_data.gui.drawer = draw_create(app_data.gui.canvas);

  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  gtk_widget_show_all (window);

	g_timeout_add(1000, update_func, NULL);
}

#define MAX_FIELDS 30
int main(int argc, char *argv[]) {

	if(argc < 2) {
		print_usage(stdout, argv[0]);
		return 0;
	}

	app_data_init(&app_data);

	/* this initialize the library and check potential ABI mismatches
	 * between the version it was compiled for and the actual shared
	 * library used. */
	LIBXML_TEST_VERSION

	xmlDocPtr doc;
	xmlNodePtr root;
	if(load_config(argv[1], &doc)) {
		printf("Error loading or valdiating XML config file!\n");
	}

	root = xmlDocGetRootElement(doc);
	//print_element_names(root, 0 );
	//printf("-------\n\n");
	//print_all_nodes(root, 0);
	//printf("-------\n\n");

	parse_config_xml(root);

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
	//int fd = open_file_nonblocking(infile);
	char line[2000];
	while(fgets(line, sizeof(line), fp) != NULL) {
		int i;
		char *fields[MAX_FIELDS];
		int field_count = split_line_into_fields(line, fields, MAX_FIELDS);
		if(field_count < 0) {
			continue;
		}

		#if 0
		printf("Got %d fields:\n", ret);
		for(i=0; i < field_count; i++) {
			printf("  %s\n", fields[i]);
		}
		#endif

		frame_ptr_t pframe = frame_alloc(app_data.bytes_per_frame);
		char *frame_pos = pframe;
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
						ERROR("Error parsing double from field!!\n");
						exit(-1);
					}
					printf("input_map #%d: column=%d, type=double, value=%g\n", i+1, map->field_num, d);
					*((double *)frame_pos) = d;
					frame_pos += sizeof(double);
					break;
				}
				default:
					ERROR("Unknown data type!\n");
			}
		}
		if(app_data.num_frames >= app_data.frames_capacity) {
			app_data.frames_capacity *= 3;
			app_data.frames = realloc(app_data.frames, sizeof(app_data.frames[0]));
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

	printf("Got %d frames\n", app_data.num_frames);
	app_data.active_frame_index = 0;
	
	printf("-------------------------------------\n");

	init_gui();
	gtk_main();
	
	printf("-------------------------------------\n");

	int i;
	for(i=0; i < app_data.num_frames; i++) {
		printf("frame #%05d:\n", i);
		int j;
		frame_ptr_t p = app_data.frames[i];

		/* loop over all input maps, stuffing the data
		 * in the frame into the proper destination location */
		for(j=0; j < app_data.num_input_maps; j++) {
			input_map_t *map = app_data.input_maps[j];
			switch(map->data_type) {
				case DATA_TYPE_DOUBLE:
					*((double *)(map->dest)) = *((double *)p);
					p += sizeof(double);
					break;
				default:
					ERROR("Unhandled data type!!!\n");
					exit(-1);
			}
		}

		/* Loop over all bodies (and eventually connectors, etc.),
		 * and (for now) print the body info */
		for(j=0; j < app_data.num_bodies; j++) {
			print_body_info(app_data.bodies[j]);
		}
	}

	return 0;
}

