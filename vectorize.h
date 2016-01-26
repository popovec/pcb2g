/*
    vectorize.h

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

    Path planer for G code (optimization for rapisd etc..)
*/

struct multigraph
{
  int x, y;			//pixel positions
  double rx, ry;		//real positions (in mm or inch), to speed up real distance calculations
  int used;			//for dumping .. 
  int dh;			//for drake_hougardy

  int count;			//same as edge_count, but this variable is decremented
                                //in euler dumping code
//router path to adjanced nodes
  struct multigraph *neighbor[4];	//targets of edges
  struct polyline *edge[4];		//for now maximum 4 edges + rapid
//rapid to next node to get euler graph
  struct multigraph *rapid;

// pointer to next in linked list .. 
  struct multigraph *next;
};
