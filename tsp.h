/*
    tsp.h

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

    External TSP solver wrapper / simple tsp solver if external 
    solver is not available.

*/
struct tsp *tsp_init ();
int tsp_solve (struct tsp *tsp);
int tsp_add (struct tsp *tsp, double x, double y, void *data);
int tsp_getnext (struct tsp *tsp, double *x, double *y, void **data);
int tsp_close (struct tsp *tsp);

typedef void tspdata;
