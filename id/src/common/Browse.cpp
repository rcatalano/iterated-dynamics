#include <cerrno>
#include <ctime>
#include <string>

#include <boost/filesystem/path.hpp>

#include "port.h"
#include "prototyp.h"
#include "fractype.h"
#include "helpdefs.h"

#include "biginit.h"
#include "Browse.h"
#include "cmdfiles.h"
#include "drivers.h"
#include "encoder.h"
#include "EscapeTime.h"
#include "Externals.h"
#include "filesystem.h"
#include "Formula.h"
#include "framain2.h"
#include "fracsubr.h"
#include "idhelp.h"
#include "loadfile.h"
#include "lorenz.h"
#include "MathUtil.h"
#include "miscres.h"
#include "prompts2.h"
#include "realdos.h"
#include "UIChoices.h"
#include "zoom.h"
#include "ZoomBox.h"

#ifdef XFRACT
#define difftime(now,then) ((now)-(then))
#endif

enum BrowseConstants
{
	MAX_WINDOWS_OPEN = 450
};

class BrowseStateImpl : public BrowseState
{
public:
	BrowseStateImpl()
		: _mask(),
		_name(),
		_browsing(false),
		_checkParameters(false),
		_checkType(false),
		_autoBrowse(false),
		_subImages(false),
		_crossHairBoxSize(3),
		_doubleCaution(false),
		_tooSmall(0.0f)
	{
	}
	virtual ~BrowseStateImpl()
	{
	}

	bool AutoBrowse() const				{ return _autoBrowse; }
	bool Browsing() const				{ return _browsing; }
	bool CheckParameters() const		{ return _checkParameters; }
	bool CheckType() const				{ return _checkType; }
	const std::string &Mask() const		{ return _mask; }
	const std::string &Name() const		{ return _name; }
	bool SubImages() const				{ return _subImages; }
	int CrossHairBoxSize() const		{ return _crossHairBoxSize; }
	bool DoubleCaution() const			{ return _doubleCaution; }
	float TooSmall() const				{ return _tooSmall; }

	void SetAutoBrowse(bool value)		{ _autoBrowse = value; }
	void SetBrowsing(bool value)		{ _browsing = value; }
	void SetCheckParameters(bool value) { _checkParameters = value; }
	void SetCheckType(bool value)		{ _checkType = value; }
	void SetName(const std::string &value) { _name = value; }
	void SetSubImages(bool value)		{ _subImages = value; }
	void SetCrossHairBoxSize(int value) { _crossHairBoxSize = value; }
	void SetDoubleCaution(bool value)	{ _doubleCaution = value; }
	void SetTooSmall(float value)		{ _tooSmall = value; }

	void ExtractReadName();
	int GetParameters();
	void MakePath(const char *fname, const char *ext);
	void MergePathNames(std::string &read_name);
	void Restart();

private:
	std::string _mask;
	std::string _name;
	bool _browsing;
	bool _checkParameters;
	bool _checkType;
	bool _autoBrowse;
	bool _subImages;
	int _crossHairBoxSize;
	bool _doubleCaution;				// confirm for deleting
	float _tooSmall;
};

static BrowseStateImpl s_browse_state;
BrowseState &g_browse_state(s_browse_state);

struct CoordinateWindow  // for look_get_window on screen browser
{
	Coordinate itl; // screen coordinates
	Coordinate ibl;
	Coordinate itr;
	Coordinate ibr;
	double win_size;   // box size for draw_window()
	std::string name;     // for filename
	int box_count;      // bytes of saved screen info
};

static int oldbf_math;
// here because must be visible inside several routines
static bf_t bt_a;
static bf_t bt_b;
static bf_t bt_c;
static bf_t bt_d;
static bf_t bt_e;
static bf_t bt_f;
static bf_t n_a;
static bf_t n_b;
static bf_t n_c;
static bf_t n_d;
static bf_t n_e;
static bf_t n_f;
static affine *cvt;
static CoordinateWindow browse_windows[MAX_WINDOWS_OPEN];

// prototypes
static void check_history(const char *, const char *);
static void transform(CoordinateD *);
static void transform_bf(bf_t, bf_t, CoordinateD *);
static void draw_window(int color, CoordinateWindow *info);
static bool is_visible_window(CoordinateWindow *, fractal_info *, multiple_precision_info_extension_block *);
static void bfsetup_convert_to_screen();
static bool fractal_types_match(const fractal_info &info, const formula_info_extension_block &formula_info);
static bool functions_match(const fractal_info &info, int num_functions);
static bool parameters_match(const fractal_info &info);

void BrowseStateImpl::ExtractReadName()
{
	::extract_filename(_name, g_read_name);
}

void BrowseStateImpl::MakePath(const char *fname, const char *ext)
{
	_name = ::make_path("", "", fname, ext).string();
}

void BrowseStateImpl::MergePathNames(std::string &read_name)
{
	::merge_path_names(false, read_name, _name);
}

void BrowseStateImpl::Restart()
{
	_tooSmall = 6.0f;
	_autoBrowse = false;
	_checkParameters = true;
	_checkType = true;
	_doubleCaution = true;
	_crossHairBoxSize = 3;
	_mask = "*.gif";
}

// get browse parameters
// returns 3 if anything changes.  code pinched from get_view_params

