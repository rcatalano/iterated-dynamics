/*
	zoom.cpp - routines for zoombox manipulation and for panning

*/
#include <string.h>

#include "port.h"
#include "prototyp.h"

#include "biginit.h"
#include "calcfrac.h"
#include "drivers.h"
#include "Externals.h"
#include "fracsubr.h"
#include "framain2.h"
#include "miscfrac.h"
#include "resume.h"
#include "StopMessage.h"
#include "zoom.h"

#include "EscapeTime.h"
#include "FrothyBasin.h"
#include "MathUtil.h"
#include "ViewWindow.h"
#include "WorkList.h"
#include "ZoomBox.h"

static double const PIXELROUND = 0.00001;

static void zmo_calc(double, double, double *, double *, double);
static void zmo_calcbf(bf_t, bf_t, bf_t, bf_t, bf_t, bf_t, bf_t, bf_t, bf_t);
static int  check_pan();
static void fix_work_list();
static void move_row(int fromrow, int torow, int col);

// big number declarations
static void calculate_corner(bf_t target, bf_t p1, double p2, bf_t p3, double p4, bf_t p5)
{
	bf_t btmp1(g_rbf_length);
	bf_t btmp2(g_rbf_length);
	bf_t btmp3(g_rbf_length);
	BigStackSaver savedStack;

	// use target as temporary variable
	floattobf(btmp3, p2);
	multiply_bf(btmp1, btmp3, p3);
	multiply_bf(btmp2, floattobf(target, p4), p5);
	add_bf(target, btmp1, btmp2);
	add_a_bf(target, p1);
}

