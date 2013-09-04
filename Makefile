
all: modviz draw_gtk_x11.o

modviz: main.c draw_gtk_x11.o
	gcc main.c draw_gtk_x11.o -o modviz \
		`xml2-config --cflags` \
		`xml2-config --libs` \
		`pkg-config --cflags --libs gtk+-2.0` \
		`pkg-config --libs x11`

draw_gtk_x11.o: draw_gtk_x11.c draw.h
	gcc -c -o draw_gtk_x11.o draw_gtk_x11.c \
		`pkg-config --cflags gtk+-2.0` \
		`pkg-config --cflags x11`