int BrowseStateImpl::GetParameters()
{
	bool old_auto_browse = _autoBrowse;;
	bool old_browse_check_type = _checkType;
	bool old_browse_check_parameters = _checkParameters;
	bool old_double_caution = _doubleCaution;
	int old_cross_hair_box_size = _crossHairBoxSize;
	double old_too_small = _tooSmall;
	std::string old_browse_mask = _mask;

restart:
	{
		UIChoices dialog(IDHELP_BROWSER_PARAMETERS, "Browse ('L'ook) Mode Options", 16);

		dialog.push("Autobrowsing? (y/n)", _autoBrowse);
		dialog.push("Ask about GIF video mode? (y/n)", g_ui_state.ask_video);
		dialog.push("Check fractal type? (y/n)", _checkType);
		dialog.push("Check fractal parameters (y/n)", _checkParameters);
		dialog.push("Confirm file deletes (y/n)", _doubleCaution);
		dialog.push("Smallest window to display (size in pixels)", _tooSmall);
		dialog.push("Smallest box size shown before crosshairs used (pix)", _crossHairBoxSize);
		dialog.push("Browse search filename mask ", _mask);
		dialog.push("");
		dialog.push("Press "FK_F4" to reset browse parameters to defaults.");

		int result = dialog.prompt();
		if (result < 0)
		{
			return 0;
		}

		if (result == IDK_F4)
		{
			g_ui_state.ask_video = true;
			Restart();
			goto restart;
		}

		int k = -1;
		_autoBrowse = (dialog.values(++k).uval.ch.val != 0);
		g_ui_state.ask_video = (dialog.values(++k).uval.ch.val != 0);
		_checkType = (dialog.values(++k).uval.ch.val != 0);
		_checkParameters = (dialog.values(++k).uval.ch.val != 0);
		_doubleCaution = (dialog.values(++k).uval.ch.val != 0);
		_tooSmall = std::max(0.0f, float(dialog.values(++k).uval.dval));
		_crossHairBoxSize = MathUtil::Clamp(dialog.values(++k).uval.ival, 1, 10);
		_mask = dialog.values(++k).uval.sval;

		int i = 0;
		if (_autoBrowse != old_auto_browse
			|| _checkType != old_browse_check_type
			|| _checkParameters != old_browse_check_parameters
			|| _doubleCaution != old_double_caution
			|| _tooSmall != old_too_small
			|| _crossHairBoxSize != old_cross_hair_box_size
			|| _mask != old_browse_mask)
		{
			i = -3;
		}
		if (g_evolving_flags)  // can't browse
		{
			_autoBrowse = false;
			i = 0;
		}

		return i;
	}
}

// maps points onto view screen
static void transform(CoordinateD *point)
{
	double tmp_pt_x = cvt->a*point->x + cvt->b*point->y + cvt->e;
	point->y = cvt->c*point->x + cvt->d*point->y + cvt->f;
	point->x = tmp_pt_x;
}

// maps points onto view screen
static void transform_bf(bf_t bt_x, bf_t bt_y, CoordinateD *point)
{
	BigStackSaver savedStack;
	bf_t bt_tmp1(g_rbf_length);
	bf_t bt_tmp2(g_rbf_length);

	// point->x = cvt->a*point->x + cvt->b*point->y + cvt->e;
	multiply_bf(bt_tmp1, n_a, bt_x);
	multiply_bf(bt_tmp2, n_b, bt_y);
	add_a_bf(bt_tmp1, bt_tmp2);
	add_a_bf(bt_tmp1, n_e);
	point->x = double(bftofloat(bt_tmp1));

	// point->y = cvt->c*point->x + cvt->d*point->y + cvt->f;
	multiply_bf(bt_tmp1, n_c, bt_x);
	multiply_bf(bt_tmp2, n_d, bt_y);
	add_a_bf(bt_tmp1, bt_tmp2);
	add_a_bf(bt_tmp1, n_f);
	point->y = double(bftofloat(bt_tmp1));
}

static void is_visible_window_corner(const fractal_info &info,
	bf_t bt_x, bf_t bt_y,
	bf_t bt_xmin, bf_t bt_xmax,
	bf_t bt_ymin, bf_t bt_ymax,
	bf_t bt_x3rd, bf_t bt_y3rd,
	CoordinateD &corner)
{
	if (oldbf_math || info.bf_math)
	{
		if (!info.bf_math)
		{
			floattobf(bt_x, (info.x_max)-(info.x_3rd-info.x_min));
			floattobf(bt_y, (info.y_max) + (info.y_min-info.y_3rd));
		}
		else
		{
			neg_a_bf(subtract_bf(bt_x, bt_x3rd, bt_xmin));
			add_a_bf(bt_x, bt_xmax);
			subtract_bf(bt_y, bt_ymin, bt_y3rd);
			add_a_bf(bt_y, bt_ymax);
		}
		transform_bf(bt_x, bt_y, &corner);
	}
	else
	{
		corner.x = (info.x_max)-(info.x_3rd-info.x_min);
		corner.y = (info.y_max) + (info.y_min-info.y_3rd);
		transform(&corner);
	}
}

inline bool is_visible(CoordinateD const &pt)
{
	return pt.x >= -g_screen_x_offset
		&& pt.x <= (g_screen_width-g_screen_x_offset)
		&& pt.y >= (0-g_screen_y_offset)
		&& pt.y <= (g_screen_height-g_screen_y_offset);
}

