
all: modviz

modviz: main.c
	gcc main.c -o modviz `xml2-config --cflags` `xml2-config --libs`