void zoom_box_draw(bool drawit)
{
	Coordinate tl;
	Coordinate bl;
	Coordinate tr;
	Coordinate br; // dot addr of topleft, botleft, etc
	double tmpx;
	double tmpy;
	double dx;
	double dy;
	double rotcos;
	double rotsin;
	double ftemp1;
	double ftemp2;
	double fxwidth;
	double fxskew;
	double fydepth;
	double fyskew;
	double fxadj;
	bf_t bffxwidth;
	bf_t bffxskew;
	bf_t bffydepth;
	bf_t bffyskew;
	bf_t bffxadj;
	if (g_z_width == 0)  // no box to draw
	{
		if (g_zoomBox.count() != 0)  // remove the old box from display
		{
			g_zoomBox.clear();
			g_zoomBox.set_count(0);
		}
		reset_zoom_corners();
		return;
	}
	BigStackSaver savedStack;
	if (g_bf_math)
	{
		bffxwidth = bf_t(g_rbf_length);
		bffxskew = bf_t(g_rbf_length);
		bffydepth = bf_t(g_rbf_length);
		bffyskew = bf_t(g_rbf_length);
		bffxadj = bf_t(g_rbf_length);
	}
	ftemp1 = MathUtil::Pi*g_z_rotate/72; // convert to radians
	rotcos = std::cos(ftemp1);   // sin & cos of rotation
	rotsin = std::sin(ftemp1);

	// do some calcs just once here to reduce fp work a bit
	fxwidth = g_sx_max-g_sx_3rd;
	fxskew  = g_sx_3rd-g_sx_min;
	fydepth = g_sy_3rd-g_sy_max;
	fyskew  = g_sy_min-g_sy_3rd;
	fxadj   = g_z_width*g_z_skew;

	if (g_bf_math)
	{
		// do some calcs just once here to reduce fp work a bit
		subtract_bf(bffxwidth, g_sx_max_bf, g_sx_3rd_bf);
		subtract_bf(bffxskew, g_sx_3rd_bf, g_sx_min_bf);
		subtract_bf(bffydepth, g_sy_3rd_bf, g_sy_max_bf);
		subtract_bf(bffyskew, g_sy_min_bf, g_sy_3rd_bf);
		floattobf(bffxadj, fxadj);
	}

	// calc co-ords of topleft & botright corners of box
	tmpx = g_z_width/-2 + fxadj; // from zoombox center as origin, on g_x_dots scale
	tmpy = g_z_depth*g_viewWindow.AspectRatio()/2;
	dx = (rotcos*tmpx - rotsin*tmpy) - tmpx; // delta x to rotate topleft
	dy = tmpy - (rotsin*tmpx + rotcos*tmpy); // delta y to rotate topleft

	// calc co-ords of topleft
	ftemp1 = g_zbx + dx + fxadj;
	ftemp2 = g_zby + dy/g_viewWindow.AspectRatio();

	tl.x   = int(ftemp1*(g_dx_size + PIXELROUND)); // screen co-ords
	tl.y   = int(ftemp2*(g_dy_size + PIXELROUND));
	g_escape_time_state.m_grid_fp.x_min()  = g_sx_min + ftemp1*fxwidth + ftemp2*fxskew; // real co-ords
	g_escape_time_state.m_grid_fp.y_max()  = g_sy_max + ftemp2*fydepth + ftemp1*fyskew;
	if (g_bf_math)
	{
		calculate_corner(g_escape_time_state.m_grid_bf.x_min(), g_sx_min_bf, ftemp1, bffxwidth, ftemp2, bffxskew);
		calculate_corner(g_escape_time_state.m_grid_bf.y_max(), g_sy_max_bf, ftemp2, bffydepth, ftemp1, bffyskew);
	}

	// calc co-ords of bottom right
	ftemp1 = g_zbx + g_z_width - dx - fxadj;
	ftemp2 = g_zby - dy/g_viewWindow.AspectRatio() + g_z_depth;
	br.x   = int(ftemp1*(g_dx_size + PIXELROUND));
	br.y   = int(ftemp2*(g_dy_size + PIXELROUND));
	g_escape_time_state.m_grid_fp.x_max()  = g_sx_min + ftemp1*fxwidth + ftemp2*fxskew;
	g_escape_time_state.m_grid_fp.y_min()  = g_sy_max + ftemp2*fydepth + ftemp1*fyskew;
	if (g_bf_math)
	{
		calculate_corner(g_escape_time_state.m_grid_bf.x_max(), g_sx_min_bf, ftemp1, bffxwidth, ftemp2, bffxskew);
		calculate_corner(g_escape_time_state.m_grid_bf.y_min(), g_sy_max_bf, ftemp2, bffydepth, ftemp1, bffyskew);
	}
	// do the same for botleft & topright
	tmpx = g_z_width/-2 - fxadj;
	tmpy = -tmpy;
	dx = (rotcos*tmpx - rotsin*tmpy) - tmpx;
	dy = tmpy - (rotsin*tmpx + rotcos*tmpy);
	ftemp1 = g_zbx + dx - fxadj;
	ftemp2 = g_zby + dy/g_viewWindow.AspectRatio() + g_z_depth;
	bl.x   = int(ftemp1*(g_dx_size + PIXELROUND));
	bl.y   = int(ftemp2*(g_dy_size + PIXELROUND));
	g_escape_time_state.m_grid_fp.x_3rd()  = g_sx_min + ftemp1*fxwidth + ftemp2*fxskew;
	g_escape_time_state.m_grid_fp.y_3rd()  = g_sy_max + ftemp2*fydepth + ftemp1*fyskew;
	if (g_bf_math)
	{
		calculate_corner(g_escape_time_state.m_grid_bf.x_3rd(), g_sx_min_bf, ftemp1, bffxwidth, ftemp2, bffxskew);
		calculate_corner(g_escape_time_state.m_grid_bf.y_3rd(), g_sy_max_bf, ftemp2, bffydepth, ftemp1, bffyskew);
	}
	ftemp1 = g_zbx + g_z_width - dx + fxadj;
	ftemp2 = g_zby - dy/g_viewWindow.AspectRatio();
	tr.x   = int(ftemp1*(g_dx_size + PIXELROUND));
	tr.y   = int(ftemp2*(g_dy_size + PIXELROUND));

	if (g_zoomBox.count() != 0)  // remove the old box from display
	{
		g_zoomBox.clear();
		g_zoomBox.set_count(0);
	}

	if (drawit)  // caller wants box drawn as well as co-ords calc'd
	{
#ifndef XFRACT
		// build the list of zoom box pixels
		add_box(tl); add_box(tr);               // corner pixels
		add_box(bl); add_box(br);
		draw_lines(tl, tr, bl.x-tl.x, bl.y-tl.y); // top & bottom lines
		draw_lines(tl, bl, tr.x-tl.x, tr.y-tl.y); // left & right lines
#else
		g_box_x[0] = tl.x + g_screen_x_offset;
		g_box_y[0] = tl.y + g_screen_y_offset;
		g_box_x[1] = tr.x + g_screen_x_offset;
		g_box_y[1] = tr.y + g_screen_y_offset;
		g_box_x[2] = br.x + g_screen_x_offset;
		g_box_y[2] = br.y + g_screen_y_offset;
		g_box_x[3] = bl.x + g_screen_x_offset;
		g_box_y[3] = bl.y + g_screen_y_offset;
		g_zoomBox.set_count(1);
#endif
		g_zoomBox.display();
	}
}