static bool is_visible_window(CoordinateWindow *list, fractal_info *info,
	multiple_precision_info_extension_block *mp_info)
{
	double toobig = std::sqrt(sqr(double(g_screen_width)) + sqr(double(g_screen_height)))*1.5;
	BigStackSaver savedStack;

	// Save original values.
	int orig_bflength = g_bf_length;
	int orig_bnlength = g_bn_length;
	int orig_padding = g_padding;
	int orig_rlength = g_r_length;
	int orig_shiftfactor = g_shift_factor;
	int orig_rbflength = g_rbf_length;

	int two_len = g_bf_length + 2;
	bf_t bt_x(g_bf_length);
	bf_t bt_y(g_bf_length);
	bf_t bt_xmin(g_bf_length);
	bf_t bt_xmax(g_bf_length);
	bf_t bt_ymin(g_bf_length);
	bf_t bt_ymax(g_bf_length);
	bf_t bt_x3rd(g_bf_length);
	bf_t bt_y3rd(g_bf_length);

	if (info->bf_math)
	{
		int di_bflength = info->bflength + g_step_bn;
		int two_di_len = di_bflength + 2;
		int two_rbf = g_rbf_length + 2;

		n_a = bf_t(g_rbf_length);
		n_b = bf_t(g_rbf_length);
		n_c = bf_t(g_rbf_length);
		n_d = bf_t(g_rbf_length);
		n_e = bf_t(g_rbf_length);
		n_f = bf_t(g_rbf_length);

		convert_bf(n_a, bt_a, g_rbf_length, orig_rbflength);
		convert_bf(n_b, bt_b, g_rbf_length, orig_rbflength);
		convert_bf(n_c, bt_c, g_rbf_length, orig_rbflength);
		convert_bf(n_d, bt_d, g_rbf_length, orig_rbflength);
		convert_bf(n_e, bt_e, g_rbf_length, orig_rbflength);
		convert_bf(n_f, bt_f, g_rbf_length, orig_rbflength);

		bf_t bt_t1(di_bflength);
		bf_t bt_t2(di_bflength);
		bf_t bt_t3(di_bflength);
		bf_t bt_t4(di_bflength);
		bf_t bt_t5(di_bflength);
		bf_t bt_t6(di_bflength);

		memcpy(bt_t1.storage(), mp_info->apm_data, two_di_len);
		memcpy(bt_t2.storage(), mp_info->apm_data + two_di_len, two_di_len);
		memcpy(bt_t3.storage(), mp_info->apm_data + 2*two_di_len, two_di_len);
		memcpy(bt_t4.storage(), mp_info->apm_data + 3*two_di_len, two_di_len);
		memcpy(bt_t5.storage(), mp_info->apm_data + 4*two_di_len, two_di_len);
		memcpy(bt_t6.storage(), mp_info->apm_data + 5*two_di_len, two_di_len);

		convert_bf(bt_xmin, bt_t1, two_len, two_di_len);
		convert_bf(bt_xmax, bt_t2, two_len, two_di_len);
		convert_bf(bt_ymin, bt_t3, two_len, two_di_len);
		convert_bf(bt_ymax, bt_t4, two_len, two_di_len);
		convert_bf(bt_x3rd, bt_t5, two_len, two_di_len);
		convert_bf(bt_y3rd, bt_t6, two_len, two_di_len);
	}

	// tranform maps real plane co-ords onto the current screen view
	// see above
	CoordinateD tl;
	is_visible_window_corner(*info, bt_x, bt_y, bt_xmin, bt_xmax, bt_ymin, bt_ymax, bt_x3rd, bt_y3rd, tl);
	list->itl.x = int(tl.x + 0.5);
	list->itl.y = int(tl.y + 0.5);

	CoordinateD tr;
	is_visible_window_corner(*info, bt_x, bt_y, bt_xmin, bt_xmax, bt_ymin, bt_ymax, bt_x3rd, bt_y3rd, tr);
	list->itr.x = int(tr.x + 0.5);
	list->itr.y = int(tr.y + 0.5);

	CoordinateD bl;
	is_visible_window_corner(*info, bt_x, bt_y, bt_xmin, bt_xmax, bt_ymin, bt_ymax, bt_x3rd, bt_y3rd, bl);
	list->ibl.x = int(bl.x + 0.5);
	list->ibl.y = int(bl.y + 0.5);

	CoordinateD br;
	is_visible_window_corner(*info, bt_x, bt_y, bt_xmin, bt_xmax, bt_ymin, bt_ymax, bt_x3rd, bt_y3rd, br);
	list->ibr.x = int(br.x + 0.5);
	list->ibr.y = int(br.y + 0.5);

	double tmp_sqrt = std::sqrt(sqr(tr.x-bl.x) + sqr(tr.y-bl.y));
	list->win_size = tmp_sqrt; // used for box vs crosshair in draw_window()
	// arbitrary value... stops browser zooming out too far
	bool cant_see = false;
	if (tmp_sqrt < g_browse_state.TooSmall())
	{
		cant_see = true;
	}
	// reject anything too small onscreen
	if (tmp_sqrt > toobig)
	{
		cant_see = true;
	}
	// or too big...

	// restore original values
	g_bf_length = orig_bflength;
	g_bn_length = orig_bnlength;
	g_padding = orig_padding;
	g_r_length = orig_rlength;
	g_shift_factor = orig_shiftfactor;
	g_rbf_length = orig_rbflength;

	if (cant_see) // do it this way so bignum stack is released
	{
		return false;
	}

	// now see how many corners are on the screen, accept if one or more
	return is_visible(tl) || is_visible(bl) || is_visible(tr) || is_visible(br);
}

