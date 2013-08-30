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
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

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

void print_xpath_nodes(xmlNodeSetPtr nodes) {
	xmlNodePtr cur;
	int size;
	int i;
    
	size = (nodes) ? nodes->nodeNr : 0;
    
	printf("Result (%d nodes):\n", size);
	for(i = 0; i < size; ++i) {
		if(nodes->nodeTab[i]->type == XML_NAMESPACE_DECL) {
			xmlNsPtr ns;
			ns = (xmlNsPtr)nodes->nodeTab[i];
			cur = (xmlNodePtr)ns->next;
			if(cur->ns) { 
				printf("= namespace \"%s\"=\"%s\" for node %s:%s\n", 
					ns->prefix, ns->href, cur->ns->href, cur->name);
			} else {
				printf("= namespace \"%s\"=\"%s\" for node %s\n", 
					ns->prefix, ns->href, cur->name);
			}
		} else if(nodes->nodeTab[i]->type == XML_ELEMENT_NODE) {
			cur = nodes->nodeTab[i];   	    
			if(cur->ns) { 
				printf("= element node \"%s:%s\"\n", 
					cur->ns->href, cur->name);
			} else {
				printf("= element node \"%s\"\n", cur->name);
			}
		} else {
			cur = nodes->nodeTab[i];    
			printf("= node \"%s\": type %d\n", cur->name, cur->type);
		}
	}
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


#if 0
	if(validate_args(argc, argv)) {
		return -1;
	}
	int i;
	int ret;
	int c;
	for(i=1; i<argc; i++) {
		printf("Reading from file: %s\n", argv[i]);
		int fd = open_file_nonblocking(argv[i]);
		while((ret = read(fd, &c, 1)) > 0) {
		} 
		
	}
#endif

	xmlDocPtr doc;
	xmlNodePtr root;
	if(load_config(argv[1], &doc)) {
		printf("Error loading or valdiating XML config file!\n");
	}

	root = xmlDocGetRootElement(doc);
	print_element_names(root, 0 );

	/* Create xpath evaluation context */
	xmlXPathContextPtr xpathCtx;
	xpathCtx = xmlXPathNewContext(doc);
	if(xpathCtx == NULL) {
		printf("Error: unable to create new XPath context\n");
		xmlFreeDoc(doc); 
		return(-1);
	}

	/* Evaluate xpath expression */
	xmlXPathObjectPtr xpathObj;
	xpathObj = xmlXPathEvalExpression(BAD_CAST "ball", xpathCtx);
	if(xpathObj == NULL) {
		printf("Error: unable to evaluate xpath expression\n");
		xmlXPathFreeContext(xpathCtx); 
		xmlFreeDoc(doc); 
		return(-1);
	}

	/* Print results */
	print_xpath_nodes(xpathObj->nodesetval);



	return 0;
}