void draw_lines(Coordinate fr, Coordinate to,
						int dx, int dy)
{
	int xincr;
	int yincr;
	int ctr;
	int altctr;
	int altdec;
	int altinc;
	Coordinate tmpp;
	Coordinate line1;
	Coordinate line2;

	if (abs(to.x-fr.x) > abs(to.y-fr.y))  // delta.x > delta.y
	{
		if (fr.x > to.x)  // swap so from.x is < to.x
		{
			tmpp = fr;
			fr = to;
			to = tmpp;
		}
		xincr = (to.x-fr.x)*4/g_screen_width + 1; // do every 1st, 2nd, 3rd, or 4th dot
		ctr = (to.x-fr.x-1)/xincr;
		altdec = abs(to.y-fr.y)*xincr;
		altinc = to.x-fr.x;
		altctr = altinc/2;
		yincr = (to.y > fr.y)?1:-1;
		line1.x = fr.x;
		line1.y = fr.y;
		line2.x = line1.x + dx;
		line2.y = line1.y + dy;
		while (--ctr >= 0)
		{
			line1.x += xincr;
			line2.x += xincr;
			altctr -= altdec;
			while (altctr < 0)
			{
				altctr  += altinc;
				line1.y += yincr;
				line2.y += yincr;
			}
			add_box(line1);
			add_box(line2);
		}
	}

	else  // delta.y > delta.x
	{
		if (fr.y > to.y)  // swap so from.y is < to.y
		{
			tmpp = fr;
			fr = to;
			to = tmpp;
		}
		yincr = (to.y-fr.y)*4/g_screen_height + 1; // do every 1st, 2nd, 3rd, or 4th dot
		ctr = (to.y-fr.y-1)/yincr;
		altdec = abs(to.x-fr.x)*yincr;
		altinc = to.y-fr.y;
		altctr = altinc/2;
		xincr = (to.x > fr.x) ? 1 : -1;
		line1.x = fr.x;
		line1.y = fr.y;
		line2.x = line1.x + dx;
		line2.y = line1.y + dy;
		while (--ctr >= 0)
		{
			line1.y += yincr;
			line2.y += yincr;
			altctr  -= altdec;
			while (altctr < 0)
			{
				altctr  += altinc;
				line1.x += xincr;
				line2.x += xincr;
			}
			add_box(line1);
			add_box(line2);
		}
	}
}

void add_box(Coordinate point)
{
	assert(g_zoomBox.count() < NUM_BOXES);
	point.x += g_screen_x_offset;
	point.y += g_screen_y_offset;
	if (point.x >= 0 && point.x < g_screen_width &&
		point.y >= 0 && point.y < g_screen_height)
	{
		g_zoomBox.push(point);
	}
}

static void zoom_box_move_coordinate(double delta, int alignment, double &coordinate, double half_size, double size)
{
	if (delta != 0.0)
	{
		coordinate += delta;
		if (coordinate + half_size < 0)  // center must stay onscreen
		{
			coordinate = -half_size;
		}
		if (coordinate + half_size > 1)
		{
			coordinate = 1.0 - half_size;
		}
		int pos;
		if (alignment != 0
			&& ((pos = int(coordinate*(size + PIXELROUND))) & (alignment-1)) != 0)
		{
			if (delta > 0)
			{
				pos += alignment;
			}
			pos -= pos & (alignment-1); // adjust col to pass alignment
			coordinate = pos/size;
		}
	}
}

