/*
    pcb2g.h

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

    pcb2g.h main header file

*/
/* insert stdio.h, FILE is needed inside struct image */
#ifndef _STDIO_H
#include <stdio.h>
#endif

#ifndef _TIME_H
#include <time.h>
#endif

struct tool_param {
  char *name;	//tool description
  int  pocket;	//optionaly pocket number in tool charger (not used)
  double dia;	//tool diameter (mm)
  double r_speed;	//tool radial speed (etching cutting)
  double a_speed;	//tool axial speed (drilling)
  double rpm;		//tool RPM
};

struct image
{
  unsigned char *data;
  unsigned char *data_orig;
  unsigned char *mask_data;
  unsigned char *or_mask;
  int size;			//x*(y-2)
  int x, y;
  int dpi;
  double real_x, real_y;	//real X and Y dimension
  char crc[5];			//16bit crc in ascii + terminating '\0'
  char *drill_file;
  char *image_file;
  char *output_file;		//output filename
  char *cut_file;		//filename fro cut file (coordinates of cut polygon)
  int route_border;		//route board border
  int auto_border;		//border for non retrangular PCB
  int route_optimize;		//switch for  routes optimization
  double safe_traverse;		//safe traverse over wise etc.. default 10mm
  double route_retract;		//default 2
  double hole_retract;		//default 5
  double cnc_G64P;		//G64 P parameter (0.01)
  double cnc_G64Q;		//G64 Q parameter (0.01)

//TOOLs parameters
  struct tool_param cut;
  struct tool_param etch;
  struct tool_param drill;

  double hole_asymmetry;	//if hole is detected, check asymetry, Hole is detected if asymetry is below this value
  double drill_last_x;		//last point in drill cycle
  double drill_last_y;		// to continue routing in minimal rapid path
  double route_last_x;		//last point in route
  double route_last_y;		//

  struct timespec ts;		//time for profiling  
  int debug_files;		//enable/disable debug files creation

  char *commandline;
  char *comment;
  char *mtime;
  struct hole *first_hole;
  struct polyline *first_polyline;
  struct multigraph *first_mg;

/* statistical */
  double hole_line_min;		//minimal distance hole to division line in real units


};


struct hole
{
//from fill algo:
  int xmin, xmax;
  int ymin, ymax;
  int count;
  int level;
//calculated from xmin,xmax ymin,ymax
  int center[2];
// temporary flag for procedures
  int flag;
//all groups for this hole
  struct h_group *h_group;
// 0 if fx,fy is calculated from image
// 1 if drill file is used to set up fx,fy

  int drill_file;
//final hole position and diameter in mm (calculated or from drill file)
  double fx, fy, dia;


  struct hole *next;

/* statistical */
  double hole_to_line;		//minimal distance from center to isolation line 
};

//specify flag M_GRAY to not convert gray image to bw image
#define M_GRAY 1
int input_img_read (struct image *image,int flags);

void create_border (struct image *image);
int img_write (struct image *image, char *name, char *comment);
int debug_write (struct image *image, char *name);
double bres_line_test (struct image *image, int x0, int y0, int x1, int y1);


struct fill_data
{
  int xmin, xmax;
  int ymin, ymax;
  int count;
  int level;
};

struct fill_data *image_fill (struct image *image, int vx, int vy,
			      unsigned char fill_data, unsigned char border);

#define C_HOLE 5
#define C_HOLE_ALIGNED 6
//all below 64 is copper
#define C_COPPER 64


void recolor (struct image *image, int fill_color, int level);
int expand_copper (struct image *image);


double i2realX (struct image *image, int x);
double i2realY (struct image *image, int y);

int trace (struct image *image);

void dump_lines (struct image *image);


void create_hole (struct image *image, struct fill_data *vd);
void dump_holes (struct image *image);
void get_drill_file (struct image *image);
void mark_holes (struct image *image);
struct timespec tsx_diff (struct timespec start, struct timespec end);


extern int debug_level;
#define VOPT 1

void optim (struct image *image);
void create_cut (char *filename);
