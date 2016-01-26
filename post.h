/*
    post.h

    This is part of pcb2g - pcb bitmap to G code converter

    Copyright (C) 2011- 2015 Peter Popovec, popovec@fei.tuke.sk

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Postprocesor wrapper functions (headers)

*/


struct postprocesor
{
  char *name;
  void *data;
  struct post_operations *ops;
  struct postprocesor *next;
  void *module;
};


enum POST_MACHINE_OPS
{
  MACHINE_IDLE,
  MACHINE_SETUP,
  MACHINE_ETCH,
  MACHINE_DRILL,
  MACHINE_CUT,
  MACHINE_END,
};


enum POST_SET
{
  POST_SET_X,
  POST_SET_Y,
  POST_SET_CUT,
  POST_SET_ETCH,
  POST_SET_ROUTE_RETRACT,
  POST_SET_HOLE_RETRACT,
  POST_SET_SAFE_TRAVERSE,
  POST_SET_CUTTER_COMP
};
/*
calling operations order:

open,

machine_operation(SETUP)

machine_operation(ETCH)
write_comment/spindle/rapid/route in mixed order (for etching)

machine_operation(IDLE)

machine_operation(DRILL)
write_comment/spindle/hole in mixed order (for drilling)


machine_operation(IDLE)

machine_operation(CUT)
write_comment/spindle/rapid/route in mixed order (for edge cuts)

machine_operation(IDLE)

machine_operation(END)
close
*/

struct post_operations
{
  void (*open) (struct postprocesor *, char *filename);	//set filename  and open  postprocesor output .. 
  void (*close) (struct postprocesor *);	//close postprocesor output

  void (*set) (struct postprocesor *, enum POST_SET op, va_list ap);
    
  void (*operation) (struct postprocesor *, enum POST_MACHINE_OPS op);

  void (*write_comment) (struct postprocesor *, char *);

  void (*route) (struct postprocesor *, double x, double y);
  void (*rapid) (struct postprocesor *, double x, double y);
  void (*route_arc) (struct postprocesor *, double x, double y, double cx, double cy, int dir);
  void (*hole) (struct postprocesor *, double x, double y, double dia);

};


struct postprocesor *postprocesor_register (struct post_operations *p,
					    void *data, char *name);

void postprocesor_init (void);

void postprocesor_open (char *filename);
void postprocesor_close (void);
void postprocesor_set(enum POST_SET op,...);
void postprocesor_set_dim(double x, double y);

void postprocesor_operation (enum POST_MACHINE_OPS op);

void postprocesor_write_comment (char *comment);
#ifdef _GNU_SOURCE
#define P_COMMENT(c,...) {char *tmp;asprintf(&tmp,c,__VA_ARGS__);postprocesor_write_comment(tmp);free(tmp);}
#endif
//void postprocesor_write_header (char *header);


void postprocesor_route (double x, double y);
void postprocesor_rapid (double x, double y);
void postprocesor_hole (double x, double y, double retract);
void postprocesor_route_arc(double x, double y, double cx, double cy, int dir);