void zoom_box_move(double dx, double dy)
{
	int align = check_pan();
	zoom_box_move_coordinate(dx, align, g_zbx, g_z_width/2.0, g_dx_size);
	zoom_box_move_coordinate(dy, align, g_zby, g_z_depth/2.0, g_dy_size);
}

static void change_box_adjust(double &delta_length, double &length)
{
	if (length + delta_length > 1.0)
	{
		delta_length = 1.0 - length;
	}
	if (length + delta_length < 0.05)
	{
		delta_length = 0.05 - length;
	}
	length += delta_length;
}

static void change_box(double dwidth, double ddepth)
{
	change_box_adjust(dwidth, g_z_width);
	change_box_adjust(ddepth, g_z_depth);
	zoom_box_move(dwidth/-2, ddepth/-2); // keep it centered & check limits
}

void zoom_box_resize(int steps)
{
	double deltax;
	double deltay;
	if (g_z_depth*g_screen_aspect_ratio > g_z_width)  // box larger on y axis
	{
		deltay = steps*0.036/g_screen_aspect_ratio;
		deltax = g_z_width*deltay/g_z_depth;
	}
	else  // box larger on x axis
	{
		deltax = steps*0.036;
		deltay = g_z_depth*deltax/g_z_width;
	}
	change_box(deltax, deltay);
}

void zoom_box_change_i(int dw, int dd)
{
	// change size by pixels
	change_box(double(dw)/g_dx_size, double(dd)/g_dy_size);
}

static void zmo_calcbf(bf_t bfdx, bf_t bfdy,
	bf_t bfnewx, bf_t bfnewy, bf_t bfplotmx1, bf_t bfplotmx2, bf_t bfplotmy1,
	bf_t bfplotmy2, bf_t bfftemp)
{
	BigStackSaver savedStack;

	bf_t btmp1(g_rbf_length);
	bf_t btmp2(g_rbf_length);
	bf_t btmp3(g_rbf_length);
	bf_t btmp4(g_rbf_length);
	bf_t btmp2a(g_rbf_length);
	bf_t btmp4a(g_rbf_length);
	bf_t btempx(g_rbf_length);
	bf_t btempy(g_rbf_length);

	/* calc cur screen corner relative to zoombox, when zoombox co-ords
		are taken as (0, 0) topleft thru (1, 1) bottom right */

	// tempx = dy*g_plot_mx1 - dx*g_plot_mx2;
	multiply_bf(btmp1, bfdy, bfplotmx1);
	multiply_bf(btmp2, bfdx, bfplotmx2);
	subtract_bf(btempx, btmp1, btmp2);

	// tempy = dx*g_plot_my1 - dy*g_plot_my2;
	multiply_bf(btmp1, bfdx, bfplotmy1);
	multiply_bf(btmp2, bfdy, bfplotmy2);
	subtract_bf(btempy, btmp1, btmp2);

	// calc new corner by extending from current screen corners
	// *newx = g_sx_min + tempx*(g_sx_max-g_sx_3rd)/ftemp + tempy*(g_sx_3rd-g_sx_min)/ftemp;
	subtract_bf(btmp1, g_sx_max_bf, g_sx_3rd_bf);
	multiply_bf(btmp2, btempx, btmp1);
	div_bf(btmp2a, btmp2, bfftemp);
	subtract_bf(btmp3, g_sx_3rd_bf, g_sx_min_bf);
	multiply_bf(btmp4, btempy, btmp3);
	div_bf(btmp4a, btmp4, bfftemp);
	add_bf(bfnewx, g_sx_min_bf, btmp2a);
	add_a_bf(bfnewx, btmp4a);

	// *newy = g_sy_max + tempy*(g_sy_3rd-g_sy_max)/ftemp + tempx*(g_sy_min-g_sy_3rd)/ftemp;
	subtract_bf(btmp1, g_sy_3rd_bf, g_sy_max_bf);
	multiply_bf(btmp2, btempy, btmp1);
	div_bf(btmp2a, btmp2, bfftemp);
	subtract_bf(btmp3, g_sy_min_bf, g_sy_3rd_bf);
	multiply_bf(btmp4, btempx, btmp3);
	div_bf(btmp4a, btmp4, bfftemp);
	add_bf(bfnewy, g_sy_max_bf, btmp2a);
	add_a_bf(bfnewy, btmp4a);
}

