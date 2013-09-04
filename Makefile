
all: modviz draw_gtk_x11.o

modviz: main.c
	gcc main.c -o modviz `xml2-config --cflags` `xml2-config --libs`

draw_gtk_x11.o: draw_gtk_x11.c draw.h
	gcc -c -o draw_gtk_x11.o draw_gtk_x11.c \
		`pkg-config --cflags gtk+-2.0` \
		`pkg-config --cflags x11`
