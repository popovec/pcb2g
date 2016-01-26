/*
    pgeom.h

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

    geometry functions (2D)
*/
double pointToPointDistance2D (double x1, double y1, double x2, double y2);

double
lineToPointSDistance2D (double Ax, double Ay, double Bx, double By,
			double Px, double Py);
double
lineToPointDistance2D (double Ax, double Ay, double Bx, double By,
		       double Px, double Py);
double
pontToLineProjection2D (double px, double py, double x1, double y1,
			double x2, double y2);
double
lineSegmentToPointDistance2D (double Ax, double Ay, double Bx, double By,
			      double Px, double Py);
double
lineParaSegmentToPointDistance2D (double Ax, double Ay, double Bx, double By,
				  double Px, double Py);
double
lineSegmentToLineSegment2D (double x0, double y0, double x1, double y1,
			    double x2, double y2, double x3, double y3);

double
arcToPointDistance2D (double Sx, double Sy, double Ex, double Ey,
		      double Rx, double Ry, int dir, double Px, double Py);