static void zmo_calc(double dx, double dy, double *newx, double *newy, double ftemp)
{
	double tempx;
	double tempy;
	/* calc cur screen corner relative to zoombox, when zoombox co-ords
		are taken as (0, 0) topleft thru (1, 1) bottom right */
	tempx = dy*g_plot_mx1 - dx*g_plot_mx2;
	tempy = dx*g_plot_my1 - dy*g_plot_my2;

	// calc new corner by extending from current screen corners
	*newx = g_sx_min + tempx*(g_sx_max-g_sx_3rd)/ftemp + tempy*(g_sx_3rd-g_sx_min)/ftemp;
	*newy = g_sy_max + tempy*(g_sy_3rd-g_sy_max)/ftemp + tempx*(g_sy_min-g_sy_3rd)/ftemp;
}

static void zoom_out_bf() // for ctl-enter, calc corners for zooming out
{
	/* (xmin, ymax), etc, are already set to zoombox corners;
	(g_sx_min, g_sy_max), etc, are still the screen's corners;
	use the same logic as plot_orbit stuff to first calculate current screen
	corners relative to the zoombox, as if the zoombox were a square with
	upper left (0, 0) and width/depth 1; ie calc the current screen corners
	as if plotting them from the zoombox;
	then extend these co-ords from current real screen corners to get
	new actual corners
	*/
	BigStackSaver savedStack;
	bf_t savbfxmin(g_rbf_length);
	bf_t savbfymax(g_rbf_length);
	bf_t bfftemp(g_rbf_length);
	bf_t tmp1(g_rbf_length);
	bf_t tmp2(g_rbf_length);
	bf_t tmp3(g_rbf_length);
	bf_t tmp4(g_rbf_length);
	bf_t tmp5(g_rbf_length);
	bf_t tmp6(g_rbf_length);
	bf_t bfplotmx1(g_rbf_length);
	bf_t bfplotmx2(g_rbf_length);
	bf_t bfplotmy1(g_rbf_length);
	bf_t bfplotmy2(g_rbf_length);
	// ftemp = (ymin-y3rd)*(x3rd-xmin) - (xmax-x3rd)*(y3rd-ymax);
	subtract_bf(tmp1, g_escape_time_state.m_grid_bf.y_min(), g_escape_time_state.m_grid_bf.y_3rd());
	subtract_bf(tmp2, g_escape_time_state.m_grid_bf.x_3rd(), g_escape_time_state.m_grid_bf.x_min());
	subtract_bf(tmp3, g_escape_time_state.m_grid_bf.x_max(), g_escape_time_state.m_grid_bf.x_3rd());
	subtract_bf(tmp4, g_escape_time_state.m_grid_bf.y_3rd(), g_escape_time_state.m_grid_bf.y_max());
	multiply_bf(tmp5, tmp1, tmp2);
	multiply_bf(tmp6, tmp3, tmp4);
	subtract_bf(bfftemp, tmp5, tmp6);
	// g_plot_mx1 = (x3rd-xmin); */; /* reuse the plotxxx vars is safe
	copy_bf(bfplotmx1, tmp2);
	// g_plot_mx2 = (y3rd-ymax);
	copy_bf(bfplotmx2, tmp4);
	// g_plot_my1 = (ymin-y3rd);
	copy_bf(bfplotmy1, tmp1);
	// g_plot_my2 = (xmax-x3rd); ;
	copy_bf(bfplotmy2, tmp3);

	// savxxmin = xmin; savyymax = ymax;
	copy_bf(savbfxmin, g_escape_time_state.m_grid_bf.x_min());
	copy_bf(savbfymax, g_escape_time_state.m_grid_bf.y_max());

	subtract_bf(tmp1, g_sx_min_bf, savbfxmin);
	subtract_bf(tmp2, g_sy_max_bf, savbfymax);
	zmo_calcbf(tmp1, tmp2, g_escape_time_state.m_grid_bf.x_min(), g_escape_time_state.m_grid_bf.y_max(), bfplotmx1, bfplotmx2, bfplotmy1,
					bfplotmy2, bfftemp);
	subtract_bf(tmp1, g_sx_max_bf, savbfxmin);
	subtract_bf(tmp2, g_sy_min_bf, savbfymax);
	zmo_calcbf(tmp1, tmp2, g_escape_time_state.m_grid_bf.x_max(), g_escape_time_state.m_grid_bf.y_min(), bfplotmx1, bfplotmx2, bfplotmy1,
					bfplotmy2, bfftemp);
	subtract_bf(tmp1, g_sx_3rd_bf, savbfxmin);
	subtract_bf(tmp2, g_sy_3rd_bf, savbfymax);
	zmo_calcbf(tmp1, tmp2, g_escape_time_state.m_grid_bf.x_3rd(), g_escape_time_state.m_grid_bf.y_3rd(), bfplotmx1, bfplotmx2, bfplotmy1,
					bfplotmy2, bfftemp);
}

