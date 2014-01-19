CFLAGS = -Wall -Wno-unused-function -Wno-pointer-sign -Iexternals/jbplot

LIBS = externals/jbplot/jbplot.o externals/jbplot/jbplot-marshallers.o

first_target: modviz_cairo

modviz_cairo: main.c draw_gtk_cairo.o cmdline.c cmdline.h
	gcc $(CFLAGS) main.c cmdline.c draw_gtk_cairo.o -o modviz_cairo \
		`xml2-config --cflags` \
		`xml2-config --libs` \
		`pkg-config --cflags --libs gtk+-2.0` \
		$(LIBS)

modviz_x11: main.c draw_gtk_x11.o cmdline.c cmdline.h
	gcc $(CFLAGS) main.c cmdline.c draw_gtk_x11.o -o modviz_x11 \
		`xml2-config --cflags` \
		`xml2-config --libs` \
		`pkg-config --cflags --libs gtk+-2.0` \
		`pkg-config --libs x11` \
		$(LIBS)

draw_gtk_cairo.o: draw_gtk_cairo.c draw.h
	gcc $(CFLAGS) -c -o draw_gtk_cairo.o draw_gtk_cairo.c \
		`pkg-config --cflags gtk+-2.0` \

draw_gtk_x11.o: draw_gtk_x11.c draw.h
	gcc $(CFLAGS) -c -o draw_gtk_x11.o draw_gtk_x11.c \
		`pkg-config --cflags gtk+-2.0` \
		`pkg-config --cflags x11`

cmdline.c cmdline.h: cmdline.ggo
	gengetopt -u < cmdline.ggo

.PHONY: clean
clean:
	rm -f *.o
	rm cmdline.c cmdline.h
