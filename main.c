#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

#define PRINT_DEBUGS 1
#define PRINT_ERRORS 1

#if PRINT_DEBUGS
	#define DEBUG(...) printf(__VA_ARGS__)
#else
	#define DEBUG(...) 
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

#define MAX_BODIES     500
#define MAX_CONNECTORS 500

typedef struct _app_data_t {
	body_t *bodies[MAX_BODIES];
	int num_bodies;

	connector_t *connectors[MAX_CONNECTORS];
	int num_connectors;
} app_data_t;

static app_data_t app_data;

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
	if(strcmp(fname, "-") != 0) { // dash denotes STDIN
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

int parse_attrib_to_int(xmlNode *xml, int *dest, char *attrib_name, bool required, int dflt) {
	char *val_str = xmlGetProp(xml, attrib_name);
	if(val_str == NULL) {
		if(required) {
			ERROR("Error: No \"%s\" attribute specified (it's required!)\n", attrib_name);
			return -1;
		}
		else {
			*dest = dflt;
			DEBUG("Didn't find \"%s\" attribute. Using default value (%d) instead...\n", attrib_name, dflt);
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
	printf("successfully parsed the \"%s\" attribute into an integer (%d)\n", attrib_name, i);
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
			DEBUG("Didn't find \"%s\" attribute. Using default value (%g) instead...\n", attrib_name, dflt);
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
	DEBUG("successfully parsed the \"%s\" attribute into a double (%g)\n", attrib_name, d);
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
			DEBUG (
				"Didn't find \"%s\" attribute. Using default value (%s) instead...\n", 
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
		"successfully parsed the \"%s\" attribute into a boolean (%s)\n", 
		attrib_name, (b ? "TRUE" : "FALSE")
	);
	return 0;
}

int parse_body_xml(xmlNode *xml, body_t *body) {
	int error = 0;

	error = error || parse_attrib_to_int(xml, &body->id, "id", false, -1);
	error = error || parse_attrib_to_bool(xml, &body->show_cs, "show_cs", false, false);
	error = error || parse_attrib_to_bool(xml, &body->show_name, "show_name", false, false);
	error = error || parse_attrib_to_bool(xml, &body->show_id, "show_id", false, false);
	error = error || parse_attrib_to_bool(xml, &body->filled, "filled", false, true);
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

int parse_config_xml(xmlNode *xml) {
	xmlNode *curNode;
	printf("parsing config XML...\n");
	for(curNode = xml->children; curNode != NULL; curNode = curNode->next) {
		if(curNode->type != XML_ELEMENT_NODE) {
			//printf("skipping non-Element node...\n");
			continue;
		}
		if(!strcmp(curNode->name, "ball")) {
			ball_t ball;
			printf("Got \"ball\" element!\n");
			parse_ball_xml(curNode, &ball);
		}
	}
	return 0;
}

int main(int argc, char *argv[]) {

	if(argc < 2) {
		printf("Usage: %s XML_CONFIG_FILE [DATAFILE]\n", argv[0]);
		return 0;
	}

	/*
	 * this initialize the library and check potential ABI mismatches
	 * between the version it was compiled for and the actual shared
	 * library used.
	 */
	LIBXML_TEST_VERSION

	ball_t ball_1;
	ball_init(&ball_1);
	body_set_name((body_t *)&ball_1, "hello_world");

	custom_t *pcust_1;
	pcust_1 = custom_alloc();
	custom_init(pcust_1);
	body_set_name((body_t *)pcust_1, "custom_1");

	//printf("ball_1's name is: \"%s\"\n", ((body_t *)&ball_1)->name);
	//printf("cust_1's name is: \"%s\"\n", ((body_t *)pcust_1)->name);

	xmlDocPtr doc;
	xmlNodePtr root;
	if(load_config(argv[1], &doc)) {
		printf("Error loading or valdiating XML config file!\n");
	}

	root = xmlDocGetRootElement(doc);
	print_element_names(root, 0 );
	printf("-------\n\n");
	print_all_nodes(root, 0);

	printf("-------\n\n");
	parse_config_xml(root);
	return 0;
}

