/*
    polyline.c

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

    Polyline creating/changing/deleting/searching functions

*/


/*
in culloms polylines, in rows polilines points
  
   x,y            x,y
   ppoints   ->   NULL
   next         
     |
     v
   x,y             x,y         x,y
   points    ->    next   ->   next
   NULL

*/


struct polyline_point
{
  int x, y;
  int flag;			//if not 0, do not append to this point
  struct polyline_point *next;
  struct polyline *head;
  double mindistance;		//minimum distance from this line to other points
};

struct polyline
{
  //int start_x,start_y;        //start is accesible by path->x, path->y
  int end_x, end_y;
  struct polyline_point *points;
  struct polyline *next;
  int used;			//for dump
  int used_start;		//for dump
  int used_end;			//for dump
  struct polyline_tree *tree;

  int optimized_count;		//for optimization
};

struct polyline_tree
{
  struct polyline_point *start;
  struct polyline_point *end;
  double len;			//real len of all lines in this segment
  double air_len;		//air len of this segment
  double max_dev;		//maximum deviation 
  double aver_dev;
  double quad_dev;
  int components;		//how many lines in this tree
  int plus, minus;		//how many lines to left or  right
  double sig;			//signifikanci

  int diagonal;			//0 not diagonal, 1 posible diagonal line
  int direct;			//direct < 0  do not optimize by straight line (some of holes is closer)
  //direct > 0  can be optimized by direct line
  struct polyline_tree *left;
  struct polyline_tree *right;

  int level;			//recursion level
};

void create_line (struct image *image, int x1, int y1, int x2, int y2);
void polyline_reverse (struct polyline *p);
void polyline_free_all (struct image *image);
void polyline_start (struct image *image, int x, int y);
void polyline_extend (struct image *image, int x, int y);
void polyline_append (struct image *image, int x, int y);
int polyline_find_append (struct image *image, int x, int y, int rx, int ry);
void polyline_delete_default (struct image *image);
void svg_debug (struct image *image);
void polylines_dump (struct image *image);
void polyline_delete_m_line (struct image *image, int x1, int y1, int x2,
			     int y2);
void polyline_statist (struct image *image);
