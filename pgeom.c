/*
    pgeom.c

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
#include <math.h>		//sqrt, HUGE_VAL
/*
  2D geometry code
*/

double
pointToPointDistance2D (double x1, double y1, double x2, double y2)
{
  double a, b;

  a = x2 - x1;
  b = y2 - y1;
  return sqrt (a * a + b * b);
}

/* 
   signed distance point to line
   for view from start of line to end if point is on left 
   return value is negative

   WARNING!, points Ax,Ay and Bx,By of line direction vector must be different!  
*/
double
lineToPointSDistance2D (double Ax, double Ay, double Bx, double By,
			double Px, double Py)
{
  double normalLength = sqrt ((Bx - Ax) * (Bx - Ax) + (By - Ay) * (By - Ay));

  return ((Bx - Ax) * (Ay - Py) - (Ax - Px) * (By - Ay)) / normalLength;
}


double
lineToPointDistance2D (double Ax, double Ay, double Bx, double By,
		       double Px, double Py)
{
  return fabs (lineToPointDistance2D (Ax, Ay, Bx, By, Px, Py));
}

/***
   *\brief projection from point to line
   *
   *calculate parameter "t" for line normal cross defined point
   *
   *\return parameter t 
   */

// line_to_point()
double
pontToLineProjection2D (double px, double py, double x1, double y1,
			double x2, double y2)
{
  double a, b;

  a = x2 - x1;
  b = y2 - y1;
  return (a * (px - x1) + b * (py - y1)) / (a * a + b * b);
}



/* point to line segment distance */
double
lineSegmentToPointDistance2D (double Ax, double Ay, double Bx, double By,
			      double Px, double Py)
{
  double dx, dy;
  double param;

//  param = pontToLineProjection2D (Px, Py, Ax, Ay, Bx, By);

  param = ((Px - Ax) * (Bx - Ax) + (Py - Ay) * (By - Ay));
  param /= (Bx - Ax) * (Bx - Ax) + (By - Ay) * (By - Ay);

  if (param > 1)
    param = 1;
  if (param < 0)
    param = 0;

  dx = Ax + param * (Bx - Ax) - Px;
  dy = Ay + param * (By - Ay) - Py;
  return sqrt (dx * dx + dy * dy);
}

/* line segment to point distance for point paralel to line segment
   if point projection on line is out of segment, return HUGE_VAL
 */
double
lineParaSegmentToPointDistance2D (double Ax, double Ay, double Bx, double By,
				  double Px, double Py)
{
  double dx, dy;
  double param;

//  param = pontToLineProjection2D (Px, Py, Ax, Ay, Bx, By);

  param = ((Px - Ax) * (Bx - Ax) + (Py - Ay) * (By - Ay));
  param /= (Bx - Ax) * (Bx - Ax) + (By - Ay) * (By - Ay);
  if (param > 1)
    return HUGE_VAL;
  if (param < 0)
    return HUGE_VAL;

  dx = Ax + param * (Bx - Ax) - Px;
  dy = Ay + param * (By - Ay) - Py;
  return sqrt (dx * dx + dy * dy);
}




/***
  *
  * calculate distance from line segment to line segment
  */