static void draw_window(int color, CoordinateWindow *info)
{
#ifndef XFRACT
	int cross_size;
	Coordinate ibl;
	Coordinate itr;
#endif

	g_zoomBox.set_color(color);
	g_zoomBox.set_count(0);
	if (info->win_size >= g_browse_state.CrossHairBoxSize())
	{
		// big enough on screen to show up as a box so draw it
		// corner pixels
#ifndef XFRACT
		add_box(info->itl);
		add_box(info->itr);
		add_box(info->ibl);
		add_box(info->ibr);
		draw_lines(info->itl, info->itr, info->ibl.x-info->itl.x, info->ibl.y-info->itl.y); // top & bottom lines
		draw_lines(info->itl, info->ibl, info->itr.x-info->itl.x, info->itr.y-info->itl.y); // left & right lines
#else
		g_box_x[0] = info->itl.x + g_screen_x_offset;
		g_box_y[0] = info->itl.y + g_screen_y_offset;
		g_box_x[1] = info->itr.x + g_screen_x_offset;
		g_box_y[1] = info->itr.y + g_screen_y_offset;
		g_box_x[2] = info->ibr.x + g_screen_x_offset;
		g_box_y[2] = info->ibr.y + g_screen_y_offset;
		g_box_x[3] = info->ibl.x + g_screen_x_offset;
		g_box_y[3] = info->ibl.y + g_screen_y_offset;
		g_zoomBox.set_count(4);
#endif
		g_zoomBox.display();
	}
	else  // draw crosshairs
	{
#ifndef XFRACT
		cross_size = g_y_dots/45;
		if (cross_size < 2)
		{
			cross_size = 2;
		}
		itr.x = info->itl.x - cross_size;
		itr.y = info->itl.y;
		ibl.y = info->itl.y - cross_size;
		ibl.x = info->itl.x;
		draw_lines(info->itl, itr, ibl.x-itr.x, 0); // top & bottom lines
		draw_lines(info->itl, ibl, 0, itr.y-ibl.y); // left & right lines
		g_zoomBox.display();
#endif
	}
}

static void bfsetup_convert_to_screen()
{
	// setup_convert_to_screen() in LORENZ.C, converted to bf_math
	// Call only from within look_get_window()
	BigStackSaver savedStack;
	bf_t bt_inter1(g_rbf_length);
	bf_t bt_inter2(g_rbf_length);
	bf_t bt_det(g_rbf_length);
	bf_t bt_xd(g_rbf_length);
	bf_t bt_yd(g_rbf_length);
	bf_t bt_tmp1(g_rbf_length);
	bf_t bt_tmp2(g_rbf_length);

	// x3rd-xmin
	subtract_bf(bt_inter1, g_escape_time_state.m_grid_bf.x_3rd(), g_escape_time_state.m_grid_bf.x_min());
	// ymin-ymax
	subtract_bf(bt_inter2, g_escape_time_state.m_grid_bf.y_min(), g_escape_time_state.m_grid_bf.y_max());
	// (x3rd-xmin)*(ymin-ymax)
	multiply_bf(bt_tmp1, bt_inter1, bt_inter2);

	// ymax-y3rd
	subtract_bf(bt_inter1, g_escape_time_state.m_grid_bf.y_max(), g_escape_time_state.m_grid_bf.y_3rd());
	// xmax-xmin
	subtract_bf(bt_inter2, g_escape_time_state.m_grid_bf.x_max(), g_escape_time_state.m_grid_bf.x_min());
	// (ymax-y3rd)*(xmax-xmin)
	multiply_bf(bt_tmp2, bt_inter1, bt_inter2);

	// det = (x3rd-xmin)*(ymin-ymax) + (ymax-y3rd)*(xmax-xmin)
	add_bf(bt_det, bt_tmp1, bt_tmp2);

	// xd = g_dx_size/det
	floattobf(bt_tmp1, g_dx_size);
	div_bf(bt_xd, bt_tmp1, bt_det);

	// a = xd*(ymax-y3rd)
	subtract_bf(bt_inter1, g_escape_time_state.m_grid_bf.y_max(), g_escape_time_state.m_grid_bf.y_3rd());
	multiply_bf(bt_a, bt_xd, bt_inter1);

	// b = xd*(x3rd-xmin)
	subtract_bf(bt_inter1, g_escape_time_state.m_grid_bf.x_3rd(), g_escape_time_state.m_grid_bf.x_min());
	multiply_bf(bt_b, bt_xd, bt_inter1);

	// e = -(a*xmin + b*ymax)
	multiply_bf(bt_tmp1, bt_a, g_escape_time_state.m_grid_bf.x_min());
	multiply_bf(bt_tmp2, bt_b, g_escape_time_state.m_grid_bf.y_max());
	neg_a_bf(add_bf(bt_e, bt_tmp1, bt_tmp2));

	// x3rd-xmax
	subtract_bf(bt_inter1, g_escape_time_state.m_grid_bf.x_3rd(), g_escape_time_state.m_grid_bf.x_max());
	// ymin-ymax
	subtract_bf(bt_inter2, g_escape_time_state.m_grid_bf.y_min(), g_escape_time_state.m_grid_bf.y_max());
	// (x3rd-xmax)*(ymin-ymax)
	multiply_bf(bt_tmp1, bt_inter1, bt_inter2);

	// ymin-y3rd
	subtract_bf(bt_inter1, g_escape_time_state.m_grid_bf.y_min(), g_escape_time_state.m_grid_bf.y_3rd());
	// xmax-xmin
	subtract_bf(bt_inter2, g_escape_time_state.m_grid_bf.x_max(), g_escape_time_state.m_grid_bf.x_min());
	// (ymin-y3rd)*(xmax-xmin)
	multiply_bf(bt_tmp2, bt_inter1, bt_inter2);

	// det = (x3rd-xmax)*(ymin-ymax) + (ymin-y3rd)*(xmax-xmin)
	add_bf(bt_det, bt_tmp1, bt_tmp2);

	// yd = g_dy_size/det
	floattobf(bt_tmp2, g_dy_size);
	div_bf(bt_yd, bt_tmp2, bt_det);

	// c = yd*(ymin-y3rd)
	subtract_bf(bt_inter1, g_escape_time_state.m_grid_bf.y_min(), g_escape_time_state.m_grid_bf.y_3rd());
	multiply_bf(bt_c, bt_yd, bt_inter1);

	// d = yd*(x3rd-xmax)
	subtract_bf(bt_inter1, g_escape_time_state.m_grid_bf.x_3rd(), g_escape_time_state.m_grid_bf.x_max());
	multiply_bf(bt_d, bt_yd, bt_inter1);

	// f = -(c*xmin + d*ymax)
	multiply_bf(bt_tmp1, bt_c, g_escape_time_state.m_grid_bf.x_min());
	multiply_bf(bt_tmp2, bt_d, g_escape_time_state.m_grid_bf.y_max());
	neg_a_bf(add_bf(bt_f, bt_tmp1, bt_tmp2));
}