static void zoom_out_double() // for ctl-enter, calc corners for zooming out
{
	/* (xmin, ymax), etc, are already set to zoombox corners;
		(g_sx_min, g_sy_max), etc, are still the screen's corners;
		use the same logic as plot_orbit stuff to first calculate current screen
		corners relative to the zoombox, as if the zoombox were a square with
		upper left (0, 0) and width/depth 1; ie calc the current screen corners
		as if plotting them from the zoombox;
		then extend these co-ords from current real screen corners to get
		new actual corners
		*/
	double savxxmin;
	double savyymax;
	double ftemp;
	ftemp = (g_escape_time_state.m_grid_fp.y_min()-g_escape_time_state.m_grid_fp.y_3rd())*(g_escape_time_state.m_grid_fp.x_3rd()-g_escape_time_state.m_grid_fp.x_min()) - (g_escape_time_state.m_grid_fp.x_max()-g_escape_time_state.m_grid_fp.x_3rd())*(g_escape_time_state.m_grid_fp.y_3rd()-g_escape_time_state.m_grid_fp.y_max());
	g_plot_mx1 = (g_escape_time_state.m_grid_fp.x_3rd()-g_escape_time_state.m_grid_fp.x_min()); // reuse the plotxxx vars is safe
	g_plot_mx2 = (g_escape_time_state.m_grid_fp.y_3rd()-g_escape_time_state.m_grid_fp.y_max());
	g_plot_my1 = (g_escape_time_state.m_grid_fp.y_min()-g_escape_time_state.m_grid_fp.y_3rd());
	g_plot_my2 = (g_escape_time_state.m_grid_fp.x_max()-g_escape_time_state.m_grid_fp.x_3rd());
	savxxmin = g_escape_time_state.m_grid_fp.x_min();
	savyymax = g_escape_time_state.m_grid_fp.y_max();
	zmo_calc(g_sx_min-savxxmin, g_sy_max-savyymax, &g_escape_time_state.m_grid_fp.x_min(), &g_escape_time_state.m_grid_fp.y_max(), ftemp);
	zmo_calc(g_sx_max-savxxmin, g_sy_min-savyymax, &g_escape_time_state.m_grid_fp.x_max(), &g_escape_time_state.m_grid_fp.y_min(), ftemp);
	zmo_calc(g_sx_3rd-savxxmin, g_sy_3rd-savyymax, &g_escape_time_state.m_grid_fp.x_3rd(), &g_escape_time_state.m_grid_fp.y_3rd(), ftemp);
}

void zoom_box_out() // for ctl-enter, calc corners for zooming out
{
	if (g_bf_math)
	{
		zoom_out_bf();
	}
	else
	{
		zoom_out_double();
	}
}