double
lineSegmentToLineSegment2D (double x0, double y0, double x1, double y1,
			    double x2, double y2, double x3, double y3)
{

  double xa, ya, xb, yb, a, b, t;
  int f1 = 0, f2 = 0;


  if (x0 == x1 && y0 == y1)	// segment 1 has zero length
    f1 = 1;
  if (x2 == x3 && y2 == y3)	// segment 2 has zero length
    f2 = 1;

  if (f1 && f2)			// both segments have zero length
    return pointToPointDistance2D (x0, y0, x2, y2);
  // only 1 segment has zero length
  if (f1)
    {
      t = pontToLineProjection2D (x0, y0, x2, y2, x3, y3);
      if (t < 0)
	t = 0;
      if (t > 1)
	t = 1;
      return pointToPointDistance2D (x0, y0, x2 + t * (x3 - x2),
				     y2 + t * (y3 - y2));
    }
  if (f2)
    {
      t = pontToLineProjection2D (x2, y2, x0, y0, x1, y1);
      if (t < 0)
	t = 0;
      if (t > 1)
	t = 1;
      return pointToPointDistance2D (x2, y2, x0 + t * (x1 - x0),
				     y0 + t * (y1 - y0));
    }

  // no segments have zero length
  t = (y3 - y2) * (x1 - x0) - (x3 - x2) * (y1 - y0);
  a = (x3 - x2) * (y0 - y2) - (y3 - y2) * (x0 - x2);

  if (t != 0)
    {				// not parallel
      a /= t;
      b = (x1 - x0) * (y0 - y2) - (y1 - y0) * (x0 - x2);
      b /= t;

      if (a >= 0 && a <= 1 && b >= 0 && b <= 1)	// segment intersection
	{
	  //TODO calculate intersection
	  return 0;
	}
      else
	{			// no segment intersection
	  t = a;
	  if (t < 0)
	    t = 0;
	  if (t > 1)
	    t = 1;
	  xa = x0 + t * (x1 - x0);
	  ya = y0 + t * (y1 - y0);
	  t = pontToLineProjection2D (xa, ya, x2, y2, x3, y3);
	  if (t < 0)
	    t = 0;
	  if (t > 1)
	    t = 1;
	  xa = x2 + t * (x3 - x2);
	  ya = y2 + t * (y3 - y2);
	  if (t < 0)
	    t = 0;
	  if (t > 1)
	    t = 1;
	  xb = x2 + t * (x3 - x2);
	  yb = y2 + t * (y3 - y2);
	  t = pontToLineProjection2D (xb, yb, x0, y0, x1, y1);
	  if (t < 0)
	    t = 0;
	  if (t > 1)
	    t = 1;
	  xb = x0 + t * (x1 - x0);
	  yb = y0 + t * (y1 - y0);

	  return pointToPointDistance2D (xa, ya, xb, yb);
	}
    }
  else if (a != 0)
    {				// parallel
      a = pontToLineProjection2D (x0, y0, x2, y2, x3, y3);

      if (a <= 1 && a >= 0)
	{
	  t = a;
	  xa = x2 + t * (x3 - x2);
	  ya = y2 + t * (y3 - y2);
	  xb = x0;
	  yb = y0;
	}
      else
	{
	  t = a;
	  if (t < 0)
	    t = 0;
	  if (t > 1)
	    t = 1;
	  xa = x2 + t * (x3 - x2);
	  ya = y2 + t * (y3 - y2);
	  t = pontToLineProjection2D (xa, ya, x0, y0, x1, y1);
	  if (t < 0)
	    t = 0;
	  if (t > 1)
	    t = 1;
	  xb = x0 + t * (x1 - x0);
	  yb = y0 + t * (y1 - y0);
	}

      return pointToPointDistance2D (xa, ya, xb, yb);
    }
  else				// colinear
    {
      //TODO calculate start and end point of overlap
      return 0;
    }
}


// arc in G-code notation, S start point, E end point (arc chord  is from S to E)
// R is arc center relative to S, dir direction 0 = clockwise 1 = counterclockwise
// (G2 = dir 0, G3 = dir 1) 

double
arcToPointDistance2D (double Sx, double Sy, double Ex, double Ey,
		      double Rx, double Ry, int dir, double Px, double Py)
{


  double Cx, Cy, radius;
  double Sdist, Edist, Pdist;
  double Spos, Epos;

//center position
  Cx = Sx + Rx;
  Cy = Sy + Ry;

  radius = pointToPointDistance2D (0, 0, Rx, Ry);
  Sdist = pointToPointDistance2D (Sx, Sy, Px, Py);
  Edist = pointToPointDistance2D (Ex, Ey, Px, Py);
//Pdist = projection distance, line from point to center of arc defines point on arc circle
//        point to point on arc circle = Pdist 

  Pdist = fabs (pointToPointDistance2D (Cx, Cy, Px, Py) - radius);

// we have line  from POINT to CENTER, determine position of START and END to this line
  Spos = (Cx - Px) * (Py - Sy) - (Px - Sx) * (Cy - Py);
  Epos = (Cx - Px) * (Py - Ey) - (Px - Ex) * (Cy - Py);

  //ARC intersect  POINTT -> ARC CENTER line ? 
  //if position of arc START  and END  does have different singum and 
  //rotation direction match, ARC intersect line from POINT to CENTER
  if (Spos <= 0 && Epos >= 0 && dir)
    return Pdist;
  if (Spos >= 0 && Epos <= 0 && !dir)
    return Pdist;
  //line from POINT to CENTER does not intersect arc
  return Sdist < Edist ? Sdist : Edist;
}

//if projection point is on line, due to rounding errror small 
//value must be used to compare "t" parameter 
#define ZERO_T  1E-30