static bool fractal_types_match(const fractal_info &info, const formula_info_extension_block &formula_info)
{
	if (fractal_type_formula(g_fractal_type) && fractal_type_formula(info.fractal_type))
	{
		if (!stricmp(formula_info.form_name, g_formula_state.get_formula()))
		{
			int num_functions = g_formula_state.max_fn();
			return (num_functions > 0) ? functions_match(info, num_functions) : true;
		}
		else
		{
			return false; // two formulas but names don't match
		}
	}
	else if (info.fractal_type == g_fractal_type ||
			info.fractal_type == g_current_fractal_specific->tofloat)
	{
		int num_functions = g_current_fractal_specific->num_functions();
		return (num_functions > 0) ? functions_match(info, num_functions) : true;
	}
	return false; // no match
}

static bool functions_match(const fractal_info &info, int num_functions)
{
	for (int i = 0; i < num_functions; i++)
	{
		if (info.function_index[i] != g_function_index[i])
		{
			return false;
		}
	}
	return true;
}

static bool epsilon_equal(double x, double y, double epsilon = 0.001)
{
	return std::abs(x - y) < epsilon;
}

static bool parameters_match(const fractal_info &info)
{
	double parameter3;
	double parameter4;
	if (info.version > 6)
	{
		parameter3 = info.dparm3;
		parameter4 = info.dparm4;
	}
	else
	{
		parameter3 = info.parm3;
		round_float_d(&parameter3);
		parameter4 = info.parm4;
		round_float_d(&parameter4);
	}

	double parameter5 = 0.0;
	double parameter6 = 0.0;
	double parameter7 = 0.0;
	double parameter8 = 0.0;
	double parameter9 = 0.0;
	double parameter10 = 0.0;
	if (info.version > 8)
	{
		parameter5 = info.dparm5;
		parameter6 = info.dparm6;
		parameter7 = info.dparm7;
		parameter8 = info.dparm8;
		parameter9 = info.dparm9;
		parameter10 = info.dparm10;
	}

	// parameters are in range?
	return
		epsilon_equal(info.c_real, g_parameters[P1_REAL]) &&
		epsilon_equal(info.c_imag, g_parameters[P1_IMAG]) &&
		epsilon_equal(parameter3, g_parameters[P2_REAL]) &&
		epsilon_equal(parameter4, g_parameters[P2_IMAG]) &&
		epsilon_equal(parameter5, g_parameters[P3_REAL]) &&
		epsilon_equal(parameter6, g_parameters[P3_IMAG]) &&
		epsilon_equal(parameter7, g_parameters[P4_REAL]) &&
		epsilon_equal(parameter8, g_parameters[P4_IMAG]) &&
		epsilon_equal(parameter9, g_parameters[P5_REAL]) &&
		epsilon_equal(parameter10, g_parameters[P5_IMAG]) &&
		epsilon_equal(info.invert[0], g_inversion[0]);
}


static void check_history(const char *oldname, const char *newname)
{
	// g_file_name_stack[] is maintained in framain2.c.  It is the history
	// file for the browser and holds a maximum of 16 images.  The history
	// file needs to be adjusted if the rename or delete functions of the
	// browser are used.
	// g_name_stack_ptr is also maintained in framain2.c.  It is the index into
	// g_file_name_stack[].
	for (int i = 0; i < g_name_stack_ptr; i++)
	{
		if (stricmp(g_file_name_stack[i].c_str(), oldname) == 0) // we have a match
		{
			g_file_name_stack[i] = newname;    // insert the new name
		}
	}
}