void aspect_ratio_crop(float oldaspect, float newaspect)
{
	double xmargin;
	double ymargin;
	if (newaspect > oldaspect)  // new ratio is taller, crop x
	{
		double ftemp = (1.0 - oldaspect/newaspect)/2;
		xmargin = (g_escape_time_state.m_grid_fp.x_max() - g_escape_time_state.m_grid_fp.x_3rd())*ftemp;
		ymargin = (g_escape_time_state.m_grid_fp.y_min() - g_escape_time_state.m_grid_fp.y_3rd())*ftemp;
		g_escape_time_state.m_grid_fp.x_3rd() += xmargin;
		g_escape_time_state.m_grid_fp.y_3rd() += ymargin;
	}
	else                        // new ratio is wider, crop y
	{
		double ftemp = (1.0 - newaspect/oldaspect)/2;
		xmargin = (g_escape_time_state.m_grid_fp.x_3rd() - g_escape_time_state.m_grid_fp.x_min())*ftemp;
		ymargin = (g_escape_time_state.m_grid_fp.y_3rd() - g_escape_time_state.m_grid_fp.y_max())*ftemp;
		g_escape_time_state.m_grid_fp.x_3rd() -= xmargin;
		g_escape_time_state.m_grid_fp.y_3rd() -= ymargin;
	}
	g_escape_time_state.m_grid_fp.x_min() += xmargin;
	g_escape_time_state.m_grid_fp.y_max() += ymargin;
	g_escape_time_state.m_grid_fp.x_max() -= xmargin;
	g_escape_time_state.m_grid_fp.y_min() -= ymargin;
}

static int check_pan() // return 0 if can't, alignment requirement if can
{
	int i;
	int j;
	if ((g_externs.CalculationStatus() != CALCSTAT_RESUMABLE && g_externs.CalculationStatus() != CALCSTAT_COMPLETED) || g_evolving_flags)
	{
		return 0; // not resumable, not complete
	}
	if (g_current_fractal_specific->calculate_type != standard_fractal
		&& g_current_fractal_specific->calculate_type != calculate_mandelbrot_l
		&& g_current_fractal_specific->calculate_type != calculate_mandelbrot_fp
		&& g_current_fractal_specific->calculate_type != lyapunov
		&& g_current_fractal_specific->calculate_type != froth_calc)
	{
		return 0; // not a g_work_list-driven type
	}
	if (g_z_width != 1.0 || g_z_depth != 1.0 || g_z_skew != 0.0 || g_z_rotate != 0.0)
	{
		return 0; // not a full size unrotated unskewed zoombox
	}
	if (g_externs.StandardCalculationMode() == CALCMODE_TESSERAL)
	{
		return 0; // tesselate, can't do it
	}
	if (g_externs.StandardCalculationMode() == CALCMODE_DIFFUSION)
	{
		return 0; // diffusion scan: can't do it either
	}
	if (g_externs.StandardCalculationMode() == CALCMODE_ORBITS)
	{
		return 0; // orbits, can't do it
	}

	// can pan if we get this far

	if (g_externs.CalculationStatus() == CALCSTAT_COMPLETED)
	{
		return 1; // image completed, align on any pixel
	}
	if (g_potential_flag && g_potential_16bit)
	{
		return 1; // 1 pass forced so align on any pixel
	}
	if (g_externs.StandardCalculationMode() == CALCMODE_BOUNDARY_TRACE)
	{
		return 1; // btm, align on any pixel
	}
	if (g_externs.StandardCalculationMode() != CALCMODE_SOLID_GUESS
		|| (g_current_fractal_specific->no_solid_guessing()))
	{
		if (g_externs.StandardCalculationMode() == CALCMODE_DUAL_PASS
			|| g_externs.StandardCalculationMode() == CALCMODE_TRIPLE_PASS) // align on even pixel for 2pass
		{
			return 2;
		}
		return 1; // assume 1pass
	}
	// solid guessing
	start_resume();
	g_WorkList.get_resume();
	// don't do end_resume! we're just looking
	i = std::min(9, g_WorkList.get_lowest_pass());
	j = solid_guess_block_size(); // worst-case alignment requirement
	while (--i >= 0)
	{
		j >>= 1; // reduce requirement
	}
	return j;
}