double
arcToLineDistance2D (double Sx, double Sy, double Ex, double Ey,
		     double Rx, double Ry, int dir, double Ax, double Ay,
		     double Bx, double By)
{
  double t, Cx, Cy, Px, Py, radius;
  double Spos, Epos;
  double Sdist, Edist;

// center position
  Cx = Sx + Rx;
  Cy = Sy + Ry;

// projection of arc center to line
  t = pontToLineProjection2D (Cx, Cy, Ax, Ay, Bx, By);
  Px = Ax + t * (Bx - Ax);
  Py = Ay + t * (By - Ay);

  radius = pointToPointDistance2D (0, 0, Rx, Ry);

  //t!=0 if line does not intersect center, or line is outside radius 
  if (fabs (t) > ZERO_T || radius <= pointToPointDistance2D (Px, Py, Cx, Cy))
// TODO calculate one point intersection if radius = pointToPointDistance2D...
    return arcToPointDistance2D (Sx, Sy, Ex, Ey, Rx, Ry, dir, Px, Py);

// line intersect arc circle
// we have line  from POINT to CENTER, determine position of START and END to this line
  Spos = (Cx - Px) * (Py - Sy) - (Px - Sx) * (Cy - Py);
  Epos = (Cx - Px) * (Py - Ey) - (Px - Ex) * (Cy - Py);

  //ARC intersect  POINT -> ARC CENTER line ? 
  //if position of arc START  and END  does have different singum and 
  //rotation direction match, ARC intersect line from POINT to CENTER

  //TODO calculate intersection points
  if (Spos <= 0 && Epos >= 0 && dir)
    return 0;
  if (Spos >= 0 && Epos <= 0 && !dir)
    return 0;

  Sdist = lineToPointDistance2D (Ax, Ay, Bx, By, Sx, Sy);
  Edist = lineToPointDistance2D (Ax, Ay, Bx, By, Ex, Ey);

  return Sdist < Edist ? Sdist : Edist;
}

/* TODO
double
arcToLineSegmentDistance2D (double Sx, double Sy, double Ex, double Ey,
			    double Rx, double Ry, int dir, double Ax,
			    double Ay, double Bx, double By)
{

  double Cx, Cy, radius;
  int f1 = 0, f2 = 0;
  double Px, Py;
  double Sdist, Edist;
  double t;
  double Spos, Epos;

// center position
  Cx = Sx + Rx;
  Cy = Sy + Ry;

  radius = pointToPointDistance2D (0, 0, Rx, Ry);
  if (pointToPointDistance2D (Cx, Cy, Ax, Ay) < radius)
    f1 = 1;
  if (pointToPointDistance2D (Cx, Cy, Bx, By) < radius)
    f2 = 1;
  //if startline and endline is inside arc circle
  if (f1 && f2)
    {
      Sdist = arcToPointDistance2D (Sx, Sy, Ex, Ey, Rx, Ry, dir, Ax, Ay);
      Edist = arcToPointDistance2D (Sx, Sy, Ex, Ey, Rx, Ry, dir, Bx, By);
      return Sdist < Edist ? Sdist : Edist;
    }
// projection of arc center to line
  t = pontToLineProjection2D (Cx, Cy, Ax, Ay, Bx, By);
  Px = Ax + t * (Bx - Ax);
  Py = Ay + t * (By - Ay);

  //if startline is outside arc circe  and endline inside arc circle
  //or endline outside ans startline inside, one intersection may exists   
  if (f1 || f2)
    {
// we have line  from POINT to CENTER, determine position of START and END to this line
      Spos = (Cx - Px) * (Py - Sy) - (Px - Sx) * (Cy - Py);
      Epos = (Cx - Px) * (Py - Ey) - (Px - Ex) * (Cy - Py);

      //ARC intersect  POINT -> ARC CENTER line ? 
      //if position of arc START  and END  does have different singum and 
      //rotation direction match, ARC intersect line from POINT to CENTER

      //TODO calculate intersection points
      if (Spos <= 0 && Epos >= 0 && dir)
	return 0;
      if (Spos >= 0 && Epos <= 0 && !dir)
	return 0;
      //line does not intersect arc
      //min from(arc start to line, arc end to line, line start to arc, line end to arc)
      Sdist = lineSegmetnoToPointDistance2D (Ax, Ay, Bx, By, Sx, Sy);
      Edist = lineSegmetnoToPointDistance2D (Ax, Ay, Bx, By, Ex, Ey);
      if (Edist < Sdist)
	Sdist = Edist;
      Edist = arcToPointDistance2D (Sx, Sy, Ex, Ey, Rx, Ry, dir, Ax, Ay);
      if (Edist < Sdist)
	Sdist = Edist;
      Edist = arcToPointDistance2D (Sx, Sy, Ex, Ey, Rx, Ry, dir, Bx, By);
      if (Edist < Sdist)
	Sdist = Edist;
      return Sdist;
    }

  //startline and endline is outside arc circle  
  //check if line intersect center
  if (fabs (t) < ZERO_T)
    {
      //line intersect center,
      //get start or end point of line segment  coser to arc
      if (pointToPointDistance2D (Cx, Cy, Ax, Ay) <
	  pointToPointDistance2D (Cx, Cy, Bx, By))
	return arcToPointDistance2D (Sx, Sy, Ex, Ey, Rx, Ry, dir, Ax, Ay);
      else
	return arcToPointDistance2D (Sx, Sy, Ex, Ey, Rx, Ry, dir, Bx, By);
    }
  //check, if projection of center of arc to line is outside arc circle
  if (pointToPointDistance (Px, Py, Cx, Cy) >= radius)
    {
      // calculate arc to point distance to startline, endline and 
      // projectionpoint  and return minimum
      Sdist = ArcToPointDistance2D (Sx, Sy, Ex, Ey, Rx, Ry, dir, Ax, Ay);
      Edist = arcToPointDistance2D (Sx, Sy, Ex, Ey, Rx, Ry, dir, Bx, By);
      if (Edist < Sdist)
	Sdist = Edist;
      Edist = arcToPointDistance2D (Sx, Sy, Ex, Ey, Rx, Ry, dir, Px, Py);
      if (Edist < Sdist)
	Sdist = Edist;
      return Sdist;
    }
  //line intersect arc circle
  if (Spos <= 0 && Epos >= 0 && dir)
    return 0;			//TODO caclucate intersection point/points
  if (Spos >= 0 && Epos <= 0 && !dir)
    return 0;			//TODO caclucate intersection point/points
//  line segment does not intersect arc
  return Sdist < Edist ? Sdist : Edist;
}

*/
// TODO arc to arc