// look_get_window reads all .GIF files and draws window outlines on the screen
int look_get_window()
{
	affine stack_cvt;
	fractal_info read_info;
	resume_info_extension_block resume_info_blk;
	formula_info_extension_block formula_info;
	ranges_info_extension_block ranges_info;
	multiple_precision_info_extension_block mp_info;
	evolver_info_extension_block evolver_info;
	orbits_info_extension_block orbits_info;
	time_t thistime;
	time_t lastime;
	char message[40];
	char new_name[60];
	std::string oldname;
	int index;
	int done;
	int wincount;
	int box_color;
	CoordinateWindow winlist;
	char drive[FILE_MAX_DRIVE];
	char dir[FILE_MAX_DIR];
	char fname[FILE_MAX_FNAME];
	char ext[FILE_MAX_EXT];
	char tmpmask[FILE_MAX_PATH];
	int vid_too_big = 0;
	bool no_memory = false;
	int vidlength;
#ifdef XFRACT
	U32 blinks;
#endif

	HelpModeSaver saved_help(IDHELP_BROWSE);
	oldbf_math = g_bf_math;
	g_bf_math = BIGFLT;
	if (!oldbf_math)
	{
		// kludge because next sets it = 0
		CalculationStatusType oldcalc_status = g_externs.CalculationStatus();
		fractal_float_to_bf();
		g_externs.SetCalculationStatus(oldcalc_status);
	}
	BigStackSaver savedStack;
	bt_a = bf_t(g_rbf_length);
	bt_b = bf_t(g_rbf_length);
	bt_c = bf_t(g_rbf_length);
	bt_d = bf_t(g_rbf_length);
	bt_e = bf_t(g_rbf_length);
	bt_f = bf_t(g_rbf_length);

	vidlength = g_screen_width + g_screen_height;
	if (vidlength > 4096)
	{
		vid_too_big = 2;
	}
	// 4096 based on 4096B in g_box_x... max 1/4 pixels plotted, and need words
	// 4096 = 10240/2.5 based on size of g_box_x + g_box_y + g_box_values
#ifdef XFRACT
	vidlength = 4; // X11 only needs the 4 corners saved.
#endif
	int *boxx_storage = new int[vidlength*MAX_WINDOWS_OPEN];
	int *boxy_storage = new int[vidlength*MAX_WINDOWS_OPEN];
	int *boxvalues_storage = new int[vidlength/2*MAX_WINDOWS_OPEN];
	if (!boxx_storage || !boxy_storage || !boxvalues_storage)
	{
		no_memory = true;
	}

	// set up complex-plane-to-screen transformation
	if (oldbf_math)
	{
		bfsetup_convert_to_screen();
	}
	else
	{
		cvt = &stack_cvt; // use stack
		setup_convert_to_screen(cvt);
		// put in bf variables
		floattobf(bt_a, cvt->a);
		floattobf(bt_b, cvt->b);
		floattobf(bt_c, cvt->c);
		floattobf(bt_d, cvt->d);
		floattobf(bt_e, cvt->e);
		floattobf(bt_f, cvt->f);
	}
	g_.DAC().FindSpecialColors();
	box_color = g_.DAC().Medium();

rescan:  // entry for changed browse parms
	time(&lastime);
	bool toggle = false;
	wincount = 0;
	g_browse_state.SetSubImages(true);
	split_path(g_read_name, drive, dir, 0, 0);
	split_path(g_browse_state.Mask(), 0, 0, fname, ext);
	make_path(tmpmask, drive, dir, fname, ext);
	done = (vid_too_big == 2) || no_memory || fr_find_first(tmpmask);
	// draw all visible windows
	while (!done)
	{
		if (driver_key_pressed())
		{
			driver_get_key();
			break;
		}
		split_path(g_dta.filename, 0, 0, fname, ext);
		make_path(tmpmask, drive, dir, fname, ext);
		if (!find_fractal_info(tmpmask, &read_info, &resume_info_blk, &formula_info,
			&ranges_info, &mp_info, &evolver_info, &orbits_info)
			&& (fractal_types_match(read_info, formula_info) || !g_browse_state.CheckType())
			&& (parameters_match(read_info) || !g_browse_state.CheckParameters())
			&& stricmp(g_browse_state.Name().c_str(), g_dta.filename.c_str())
			&& evolver_info.got_data != 1
			&& is_visible_window(&winlist, &read_info, &mp_info))
		{
			winlist.name = g_dta.filename;
			draw_window(box_color, &winlist);
			g_zoomBox.set_count(g_zoomBox.count()*2); // double for byte count
			winlist.box_count = g_zoomBox.count();
			browse_windows[wincount] = winlist;

			g_zoomBox.save(&boxx_storage[wincount*vidlength],
				&boxy_storage[wincount*vidlength],
				&boxvalues_storage[wincount*vidlength/2], vidlength);
			wincount++;
		}

		if (resume_info_blk.got_data == 1) // Clean up any memory allocated
		{
			delete[] resume_info_blk.resume_data;
		}
		if (ranges_info.got_data == 1) // Clean up any memory allocated
		{
			delete[] ranges_info.range_data;
		}
		if (mp_info.got_data == 1) // Clean up any memory allocated
		{
			delete[] mp_info.apm_data;
		}

		done = (fr_find_next() || wincount >= MAX_WINDOWS_OPEN);
	}

	if (no_memory)
	{
		text_temp_message("Sorry...not enough memory to browse."); // doesn't work if NO memory available, go figure
	}
	if (wincount >= MAX_WINDOWS_OPEN)
	{ // hard code message at MAX_WINDOWS_OPEN = 450
		text_temp_message("Sorry...no more space, 450 displayed.");
	}
	if (vid_too_big == 2)
	{
		text_temp_message("Xdots + Ydots > 4096.");
	}
	int c = 0;
	if (wincount)
	{
		driver_buzzer(BUZZER_COMPLETE); // let user know we've finished
		index = 0;
		done = 0;
		winlist = browse_windows[index];
		g_zoomBox.restore(&boxx_storage[index*vidlength],
			&boxy_storage[index*vidlength],
			&boxvalues_storage[index*vidlength/2], vidlength);
		show_temp_message(winlist.name);

		// on exit done = 1 for quick exit,
		//		   done = 2 for erase boxes and  exit
		//		   done = 3 for rescan
		//		   done = 4 for set boxes and exit to save image
		while (!done)
		{
#ifdef XFRACT
			blinks = 1;
#endif
			// TODO: refactor to IInputContext
			while (!driver_key_pressed())
			{
				time(&thistime);
				if (difftime(thistime, lastime) > .2)
				{
					lastime = thistime;
					toggle = !toggle;
				}
				draw_window(toggle ? g_.DAC().Bright() : g_.DAC().Dark(), &winlist);   // flash current window
#ifdef XFRACT
				blinks++;
#endif
			}
#ifdef XFRACT
			if ((blinks & 1) == 1)   // Need an odd # of blinks, so next one leaves box turned off
			{
				draw_window(g_.DAC().Bright(), &winlist);
			}
#endif

			c = driver_get_key();
			switch (c)
			{
			case IDK_RIGHT_ARROW:
			case IDK_LEFT_ARROW:
			case IDK_DOWN_ARROW:
			case IDK_UP_ARROW:
				clear_temp_message();
				draw_window(box_color, &winlist); // dim last window
				if (c == IDK_RIGHT_ARROW || c == IDK_UP_ARROW)
				{
					index++;                     // shift attention to next window
					if (index >= wincount)
					{
						index = 0;
					}
				}
				else
				{
					index --;
					if (index < 0)
					{
						index = wincount -1;
					}
				}
				winlist = browse_windows[index];
				g_zoomBox.restore(&boxx_storage[index*vidlength],
					&boxy_storage[index*vidlength],
					&boxvalues_storage[index*vidlength/2], vidlength);
				show_temp_message(winlist.name);
				break;
#ifndef XFRACT
			case IDK_CTL_INSERT:
				box_color += key_count(IDK_CTL_INSERT);
				for (int i = 0; i < wincount; i++)
				{
					winlist = browse_windows[i];
					draw_window(box_color, &winlist);
				}
				winlist = browse_windows[index];
				draw_window(box_color, &winlist);
				break;

			case IDK_CTL_DEL:
				box_color -= key_count(IDK_CTL_DEL);
				for (int i = 0; i < wincount; i++)
				{
					winlist = browse_windows[i];
					draw_window(box_color, &winlist);
				}
				winlist = browse_windows[index];
				draw_window(box_color, &winlist);
				break;
#endif
			case IDK_ENTER:
			case IDK_ENTER_2:   // this file please
				g_browse_state.SetName(winlist.name);
				done = 1;
				break;

			case IDK_ESC:
			case 'l':
			case 'L':
#ifdef XFRACT
				// Need all boxes turned on, turn last one back on.
				draw_window(g_.DAC().Bright(), &winlist);
#endif
				g_browse_state.SetAutoBrowse(false);
				done = 2;
				break;

			case 'D': // delete file
				clear_temp_message();
				show_temp_message("Delete " + winlist.name + "? (Y/N)");
				driver_wait_key_pressed(0);
				clear_temp_message();
				c = driver_get_key();
				if (c == 'Y' && g_browse_state.DoubleCaution())
				{
					text_temp_message("ARE YOU SURE???? (Y/N)");
					if (driver_get_key() != 'Y')
					{
						c = 'N';
					}
				}
				if (c == 'Y')
				{
					split_path(g_read_name, drive, dir, 0, 0);
					split_path(winlist.name, 0, 0, fname, ext);
					make_path(tmpmask, drive, dir, fname, ext);
					if (!unlink(tmpmask))
					{
						// do a rescan
						done = 3;
						oldname = winlist.name;
						tmpmask[0] = '\0';
						check_history(oldname.c_str(), tmpmask);
						break;
					}
					else if (errno == EACCES)
					{
						text_temp_message("Sorry...it's a read only file, can't del");
						show_temp_message(winlist.name);
						break;
					}
				}
				text_temp_message("file not deleted (phew!)");
				show_temp_message(winlist.name);
				break;

			case 'R':
				clear_temp_message();
				driver_stack_screen();
				new_name[0] = 0;
				strcpy(message, "Enter the new filename for ");
				split_path(g_read_name, drive, dir, 0, 0);
				split_path(winlist.name, 0, 0, fname, ext);
				make_path(tmpmask, drive, dir, fname, ext);
				strcpy(new_name, tmpmask);
				strcat(message, tmpmask);
				{
					int i = field_prompt(message, new_name, 60);
					driver_unstack_screen();
					if (i != -1)
					{
						if (!rename(tmpmask, new_name))
						{
							if (errno == EACCES)
							{
								text_temp_message("Sorry....can't rename");
							}
							else
							{
								split_path(new_name, 0, 0, fname, ext);
								make_path(tmpmask, 0, 0, fname, ext);
								oldname = winlist.name;
								check_history(oldname.c_str(), tmpmask);
								winlist.name = tmpmask;
							}
						}
					}
				}
				browse_windows[index] = winlist;
				show_temp_message(winlist.name);
				break;

			case IDK_CTL_B:
				clear_temp_message();
				{
					ScreenStacker stacker;
					done = abs(g_browse_state.GetParameters());
				}
				show_temp_message(winlist.name);
				break;

			case 's': // save image with boxes
				g_browse_state.SetAutoBrowse(false);
				draw_window(box_color, &winlist); // current window white
				done = 4;
				break;

			case '\\': // back out to last image
				done = 2;
				break;

			default:
				break;
			}
		}

		// now clean up memory (and the screen if necessary)
		clear_temp_message();
		if (done >= 1 && done < 4)
		{
			for (index = wincount-1; index >= 0; index--) // don't need index, reuse it
			{
				winlist = browse_windows[index];
				g_zoomBox.set_count(winlist.box_count);
				g_zoomBox.restore(&boxx_storage[index*vidlength],
					&boxy_storage[index*vidlength],
					&boxvalues_storage[index*vidlength/2], vidlength);
				g_zoomBox.set_count(g_zoomBox.count()*2);
				if (g_zoomBox.count() > 0)
				{
#ifdef XFRACT
					// Turn all boxes off
					draw_window(g_.DAC().Bright(), &winlist);
#else
					g_zoomBox.clear();
#endif
				}
			}
		}
		if (done == 3)
		{
			goto rescan; // hey everybody I just used the g word!
		}
	}
	else
	{
		driver_buzzer(BUZZER_INTERRUPT); // no suitable files in directory!
		text_temp_message("Sorry.. I can't find anything");
		g_browse_state.SetSubImages(false);
	}

	delete[] boxx_storage;
	delete[] boxy_storage;
	delete[] boxvalues_storage;
	if (!oldbf_math)
	{
		free_bf_vars();
	}
	g_bf_math = oldbf_math;
	g_float_flag = (g_user_float_flag != 0);

	return c;
}

