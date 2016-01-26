all:	pcb2g post_linuxcnc.so post_svg.so

VERSION=$(shell stat -c %Y *.c *.h Makefile|sort -n|tail -n 1)

FLAGS = -g -O2

pcb2g:	pcb2g.o image.o fill.o expand.o trace.o vectorize.o pgeom.o optim.o holes.o tsp.o polyline.o postprocesor.o cut.o
	cc -Wall $(FLAGS) -rdynamic -ldl -lm pcb2g.o image.o fill.o expand.o trace.o vectorize.o pgeom.o optim.o holes.o tsp.o polyline.o postprocesor.o cut.o -lrt -o pcb2g

pcb2g.o:	pcb2g.c pcb2g.h post.h
		cc -Wall $(FLAGS) -DVERSION=$(VERSION) pcb2g.c -c -o pcb2g.o

image.o:	image.c pcb2g.h
		cc -Wall $(FLAGS) -c -o image.o image.c

fill.o:		fill.c pcb2g.h
		cc -Wall $(FLAGS) -c -o fill.o fill.c

expand.o:	expand.c pcb2g.h
		cc -Wall $(FLAGS) -c -o expand.o expand.c

trace.o:	trace.c pcb2g.h
		cc -Wall $(FLAGS) -c -o trace.o trace.c

polyline.o:	polyline.c polyline.h pcb2g.h
		cc -Wall $(FLAGS) -c -o polyline.o polyline.c

vectorize.o:	vectorize.c pcb2g.h vectorize.h polyline.h post.h
		cc -Wall $(FLAGS) -c -o vectorize.o vectorize.c

pgeom.o:	pgeom.h pgeom.c
		cc -Wall $(FLAGS) -c -o pgeom.o pgeom.c

optim.o:	optim.c pcb2g.h vectorize.h pgeom.h polyline.h
		cc -Wall $(FLAGS) -c -o optim.o optim.c

holes.o:	holes.c pcb2g.h tsp.h post.h
		cc -Wall $(FLAGS) -c -o holes.o holes.c

tsp.o:		tsp.c tsp.h
		cc -Wall $(FLAGS) -c -o tsp.o tsp.c

cut.o:		cut.c pcb2g.h post.h
		cc -Wall $(FLAGS) -c -o cut.o cut.c

postprocesor.o:	postprocesor.c post.h
		cc -Wall $(FLAGS) -c -o postprocesor.o postprocesor.c

post_linuxcnc.so:	post_linuxcnc.c post.h
		cc -Wall $(FLAGS) -fPIC -c -o post_linuxcnc.o post_linuxcnc.c
		cc -Wall $(FLAGS) -shared -o post_linuxcnc.so post_linuxcnc.o

post_svg.so:	post_svg.c post.h
		cc -Wall $(FLAGS) -fPIC -c -o post_svg.o post_svg.c
		cc -Wall $(FLAGS) -shared -o post_svg.so post_svg.o


clean:
	rm -f *~
	rm -f *.o
	rm -f *.so
	rm -f pcb2g
	