/*
if r1+r2 < center1 - center2  //intersection does not exists
  Dist1 = center1 to arc2
  Dist2 = center2 to arc1

  Dist3 = center1-center2
  return Dist1+Dist2-Dist3;

if radius1+(center1-center2)<radius2  arc1 is inside arc2

//TODO others
//center of arc circle1 is inside arc circle2

  



*/


// circle/arc center from 3 points 
// point A, B ,C, connect AB, BC, create perpendicular lines in center of AB, BC, intersection is center
// partialy tested

void
circle2D_center_from_3 (double Ax, double Ay, double Bx, double By,
			double Cx, double Cy, double *x, double *y)
{
  double yDelta_a = By - Ay;
  double xDelta_a = Bx - Ax;
  double yDelta_b = Cy - By;
  double xDelta_b = Cx - Bx;

  double aSlope, bSlope;

  double AB_Mid_x = (Ax + Bx) / 2.0;
  double AB_Mid_y = (Ay + By) / 2.0;
  double BC_Mid_x = (Bx + Cx) / 2.0;
  double BC_Mid_y = (By + Cy) / 2.0;

  if (yDelta_a == 0)
    {
      *x = AB_Mid_x;
      if (xDelta_b == 0)
	*y = BC_Mid_y;
      else
	{
	  bSlope = yDelta_b / xDelta_b;
	  *y = BC_Mid_y + (BC_Mid_x - *x) / bSlope;
	}
      return;
    }

  if (yDelta_b == 0)
    {
      *x = BC_Mid_x;
      if (xDelta_a == 0)
	*y = AB_Mid_y;
      else
	{
	  aSlope = yDelta_a / xDelta_a;
	  *y = AB_Mid_y + (AB_Mid_x - *x) / aSlope;
	}
      return;
    }

  if (xDelta_a == 0)
    {
      *y = AB_Mid_y;
      bSlope = yDelta_b / xDelta_b;
      *x = bSlope * (BC_Mid_y - *y) + BC_Mid_x;
      return;
    }
  if (xDelta_b == 0)
    {
      *y = BC_Mid_y;
      aSlope = yDelta_a / xDelta_a;
      *x = aSlope * (AB_Mid_y - *y) + AB_Mid_x;
    }
  aSlope = yDelta_a / xDelta_a;
  bSlope = yDelta_b / xDelta_b;
  *x = (aSlope * bSlope * (AB_Mid_y - BC_Mid_y)
	- aSlope * BC_Mid_x + bSlope * AB_Mid_x) / (bSlope - aSlope);
  *y = AB_Mid_y - (*x - AB_Mid_x) / aSlope;
}