static bool look(bool &stacked)
{
	switch (look_get_window())
	{
	case IDK_ENTER:
	case IDK_ENTER_2:
		g_show_file = SHOWFILE_PENDING;       // trigger load
		g_browse_state.SetBrowsing(true);    // but don't ask for the file name as it's just been selected
		if (g_name_stack_ptr == 15)
		{
			// about to run off the end of the file
			// history stack so shift it all back one to
			// make room, lose the 1st one
			for (int tmp = 1; tmp < 16; tmp++)
			{
				g_file_name_stack[tmp - 1] = g_file_name_stack[tmp];
			}
			g_name_stack_ptr = 14;
		}
		g_name_stack_ptr++;
		g_file_name_stack[g_name_stack_ptr] = g_browse_state.Name();
		g_browse_state.MergePathNames(g_read_name);
		if (g_ui_state.ask_video)
		{
				driver_stack_screen();   // save graphics image
				stacked = true;
		}
		return true;       // hop off and do it!!

	case '\\':
		if (g_name_stack_ptr >= 1)
		{
			// go back one file if somewhere to go (ie. browsing)
			g_name_stack_ptr--;
			while (g_file_name_stack[g_name_stack_ptr].length() == 0
					&& g_name_stack_ptr >= 0)
			{
				g_name_stack_ptr--;
			}
			if (g_name_stack_ptr < 0) // oops, must have deleted first one
			{
				break;
			}
			g_browse_state.SetName(g_file_name_stack[g_name_stack_ptr]);
			g_browse_state.MergePathNames(g_read_name);
			g_browse_state.SetBrowsing(true);
			g_show_file = SHOWFILE_PENDING;
			if (g_ui_state.ask_video)
			{
				driver_stack_screen(); // save graphics image
				stacked = true;
			}
			return true;
		}                   // otherwise fall through and turn off browsing
	case IDK_ESC:
	case 'l':              // turn it off
	case 'L':
		g_browse_state.SetBrowsing(false);
		break;

	case 's':
		g_browse_state.SetBrowsing(false);
		save_to_disk(g_save_name);
		break;

	default:               // or no files found, leave the state of browsing alone
		break;
	}

	return false;
}

ApplicationStateType handle_look_for_files(bool &stacked)
{
	if ((g_z_width != 0) || driver_diskp())
	{
		g_browse_state.SetBrowsing(false);
		driver_buzzer(BUZZER_ERROR);             // can't browse if zooming or disk video
	}
	else if (look(stacked))
	{
		return APPSTATE_RESTORE_START;
	}
	return APPSTATE_NO_CHANGE;
}