// move a row on the screen
static void move_row(int fromrow, int torow, int col)
{
	int startcol;
	int endcol;
	int tocol;
	std::vector<BYTE> scanline;
	scanline.resize(g_x_dots, 0);
	if (fromrow >= 0 && fromrow < g_y_dots)
	{
		tocol = 0;
		startcol = 0;
		endcol = g_x_dots - 1;
		if (col < 0)
		{
			tocol -= col;
			endcol += col;
		}
		if (col > 0)
		{
			startcol += col;
		}
		get_line(fromrow, startcol, endcol, &scanline[tocol]);
	}
	put_line(torow, 0, g_x_dots-1, &scanline[0]);
}

void init_pan_or_recalc(bool do_zoomout) // decide to recalc, or to chg g_work_list & pan
{
	int i;
	int j;
	int row;
	int col;
	int y;
	int alignmask;
	int listfull;
	if (g_z_width == 0.0)
	{
		return; // no zoombox, leave calculation status as is
	}
	// got a zoombox
	alignmask = check_pan()-1;
	if (alignmask < 0 || g_evolving_flags)
	{
		g_externs.SetCalculationStatus(CALCSTAT_PARAMS_CHANGED); // can't pan, trigger recalc
		return;
	}
	if (g_zbx == 0.0 && g_zby == 0.0)
	{
		g_zoomBox.clear();
		return; // box is full screen, leave calculation status as is
	}
	col = int(g_zbx*(g_dx_size + PIXELROUND)); // calc dest col, row of topleft pixel
	row = int(g_zby*(g_dy_size + PIXELROUND));
	if (do_zoomout)  // invert row and col
	{
		row = -row;
		col = -col;
	}
	if ((row&alignmask) != 0 || (col&alignmask) != 0)
	{
		g_externs.SetCalculationStatus(CALCSTAT_PARAMS_CHANGED); // not on useable pixel alignment, trigger recalc
		return;
	}
	// pan

	g_WorkList.reset_items();
	if (g_externs.CalculationStatus() == CALCSTAT_RESUMABLE)
	{
		start_resume();
		g_WorkList.get_resume();
	} // don't do end_resume! we might still change our mind
	g_WorkList.offset_items(row, col);
	// add g_work_list entries for the new edges
	listfull = 0;
	i = 0;
	j = g_y_dots-1;
	if (row < 0)
	{
		listfull |= g_WorkList.add(0, g_x_dots-1, 0,
			0, -row-1, 0,
			0, 0);
		i = -row;
	}
	if (row > 0)
	{
		listfull |= g_WorkList.add(0, g_x_dots-1, 0,
			g_y_dots-row, g_y_dots-1, g_y_dots-row,
			0, 0);
		j = g_y_dots - row - 1;
	}
	if (col < 0)
	{
		listfull |= g_WorkList.add(0, -col-1, 0,
			i, j, i,
			0, 0);
	}
	if (col > 0)
	{
		listfull |= g_WorkList.add(g_x_dots-col, g_x_dots-1, g_x_dots-col,
			i, j, i,
			0, 0);
	}
	if (listfull != 0)
	{
		if (stop_message(STOPMSG_CANCEL,
				"Tables full, can't pan current image.\n"
				"Cancel resumes old image, continue pans and calculates a new one."))
		{
			g_z_width = 0; // cancel the zoombox
			zoom_box_draw(true);
		}
		else
		{
			g_externs.SetCalculationStatus(CALCSTAT_PARAMS_CHANGED); // trigger recalc
		}
		return;
	}
	// now we're committed
	g_externs.SetCalculationStatus(CALCSTAT_RESUMABLE);
	g_zoomBox.clear();
	if (row > 0) // move image up
	{
		for (y = 0; y < g_y_dots; ++y)
		{
			move_row(y + row, y, col);
		}
	}
	else         // move image down
	{
		for (y = g_y_dots; --y >= 0; )
		{
			move_row(y + row, y, col);
		}
	}
	fix_work_list(); // fixup any out of bounds g_work_list entries
	g_WorkList.put_resume();
}

static void fix_work_list() // fix out of bounds and symmetry related stuff
{
	g_WorkList.fix();
}