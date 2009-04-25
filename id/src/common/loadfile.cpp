/*
		loadfile.c - load an existing fractal image, control level
*/
#include <ctime>
#include <string>

#include "port.h"
#include "prototyp.h"
#include "fractype.h"
#include "helpdefs.h"

#include "biginit.h"
#include "calcfrac.h"
#include "drivers.h"
#include "EscapeTime.h"
#include "evolve.h"
#include "Externals.h"
#include "filesystem.h"
#include "FiniteAttractor.h"
#include "Formula.h"
#include "fracsubr.h"
#include "framain2.h"
#include "idhelp.h"
#include "loadfdos.h"
#include "loadfile.h"
#include "lorenz.h"
#include "miscres.h"
#include "prompts1.h"
#include "prompts2.h"
#include "resume.h"
#include "StopMessage.h"
#include "ThreeDimensionalState.h"
#include "ViewWindow.h"
#include "zoom.h"

enum
{
	BLOCKTYPE_MAIN_INFO		= 1,
	BLOCKTYPE_RESUME_INFO	= 2,
	BLOCKTYPE_FORMULA_INFO	= 3,
	BLOCKTYPE_RANGES_INFO	= 4,
	BLOCKTYPE_MP_INFO		= 5,
	BLOCKTYPE_EVOLVER_INFO	= 6,
	BLOCKTYPE_ORBITS_INFO	= 7
};

// routines in this module

static void load_ext_blk(char *loadptr, int loadlen);
static void skip_ext_blk(int *, int *);
static void translate_obsolete_fractal_types(const fractal_info *info);
static bool fix_bof();
static bool fix_period_bof();

bool g_loaded_3d = false;
int g_file_y_dots = 0;
int g_file_x_dots = 0;
int g_file_colors = 0;
float g_file_aspect_ratio = 0.0;
int g_skip_x_dots = 0;
int g_skip_y_dots = 0;      // for decoder, when reducing image
bool g_use_old_complex_power = false;

static void read_info_version_0(const fractal_info &read_info)
{
	g_invert = 0;

	// TODO: handle old crap or abort?
	if (read_info.version > 0)
	{
		g_parameters[P2_REAL] = read_info.parm3;
		round_float_d(&g_parameters[P2_REAL]);
		g_parameters[P2_IMAG] = read_info.parm4;
		round_float_d(&g_parameters[P2_IMAG]);
		g_potential_parameter[0] = read_info.potential[0];
		g_potential_parameter[1] = read_info.potential[1];
		g_potential_parameter[2] = read_info.potential[2];
		if (!g_make_par_flag)
		{
			g_colors = read_info.colors;
		}
		g_potential_flag = (g_potential_parameter[0] != 0.0);
		g_use_fixed_random_seed = (read_info.random_flag != 0);
		g_random_seed = read_info.random_seed;
		g_externs.SetInside(read_info.inside);
		g_log_palette_mode = read_info.logmapold;
		g_inversion[0] = read_info.invert[0];
		g_inversion[1] = read_info.invert[1];
		g_inversion[2] = read_info.invert[2];
		if (g_inversion[0] != 0.0)
		{
			g_invert = 3;
		}
		g_decomposition[0] = read_info.decomposition[0];
		g_decomposition[1] = read_info.decomposition[1];
		g_externs.SetUserBiomorph(read_info.biomorph);
		g_force_symmetry = read_info.symmetry;
	}
}

static void read_info_version_1(const fractal_info &read_info)
{
	// TODO: handle old crap or abort?
	if (read_info.version > 1)
	{
		g_save_release = 1200;
		if (!g_display_3d
			&& (read_info.version <= 4 || read_info.flag3d > 0
			|| (g_current_fractal_specific->flags & FRACTALFLAG_3D_PARAMETERS)))
		{
			g_3d_state.set_raytrace_parameters(&read_info.init_3d[0]);
			g_3d_state.set_preview_factor(read_info.previewfactor);
			g_3d_state.set_x_trans(read_info.xtrans);
			g_3d_state.set_y_trans(read_info.ytrans);
			g_3d_state.set_red().set_crop_left(read_info.red_crop_left);
			g_3d_state.set_red().set_crop_right(read_info.red_crop_right);
			g_3d_state.set_blue().set_crop_left(read_info.blue_crop_left);
			g_3d_state.set_blue().set_crop_right(read_info.blue_crop_right);
			g_3d_state.set_red().set_bright(read_info.red_bright);
			g_3d_state.set_blue().set_bright(read_info.blue_bright);
			g_3d_state.set_x_adjust(read_info.xadjust);
			g_3d_state.set_eye_separation(read_info.eyeseparation);
			g_3d_state.set_glasses_type(GlassesType(read_info.glassestype));
		}
	}
}

static void read_info_version_2(const fractal_info &read_info)
{
	// TODO: handle old crap or abort?
	if (read_info.version > 2)
	{
		g_save_release = 1300;
		g_externs.SetOutside(read_info.outside);
	}
}

static void read_info_version_3(const fractal_info &read_info)
{
	g_externs.SetCalculationStatus(CALCSTAT_PARAMS_CHANGED);       // defaults if version < 4
	g_escape_time_state.m_grid_fp.x_3rd() = g_escape_time_state.m_grid_fp.x_min();
	g_escape_time_state.m_grid_fp.y_3rd() = g_escape_time_state.m_grid_fp.y_min();
	g_user_distance_test = 0;
	g_calculation_time = 0;

	// TODO: handle old crap or abort?
	if (read_info.version > 3)
	{
		g_save_release = 1400;
		g_escape_time_state.m_grid_fp.x_3rd() = read_info.x_3rd;
		g_escape_time_state.m_grid_fp.y_3rd() = read_info.y_3rd;
		g_externs.SetCalculationStatus(CalculationStatusType(read_info.calculation_status));
		g_externs.SetUserStandardCalculationMode(CalculationMode(read_info.stdcalcmode));
		g_three_pass = false;
		if (g_externs.UserStandardCalculationMode() == CalculationMode(127))
		{
			g_three_pass = true;
			g_externs.SetUserStandardCalculationMode(CALCMODE_TRIPLE_PASS);
		}
		g_user_distance_test = read_info.distestold;
		g_user_float_flag = (read_info.float_flag != 0);
		g_externs.SetBailOut(read_info.bailoutold);
		g_calculation_time = read_info.calculation_time;
		g_function_index[0] = read_info.function_index[0];
		g_function_index[1] = read_info.function_index[1];
		g_function_index[2] = read_info.function_index[2];
		g_function_index[3] = read_info.function_index[3];
		g_finite_attractor = read_info.finattract;
		g_initial_orbit_z.real(read_info.initial_orbit_z[0]);
		g_initial_orbit_z.imag(read_info.initial_orbit_z[1]);
		g_externs.SetUseInitialOrbitZ(InitialZType(read_info.use_initial_orbit_z));
		g_user_periodicity_check = read_info.periodicity;
	}
}

static void read_info_version_4(const fractal_info &read_info)
{
	g_potential_16bit = false;

	// TODO: handle old crap or abort?
	if (read_info.version > 4)
	{
		g_potential_16bit = (read_info.potential_16bit != 0);
		if (g_potential_16bit)
		{
			g_file_x_dots >>= 1;
		}
		g_file_aspect_ratio = read_info.aspect_ratio;
		if (g_file_aspect_ratio < 0.01)       // fix files produced in early v14.1
		{
			g_file_aspect_ratio = g_screen_aspect_ratio;
		}
		g_save_release = read_info.release; // from fmt 5 on we know real number
		if (read_info.version == 5        // except a few early fmt 5 cases:
			&& (g_save_release <= 0 || g_save_release >= 4000))
		{
			// TODO: handle old crap or abort?
			g_save_release = 1410;
		}
		if (!g_display_3d && read_info.flag3d > 0)
		{
			g_loaded_3d = true;
			g_3d_state.set_ambient(read_info.ambient);
			g_3d_state.set_randomize_colors(read_info.randomize);
			g_3d_state.set_haze(read_info.haze);
			g_3d_state.set_transparent0(read_info.transparent[0]);
			g_3d_state.set_transparent1(read_info.transparent[1]);
		}
	}
}

static void read_info_version_5(const fractal_info &read_info)
{
	g_rotate_lo = 1;
	g_rotate_hi = 255;
	g_distance_test_width = 71;

	// TODO: handle old crap or abort?
	if (read_info.version > 5)
	{
		g_rotate_lo = read_info.rotate_lo;
		g_rotate_hi = read_info.rotate_hi;
		g_distance_test_width = read_info.distance_test_width;
	}
}

static void read_info_version_6(const fractal_info &read_info)
{
	// TODO: handle old crap or abort?
	if (read_info.version > 6)
	{
		g_parameters[P2_REAL] = read_info.dparm3;
		g_parameters[P2_IMAG] = read_info.dparm4;
	}
}

static void read_info_version_7(const fractal_info &read_info)
{
	// TODO: handle old crap or abort?
	if (read_info.version > 7)
	{
		g_fill_color = read_info.fill_color;
	}
}

static void read_info_version_8(const fractal_info &read_info)
{
	// TODO: handle old crap or abort?
	if (read_info.version > 8)
	{
		g_m_x_max_fp = read_info.mxmaxfp;
		g_m_x_min_fp = read_info.mxminfp;
		g_m_y_max_fp = read_info.mymaxfp;
		g_m_y_min_fp = read_info.myminfp;
		g_z_dots = read_info.zdots;
		g_origin_fp = read_info.originfp;
		g_depth_fp = read_info.depthfp;
		g_height_fp = read_info.heightfp;
		g_width_fp = read_info.widthfp;
		g_screen_distance_fp = read_info.screen_distance_fp;
		g_eyes_fp = read_info.eyesfp;
		g_new_orbit_type = read_info.orbittype;
		g_juli_3d_mode = read_info.juli3Dmode;
		g_formula_state.set_max_fn(read_info.max_fn);
		g_major_method = MajorMethodType(read_info.inversejulia >> 8);
		g_minor_method = MinorMethodType(read_info.inversejulia & 255);
		g_parameters[P3_REAL] = read_info.dparm5;
		g_parameters[P3_IMAG] = read_info.dparm6;
		g_parameters[P4_REAL] = read_info.dparm7;
		g_parameters[P4_IMAG] = read_info.dparm8;
		g_parameters[P5_REAL] = read_info.dparm9;
		g_parameters[P5_IMAG] = read_info.dparm10;
	}
}

static void read_info_pre_version_14(const fractal_info &read_info)
{
	// TODO: handle old crap or abort?
	if (read_info.version < 4 && read_info.version != 0) // pre-version 14.0?
	{
		translate_obsolete_fractal_types(&read_info);
		if (g_log_palette_mode)
		{
			g_log_palette_mode = 2;
		}
		g_user_float_flag = (g_current_fractal_specific->isinteger != 0);
	}
}

static void read_info_pre_version_15(const fractal_info &read_info)
{
	// TODO: handle old crap or abort?
	if (read_info.version < 5 && read_info.version != 0) // pre-version 15.0?
	{
		if (g_log_palette_mode == 2) // logmap = old changed again in format 5!
		{
			g_log_palette_mode = LOGPALETTE_OLD;
		}
		if (g_decomposition[0] > 0 && g_decomposition[1] > 0)
		{
			g_externs.SetBailOut(g_decomposition[1]);
		}
	}

	if (g_potential_flag) // in version 15.x and 16.x logmap didn't work with pot
	{
		if (read_info.version == 6 || read_info.version == 7)
		{
			g_log_palette_mode = LOGPALETTE_NONE;
		}
	}
	set_trig_pointers(-1);
}

static void read_info_pre_version_18(const fractal_info &read_info)
{
	// TODO: handle old crap or abort?
	if (read_info.version < 9 && read_info.version != 0) // pre-version 18.0?
	{
		/* g_force_symmetry==FORCESYMMETRY_SEARCH means we want to force symmetry but don't
		know which symmetry yet, will find out in setsymmetry() */
		if (g_externs.Outside() == COLORMODE_REAL
			|| g_externs.Outside() == COLORMODE_IMAGINARY
			|| g_externs.Outside() == COLORMODE_MULTIPLY
			|| g_externs.Outside() == COLORMODE_SUM
			|| g_externs.Outside() == COLORMODE_INVERSE_TANGENT)
		{
			if (g_force_symmetry == FORCESYMMETRY_NONE)
			{
				g_force_symmetry = FORCESYMMETRY_SEARCH;
			}
		}
	}
}

static void read_info_pre_version_17_25(const fractal_info &read_info)
{
	// TODO: handle old crap or abort?
	if (g_save_release < 1725 && read_info.version != 0) // pre-version 17.25
	{
		set_if_old_bif(); // translate bifurcation types
		g_function_preloaded = true;
	}
}

static void read_info_version_9(const fractal_info &read_info)
{
	// TODO: handle old crap or abort?
	if (read_info.version > 9)
	{ // post-version 18.22
		g_externs.SetBailOut(read_info.bail_out); // use long bailout
		g_externs.SetBailOutTest(BailOutType(read_info.bailoutest));
	}
	else
	{
		g_externs.SetBailOutTest(BAILOUT_MODULUS);
	}
	set_bail_out_formula(g_externs.BailOutTest());
	if (read_info.version > 9)
	{
		// post-version 18.23
		g_max_iteration = read_info.iterations; // use long maxit
		// post-version 18.27
		g_old_demm_colors = (read_info.old_demm_colors != 0);
	}
}

static void read_info_version_10(const fractal_info &read_info)
{
	// TODO: handle old crap or abort?
	if (read_info.version > 10) // post-version 19.20
	{
		g_log_palette_mode = read_info.logmap;
		g_user_distance_test = read_info.distance_test;
	}
}

static void read_info_version_11(const fractal_info &read_info)
{
	// TODO: handle old crap or abort?
	if (read_info.version > 11) // post-version 19.20, inversion fix
	{
		g_inversion[0] = read_info.dinvert[0];
		g_inversion[1] = read_info.dinvert[1];
		g_inversion[2] = read_info.dinvert[2];
		g_log_dynamic_calculate = read_info.logcalc;
		g_externs.SetStopPass(read_info.stop_pass);
	}
}

static void read_info_version_12(const fractal_info &read_info)
{
	// TODO: handle old crap or abort?
	if (read_info.version > 12) // post-version 19.60
	{
		g_quick_calculate = (read_info.quick_calculate != 0);
		g_proximity = read_info.proximity;
		if (g_fractal_type == FRACTYPE_POPCORN_FP || g_fractal_type == FRACTYPE_POPCORN_L ||
			g_fractal_type == FRACTYPE_POPCORN_JULIA_FP || g_fractal_type == FRACTYPE_POPCORN_JULIA_L ||
			g_fractal_type == FRACTYPE_LATOOCARFIAN)
		{
			g_function_preloaded = true;
		}
	}
}

static void read_info_version_13(const fractal_info &read_info)
{
	// TODO: handle old crap or abort?
	g_beauty_of_fractals = true;
	if (read_info.version > 13) // post-version 20.1.2
	{
		g_beauty_of_fractals = (read_info.no_bof == 0);
	}
}

static void read_info_version_14()
{
	// TODO: handle old crap or abort?
	// if (read_info.version > 14)  post-version 20.1.12
	// modified saved evolver structure JCO 12JUL01
	g_log_automatic_flag = false;  // make sure it's turned off
}

static void read_info_version_15(const fractal_info &read_info)
{
	// TODO: handle old crap or abort?
	g_orbit_interval = 1;
	if (read_info.version > 15) // post-version 20.3.2
	{
		g_orbit_interval = read_info.orbit_interval;
	}
}

static void read_info_version_16(const fractal_info &read_info)
{
	// TODO: handle old crap or abort?
	g_orbit_delay = 0;
	g_math_tolerance[0] = 0.05;
	g_math_tolerance[1] = 0.05;
	if (read_info.version > 16) // post-version 20.4.0
	{
		g_orbit_delay = read_info.orbit_delay;
		g_math_tolerance[0] = read_info.math_tolerance[0];
		g_math_tolerance[1] = read_info.math_tolerance[1];
	}
}

void free_ranges()
{
	delete[] g_ranges;
	g_ranges = 0;
	g_ranges_length = 0;
}

static void got_resume_info(resume_info_extension_block const &resume_info)
{
	end_resume();

	if (resume_info.got_data == 1)
	{
		g_resume_info = resume_info.resume_data;
		g_resume_length = resume_info.length;
	}
}

static void got_formula_info(fractal_info const &read_info, formula_info_extension_block formula_info)
{
	if (formula_info.got_data != 1)
	{
		return;
	}

	formula_info.form_name[ITEMNAMELEN] = 0;
	switch (read_info.fractal_type)
	{
	case FRACTYPE_L_SYSTEM:
		g_l_system_name = formula_info.form_name;
		break;

	case FRACTYPE_IFS:
	case FRACTYPE_IFS_3D:
		g_ifs_name = formula_info.form_name;
		break;

	default:
		g_formula_state.set_uses_p1(formula_info.uses_p1 != 0);
		g_formula_state.set_uses_p2(formula_info.uses_p2 != 0);
		g_formula_state.set_uses_p3(formula_info.uses_p3 != 0);
		g_formula_state.set_uses_p4(formula_info.uses_p4 != 0);
		g_formula_state.set_uses_p5(formula_info.uses_p5 != 0);
		g_formula_state.set_uses_is_mand(formula_info.uses_is_mand != 0);
		g_is_mandelbrot = formula_info.ismand != 0;
		g_formula_state.set_formula(formula_info.form_name);
		break;
	}
	// perhaps in future add more here, check block_len for backward compatibility
}

static void got_ranges_info(ranges_info_extension_block const &ranges_info)
{
	free_ranges();
	if (ranges_info.got_data == 1)
	{
		g_ranges = (int *) ranges_info.range_data;
		g_ranges_length = ranges_info.length;
#ifdef XFRACT
		fix_ranges(g_ranges, g_ranges_length, 1);
#endif
	}
}

static void got_multiple_precision_info(const fractal_info &read_info, multiple_precision_info_extension_block const &mp_info)
{
	if (mp_info.got_data == 1)
	{
		g_bf_math = BIG_NUMBER;
		init_bf_length(read_info.bflength);
		memcpy(g_escape_time_state.m_grid_bf.x_min().storage(), mp_info.apm_data, mp_info.length);
		delete[] mp_info.apm_data;
	}
	else
	{
		g_bf_math = BIG_NONE;
	}
}

static void got_evolver_info(const fractal_info &read_info, evolver_info_extension_block evolver_info)
{
	if (evolver_info.got_data != 1)
	{
		g_evolving_flags = EVOLVE_NONE;
		return;
	}

	// TODO: handle old crap or abort?
	if (read_info.version < 15)
	{
		// Increasing NUM_GENES moves ecount in the data structure
		// We added 4 to NUM_GENES, so ecount is at NUM_GENES-4
		evolver_info.ecount = evolver_info.mutate[NUM_GENES - 4];
	}
	if (evolver_info.ecount != evolver_info.grid_size*evolver_info.grid_size
		&& g_externs.CalculationStatus() != CALCSTAT_COMPLETED)
	{
		g_externs.SetCalculationStatus(CALCSTAT_RESUMABLE);
		evolution_info resume_e_info;
		if (g_evolve_info == 0)
		{
			g_evolve_info = new evolution_info;
		}
		resume_e_info.parameter_range_x = evolver_info.parameter_range_x;
		resume_e_info.parameter_range_y = evolver_info.parameter_range_y;
		resume_e_info.opx = evolver_info.opx;
		resume_e_info.opy = evolver_info.opy;
		resume_e_info.odpx = evolver_info.odpx;
		resume_e_info.odpy = evolver_info.odpy;
		resume_e_info.px = evolver_info.px;
		resume_e_info.py = evolver_info.py;
		resume_e_info.screen_x_offset = evolver_info.sxoffs;
		resume_e_info.screen_y_offset = evolver_info.syoffs;
		resume_e_info.x_dots = evolver_info.x_dots;
		resume_e_info.y_dots = evolver_info.y_dots;
		resume_e_info.grid_size = evolver_info.grid_size;
		resume_e_info.evolving = evolver_info.evolving;
		resume_e_info.this_generation_random_seed = evolver_info.this_generation_random_seed;
		resume_e_info.fiddle_factor = evolver_info.fiddle_factor;
		resume_e_info.ecount = evolver_info.ecount;
		*g_evolve_info = resume_e_info;
	}
	else
	{
		delete g_evolve_info;
		g_evolve_info = 0;
		g_externs.SetCalculationStatus(CALCSTAT_COMPLETED);
	}
	g_parameter_range_x = evolver_info.parameter_range_x;
	g_parameter_range_y = evolver_info.parameter_range_y;
	g_parameter_offset_x = g_new_parameter_offset_x = evolver_info.opx;
	g_parameter_offset_y = g_new_parameter_offset_y = evolver_info.opy;
	g_new_discrete_parameter_offset_x = evolver_info.odpx;
	g_new_discrete_parameter_offset_y = evolver_info.odpy;
	g_discrete_parameter_offset_x = g_new_discrete_parameter_offset_x;
	g_discrete_parameter_offset_y = g_new_discrete_parameter_offset_y;
	g_px = evolver_info.px;
	g_py = evolver_info.py;
	g_screen_x_offset = evolver_info.sxoffs;
	g_screen_y_offset = evolver_info.syoffs;
	g_x_dots = evolver_info.x_dots;
	g_y_dots = evolver_info.y_dots;
	g_grid_size = evolver_info.grid_size;
	g_this_generation_random_seed = evolver_info.this_generation_random_seed;
	g_fiddle_factor = evolver_info.fiddle_factor;
	g_evolving_flags = evolver_info.evolving;
	g_viewWindow.Show(g_evolving_flags != 0);
	g_delta_parameter_image_x = g_parameter_range_x/(g_grid_size - 1);
	g_delta_parameter_image_y = g_parameter_range_y/(g_grid_size - 1);
	if (read_info.version > 14)
	{
		for (int i = 0; i < NUM_GENES; i++)
		{
			g_genes[i].mutate = int(evolver_info.mutate[i]);
		}
	}
	else
	{
		for (int i = 0; i < 6; i++)
		{
			g_genes[i].mutate = int(evolver_info.mutate[i]);
		}
		for (int i = 6; i < 10; i++)
		{
			g_genes[i].mutate = 0;
		}
		for (int i = 10; i < NUM_GENES; i++)
		{
			g_genes[i].mutate = int(evolver_info.mutate[i-4]);
		}
	}
	save_parameter_history();
}

static void got_orbits_info(orbits_info_extension_block const &orbits_info)
{
	if (orbits_info.got_data != 1)
	{
		return;
	}

	g_orbit_x_min = orbits_info.oxmin;
	g_orbit_x_max = orbits_info.oxmax;
	g_orbit_y_min = orbits_info.oymin;
	g_orbit_y_max = orbits_info.oymax;
	g_orbit_x_3rd = orbits_info.ox3rd;
	g_orbit_y_3rd = orbits_info.oy3rd;
	g_keep_screen_coords = (orbits_info.keep_scrn_coords != 0);
	g_orbit_draw_mode = int(orbits_info.drawmode);
	if (g_keep_screen_coords)
	{
		g_set_orbit_corners = true;
	}
}

static int fixup_3d_info(bool oldfloatflag, const fractal_info &read_info, formula_info_extension_block formula_info,
						 resume_info_extension_block resume_info_blk)
{
	// TODO: a klooge till the meaning of g_float_flag in line3d is clarified
	if (g_display_3d)
	{
		g_user_float_flag = oldfloatflag;
	}

	if (g_overlay_3d)
	{
		g_.SetInitialVideoMode(g_.Adapter());          // use previous adapter mode for overlays
		if (g_file_x_dots > g_x_dots || g_file_y_dots > g_y_dots)
		{
			stop_message(STOPMSG_NORMAL, "Can't overlay with a larger image");
			g_.SetInitialVideoModeNone();
			return -1;
		}
	}
	else
	{
		Display3DType old_display_3d = g_display_3d;
		bool oldfloatflag = g_float_flag;
		g_display_3d = g_loaded_3d ? DISPLAY3D_YES : DISPLAY3D_NONE;      // for <tab> display during next
		g_float_flag = (g_user_float_flag != 0); // ditto
		int i = get_video_mode(&read_info, &formula_info);
		g_display_3d = old_display_3d;
		g_float_flag = oldfloatflag;
		if (i)
		{
			if (resume_info_blk.got_data == 1)
			{
				delete[] resume_info_blk.resume_data;
				resume_info_blk.length = 0;
			}
			g_.SetInitialVideoModeNone();
			return -1;
		}
	}

	if (g_display_3d)
	{
		g_externs.SetCalculationStatus(CALCSTAT_PARAMS_CHANGED);
		g_fractal_type = FRACTYPE_PLASMA;
		g_current_fractal_specific = &g_fractal_specific[g_fractal_type];
		g_parameters[P1_REAL] = 0;
		if (!g_initialize_batch)
		{
			if (get_3d_parameters() < 0)
			{
				g_.SetInitialVideoModeNone();
				return -1;
			}
		}
	}
	return 0;
}

int read_overlay()      // read overlay/3D files, if reqr'd
{
	g_show_file = SHOWFILE_DONE;			// for any abort exit, pretend done
	g_.SetInitialVideoModeNone();					// no viewing mode set yet
	bool oldfloatflag = g_user_float_flag;
	g_loaded_3d = false;
	if (g_fast_restore)
	{
		g_viewWindow.Hide();
	}
	if (has_extension(g_read_name) == 0)
	{
		g_read_name += ".gif";
	}

	fractal_info read_info;
	resume_info_extension_block resume_info_blk;
	formula_info_extension_block formula_info;
	ranges_info_extension_block ranges_info;
	multiple_precision_info_extension_block mp_info;
	evolver_info_extension_block evolver_info;
	orbits_info_extension_block orbits_info;
	if (find_fractal_info(g_read_name.c_str(), &read_info, &resume_info_blk, &formula_info,
		&ranges_info, &mp_info, &evolver_info, &orbits_info))
	{
		// didn't find a useable file
		stop_message(STOPMSG_NORMAL, "Sorry, " + g_read_name + " isn't a file I can decode.");
		return -1;
	}

	g_max_iteration = read_info.iterationsold;
	g_fractal_type = read_info.fractal_type;
	if (g_fractal_type < 0 || g_fractal_type >= g_num_fractal_types)
	{
		stop_message(STOPMSG_INFO_ONLY, "Warning: " + g_read_name + " has a bad fractal type; using 0");
		g_fractal_type = 0;
	}
	g_current_fractal_specific = &g_fractal_specific[g_fractal_type];
	g_escape_time_state.m_grid_fp.x_min() = read_info.x_min;
	g_escape_time_state.m_grid_fp.x_max() = read_info.x_max;
	g_escape_time_state.m_grid_fp.y_min() = read_info.y_min;
	g_escape_time_state.m_grid_fp.y_max() = read_info.y_max;
	g_parameters[P1_REAL] = read_info.c_real;
	g_parameters[P1_IMAG] = read_info.c_imag;
	g_save_release = 1100; // unless we find out better later on

	read_info_version_0(read_info);
	read_info_version_1(read_info);
	read_info_version_2(read_info);
	read_info_version_3(read_info);
	read_info_version_4(read_info);
	read_info_version_5(read_info);
	read_info_version_6(read_info);
	read_info_version_7(read_info);
	read_info_version_8(read_info);
	read_info_pre_version_14(read_info);
	read_info_pre_version_15(read_info);
	read_info_pre_version_18(read_info);
	read_info_pre_version_17_25(read_info);
	read_info_version_9(read_info);
	read_info_version_10(read_info);
	read_info_version_11(read_info);
	read_info_version_12(read_info);
	read_info_version_13(read_info);
	read_info_version_14();
	read_info_version_15(read_info);
	read_info_version_16(read_info);
	backwards_v18();
	backwards_v19();
	backwards_v20();

	if (fixup_3d_info(oldfloatflag, read_info, formula_info, resume_info_blk))
	{
		return -1;
	}

	got_resume_info(resume_info_blk);
	got_formula_info(read_info, formula_info);
	got_ranges_info(ranges_info);
	got_multiple_precision_info(read_info, mp_info);
	got_evolver_info(read_info, evolver_info);
	got_orbits_info(orbits_info);

	g_show_file = SHOWFILE_PENDING;                   // trigger the file load

	return 0;
}

class GifFile
{
public:
	GifFile(char const *gif_file)
		: _file(gif_file, std::ios::in | std::ios::binary)
	{
	}
	bool Open() const
	{
		return _file.is_open();
	}
	size_t Read(void *destination, int size, int count)
	{
		_file.read(static_cast<char *>(destination), size*count);
		return _file ? size*count : 0;
	}
	int GetChar()
	{
		return _file.get();
	}
	void SeekFromEnd(long amount)
	{
		_file.seekg(amount, std::ios_base::end);
	}
	void SeekFromCurrent(long amount)
	{
		_file.seekg(amount, std::ios_base::cur);
	}

private:
	std::ifstream _file;
};

static GifFile *s_gif = 0;

static bool GifOpen(char const *gif_file)
{
	delete s_gif;
	s_gif = new GifFile(gif_file);
	return s_gif->Open();
}

static void GifClose()
{
	delete s_gif;
	s_gif = 0;
}

static void GifStartSetSizeAspect(const BYTE gifstart[18])
{
	g_file_x_dots = Get16(&gifstart[6]);
	g_file_y_dots = Get16(&gifstart[8]);
	g_file_colors = 2 << (gifstart[10] & 7);
	g_file_aspect_ratio = 0; // unknown
	if (gifstart[12])  // calc reasonably close value from gif header
	{
		g_file_aspect_ratio = float((64.0/(double(gifstart[12]) + 15.0))
			*double(g_file_y_dots)/double(g_file_x_dots));
		if (g_file_aspect_ratio > g_screen_aspect_ratio-0.03
			&& g_file_aspect_ratio < g_screen_aspect_ratio + 0.03)
		{
			g_file_aspect_ratio = g_screen_aspect_ratio;
		}
	}
	else if (g_file_y_dots*4 == g_file_x_dots*3) // assume the common square pixels
	{
		g_file_aspect_ratio = g_screen_aspect_ratio;
	}
}

static void GifFileReadColormap(const BYTE gifstart[18])
{
	if (!g_make_par_flag && (gifstart[10] & 0x80) != 0)
	{
		for (int i = 0; i < g_file_colors; i++)
		{
			int k = 0;
			for (int j = 0; j < 3; j++)
			{
				k = s_gif->GetChar();
				if (k < 0)
				{
					break;
				}
				// TODO: does not work when COLOR_CHANNEL_MAX != 63
				g_.DAC().SetChannel(i, j, BYTE(k >> 2));
			}
			if (k < 0)
			{
				break;
			}
		}
	}
}

static void FoundInfoId(fractal_info *info,
						resume_info_extension_block *resume_info,
						formula_info_extension_block *formula_info,
						ranges_info_extension_block *ranges_info,
						multiple_precision_info_extension_block *mp_info,
						evolver_info_extension_block *evolver_info,
						orbits_info_extension_block *orbits_info,
						int hdr_offset)
{
	// TODO: handle old crap or abort?
	if (info->version >= 4)
	{
		/* first reload main extension g_block, reasons:
		might be over 255 chars, and thus earlier load might be bad
		find exact endpoint, so scan back to start of ext blks works
		*/
		s_gif->SeekFromEnd(hdr_offset-15);
		int scan_extend = 1;
		while (scan_extend)
		{
			char temp1[81];
			if (s_gif->GetChar() != '!' // if not what we expect just give up
				|| s_gif->Read(temp1, 1, 13) != 13
				|| strncmp(&temp1[2], "fractint", 8))
			{
				break;
			}
			temp1[13] = 0;
			int block_type = atoi(&temp1[10]); // e.g. "fractint002"
			int block_len;
			int data_len;
			switch (block_type)
			{
			case BLOCKTYPE_MAIN_INFO: // "fractint001", the main extension g_block
				if (scan_extend == 2)  // we've been here before, done now
				{
					scan_extend = 0;
					break;
				}
				load_ext_blk(reinterpret_cast<char *>(info), FRACTAL_INFO_SIZE);
#ifdef XFRACT
				decode_fractal_info(info, 1);
#endif
				scan_extend = 2;
				// now we know total extension len, back up to first g_block
				s_gif->SeekFromCurrent(-info->tot_extend_len);
				break;
			case BLOCKTYPE_RESUME_INFO: // resume info
				skip_ext_blk(&block_len, &data_len); // once to get lengths
				resume_info->resume_data = new char[data_len];
				if (resume_info->resume_data == 0)
				{
					info->calculation_status = CALCSTAT_NON_RESUMABLE; // not resumable after all
				}
				else
				{
					s_gif->SeekFromCurrent(long(-block_len));
					load_ext_blk(resume_info->resume_data, data_len);
					resume_info->length = data_len;
					resume_info->got_data = 1; // got data
				}
				break;
			case BLOCKTYPE_FORMULA_INFO: // formula info
				skip_ext_blk(&block_len, &data_len); // once to get lengths
				// check data_len for backward compatibility
				s_gif->SeekFromCurrent(long(-block_len));
				struct formula_info formula_load_info;
				load_ext_blk(reinterpret_cast<char *>(&formula_load_info), data_len);
				strcpy(formula_info->form_name, formula_load_info.form_name);
				formula_info->length = data_len;
				formula_info->got_data = 1; // got data
				if (data_len < sizeof(formula_load_info))  // must be old GIF
				{
					formula_info->uses_p1 = 1;
					formula_info->uses_p2 = 1;
					formula_info->uses_p3 = 1;
					formula_info->uses_is_mand = 0;
					formula_info->ismand = 1;
					formula_info->uses_p4 = 0;
					formula_info->uses_p5 = 0;
				}
				else
				{
					formula_info->uses_p1 = formula_load_info.uses_p1;
					formula_info->uses_p2 = formula_load_info.uses_p2;
					formula_info->uses_p3 = formula_load_info.uses_p3;
					formula_info->uses_is_mand = formula_load_info.uses_is_mand;
					formula_info->ismand = formula_load_info.ismand;
					formula_info->uses_p4 = formula_load_info.uses_p4;
					formula_info->uses_p5 = formula_load_info.uses_p5;
				}
				break;
			case BLOCKTYPE_RANGES_INFO: // ranges info
				skip_ext_blk(&block_len, &data_len); // once to get lengths
				ranges_info->range_data = new short[data_len/sizeof(short)];
				if (ranges_info->range_data != 0)
				{
					s_gif->SeekFromCurrent(long(-block_len));
					load_ext_blk(reinterpret_cast<char *>(ranges_info->range_data), data_len);
					ranges_info->length = data_len/sizeof(short);
					ranges_info->got_data = 1; // got data
				}
				break;
			case BLOCKTYPE_MP_INFO: // extended precision parameters
				skip_ext_blk(&block_len, &data_len); // once to get lengths
				mp_info->apm_data = new char[data_len];
				if (mp_info->apm_data != 0)
				{
					s_gif->SeekFromCurrent(long(-block_len));
					load_ext_blk(mp_info->apm_data, data_len);
					mp_info->length = data_len;
					mp_info->got_data = 1; // got data
				}
				break;
			case BLOCKTYPE_EVOLVER_INFO: // evolver params
				skip_ext_blk(&block_len, &data_len); // once to get lengths
				s_gif->SeekFromCurrent(long(-block_len));
				evolution_info evolver_load_info;
				load_ext_blk(reinterpret_cast<char *>(&evolver_load_info), data_len);
				// XFRACT processing of doubles here
#ifdef XFRACT
				decode_evolver_info(&eload_info, 1);
#endif
				evolver_info->length = data_len;
				evolver_info->got_data = 1; // got data

				evolver_info->parameter_range_x = evolver_load_info.parameter_range_x;
				evolver_info->parameter_range_y = evolver_load_info.parameter_range_y;
				evolver_info->opx = evolver_load_info.opx;
				evolver_info->opy = evolver_load_info.opy;
				evolver_info->odpx = (char)evolver_load_info.odpx;
				evolver_info->odpy = (char)evolver_load_info.odpy;
				evolver_info->px = evolver_load_info.px;
				evolver_info->py = evolver_load_info.py;
				evolver_info->sxoffs = evolver_load_info.screen_x_offset;
				evolver_info->syoffs = evolver_load_info.screen_y_offset;
				evolver_info->x_dots = evolver_load_info.x_dots;
				evolver_info->y_dots = evolver_load_info.y_dots;
				evolver_info->grid_size = evolver_load_info.grid_size;
				evolver_info->evolving = evolver_load_info.evolving;
				evolver_info->this_generation_random_seed = evolver_load_info.this_generation_random_seed;
				evolver_info->fiddle_factor = evolver_load_info.fiddle_factor;
				evolver_info->ecount = evolver_load_info.ecount;
				for (int i = 0; i < NUM_GENES; i++)
				{
					evolver_info->mutate[i] = evolver_load_info.mutate[i];
				}
				break;
			case BLOCKTYPE_ORBITS_INFO: // orbits parameters
				skip_ext_blk(&block_len, &data_len); // once to get lengths
				s_gif->SeekFromCurrent(long(-block_len));
				struct orbits_info orbits_load_info;
				load_ext_blk(reinterpret_cast<char *>(&orbits_load_info), data_len);
				// XFRACT processing of doubles here
#ifdef XFRACT
				decode_orbits_info(&oload_info, 1);
#endif
				orbits_info->length = data_len;
				orbits_info->got_data = 1; // got data
				orbits_info->oxmin = orbits_load_info.oxmin;
				orbits_info->oxmax = orbits_load_info.oxmax;
				orbits_info->oymin = orbits_load_info.oymin;
				orbits_info->oymax = orbits_load_info.oymax;
				orbits_info->ox3rd = orbits_load_info.ox3rd;
				orbits_info->oy3rd = orbits_load_info.oy3rd;
				orbits_info->keep_scrn_coords = orbits_load_info.keep_scrn_coords;
				orbits_info->drawmode = orbits_load_info.drawmode;
				break;
			default:
				skip_ext_blk(&block_len, &data_len);
			}
		}
	}

	GifClose();
	g_file_aspect_ratio = g_screen_aspect_ratio; // if not >= v15, this is correct
}

int find_fractal_info(const char *gif_file, fractal_info *info,
					  resume_info_extension_block *resume_info,
					  formula_info_extension_block *formula_info,
					  ranges_info_extension_block *ranges_info,
					  multiple_precision_info_extension_block *mp_info,
					  evolver_info_extension_block *evolver_info,
					  orbits_info_extension_block *orbits_info)
{
	// initialize to no data
	resume_info->got_data = 0;
	formula_info->got_data = 0;
	ranges_info->got_data = 0;
	mp_info->got_data = 0;
	evolver_info->got_data = 0;
	orbits_info->got_data = 0;

	if (!GifOpen(gif_file))
	{
		return -1;
	}

	BYTE gifstart[18];
	s_gif->Read(gifstart, 1, 13);
	if (strncmp((char *)gifstart, "GIF", 3) != 0)  // not GIF, maybe old .tga?
	{
		GifClose();
		return -1;
	}

	GifStartSetSizeAspect(gifstart);
	GifFileReadColormap(gifstart);

	/* Format of .gif extension blocks is:
	1 byte    '!', extension g_block identifier
	1 byte    extension g_block number, 255
	1 byte    length of id, 11
	11 bytes   alpha id, "fractintnnn" with fractint, nnn is secondary id
	n * {
	1 byte    length of g_block info in bytes
	x bytes   g_block info
	}
	1 byte    0, extension terminator
	To scan extension blocks, we first look in file at length of fractal_info
	(the main extension g_block) from end of file, looking for a literal known
	to be at start of our g_block info.  Then we scan forward a bit, in case
	the file is from an earlier fractint vsn with shorter fractal_info.
	If fractal_info is found and is from vsn >= 14, it includes the total length
	of all extension blocks; we then scan them all first to last to load
	any optional ones which are present.
	Defined extension blocks:
	fractint001     header, always present
	fractint002     resume info for interrupted resumable image
	fractint003     additional formula type info
	fractint004     ranges info
	fractint005     extended precision parameters
	fractint006     evolver params
	*/
	memset(info, 0, FRACTAL_INFO_SIZE);
	int fractinf_len = FRACTAL_INFO_SIZE + (FRACTAL_INFO_SIZE + 254)/255;
	s_gif->SeekFromEnd(-1-fractinf_len);
	/* TODO: revise this to read members one at a time so we get natural alignment
	of fields within the fractal_info structure for the platform */
	s_gif->Read(info, 1, FRACTAL_INFO_SIZE);
	int hdr_offset;
	if (strcmp(INFO_ID, info->info_id) == 0)
	{
#ifdef XFRACT
		decode_fractal_info(info, 1);
#endif
		hdr_offset = -1-fractinf_len;
	}
	else
	{
		// didn't work 1st try, maybe an older vsn, maybe junk at eof, scan:
		int offset;
		char tmpbuf[110];
		hdr_offset = 0;
		offset = 80; // don't even check last 80 bytes of file for id
		while (offset < fractinf_len + 513)  // allow 512 garbage at eof
		{
			offset += 100; // go back 100 bytes at a time
			s_gif->SeekFromEnd(-offset);
			s_gif->Read(tmpbuf, 1, 110); // read 10 extra for string compare
			for (int i = 0; i < 100; ++i)
			{
				if (!strcmp(INFO_ID, &tmpbuf[i]))  // found header?
				{
					strcpy(info->info_id, INFO_ID);
					hdr_offset = i-offset;
					s_gif->SeekFromEnd(hdr_offset);
					/* TODO: revise this to read members one at a time so we get natural alignment
					of fields within the fractal_info structure for the platform */
					s_gif->Read(info, 1, FRACTAL_INFO_SIZE);
#ifdef XFRACT
					decode_fractal_info(info, 1);
#endif
					offset = 10000; // force exit from outer loop
					break;
				}
			}
		}
	}

	if (hdr_offset)  // we found INFO_ID
	{
		FoundInfoId(info, resume_info, formula_info, ranges_info, mp_info, evolver_info, orbits_info, hdr_offset);
		return 0;
	}

	strcpy(info->info_id, "GIFFILE");
	info->iterations = 150;
	info->iterationsold = 150;
	info->fractal_type = FRACTYPE_PLASMA;
	info->x_min = -1;
	info->x_max = 1;
	info->y_min = -1;
	info->y_max = 1;
	info->x_3rd = -1;
	info->y_3rd = -1;
	info->c_real = 0;
	info->c_imag = 0;
	info->deprecated_video_mode_ax = 0;
	info->deprecated_video_mode_bx = 0;
	info->deprecated_video_mode_cx = 0;
	info->deprecated_video_mode_dx = 0;
	info->deprecated_dotmode = 0;
	info->x_dots = short(g_file_x_dots);
	info->y_dots = short(g_file_y_dots);
	info->colors = short(g_file_colors);
	info->version = 0; // this forces lots more init at calling end too

	// zero means we won
	GifClose();
	return 0;
}

static void load_ext_blk(char *loadptr, int loadlen)
{
	for (int len = s_gif->GetChar(); len > 0; len = s_gif->GetChar())
	{
		while (--len >= 0)
		{
			char data = char(s_gif->GetChar());
			// discard excess characters
			if (--loadlen >= 0)
			{
				*(loadptr++) = data;
			}
		}
	}
}

static void skip_ext_blk(int *block_len, int *data_len)
{
	int len;
	*data_len = 0;
	*block_len = 1;
	while ((len = s_gif->GetChar()) > 0)
	{
		s_gif->SeekFromCurrent(long(len));
		*data_len += len;
		*block_len += len + 1;
	}
}


// switch obsolete fractal types to new generalizations
static void translate_obsolete_fractal_types(const fractal_info *info)
{
	switch (g_fractal_type)
	{
	case FRACTYPE_OBSOLETE_LAMBDA_SINE:
		g_fractal_type = FRACTYPE_LAMBDA_FUNC_FP;
		g_function_index[0] = FUNCTION_SIN;
		break;
	case FRACTYPE_OBSOLETE_LAMBDA_COS:
		g_fractal_type = FRACTYPE_LAMBDA_FUNC_FP;
		g_function_index[0] = FUNCTION_COSXX;
		break;
	case FRACTYPE_OBSOLETE_LAMBDA_EXP:
		g_fractal_type = FRACTYPE_LAMBDA_FUNC_FP;
		g_function_index[0] = FUNCTION_EXP;
		break;
	case FRACTYPE_OBSOLETE_MANDELBROT_SINE:
		g_fractal_type = FRACTYPE_MANDELBROT_FUNC_FP;
		g_function_index[0] = FUNCTION_SIN;
		break;
	case FRACTYPE_OBSOLETE_MANDELBROT_COS:
		g_fractal_type = FRACTYPE_MANDELBROT_FUNC_FP;
		g_function_index[0] = FUNCTION_COSXX;
		break;
	case FRACTYPE_OBSOLETE_MANDELBROT_EXP:
		g_fractal_type = FRACTYPE_MANDELBROT_FUNC_FP;
		g_function_index[0] = FUNCTION_EXP;
		break;
	case FRACTYPE_OBSOLETE_MANDELBROT_SINH:
		g_fractal_type = FRACTYPE_MANDELBROT_FUNC_FP;
		g_function_index[0] = FUNCTION_SINH;
		break;
	case FRACTYPE_OBSOLETE_LAMBDA_SINH:
		g_fractal_type = FRACTYPE_LAMBDA_FUNC_FP;
		g_function_index[0] = FUNCTION_SINH;
		break;
	case FRACTYPE_OBSOLETE_MANDELBROT_COSH:
		g_fractal_type = FRACTYPE_MANDELBROT_FUNC_FP;
		g_function_index[0] = FUNCTION_COSH;
		break;
	case FRACTYPE_OBSOLETE_LAMBDA_COSH:
		g_fractal_type = FRACTYPE_LAMBDA_FUNC_FP;
		g_function_index[0] = FUNCTION_COSH;
		break;
	case FRACTYPE_OBSOLETE_MANDELBROT_SINE_L:
		g_fractal_type = FRACTYPE_MANDELBROT_FUNC;
		g_function_index[0] = FUNCTION_SIN;
		break;
	case FRACTYPE_OBSOLETE_LAMBDA_SINE_L:
		g_fractal_type = FRACTYPE_LAMBDA_FUNC;
		g_function_index[0] = FUNCTION_SIN;
		break;
	case FRACTYPE_OBSOLETE_MANDELBROT_COS_L:
		g_fractal_type = FRACTYPE_MANDELBROT_FUNC;
		g_function_index[0] = FUNCTION_COSXX;
		break;
	case FRACTYPE_OBSOLETE_LAMBDA_COS_L:
		g_fractal_type = FRACTYPE_LAMBDA_FUNC;
		g_function_index[0] = FUNCTION_COSXX;
		break;
	case FRACTYPE_OBSOLETE_MANDELBROT_SINH_L:
		g_fractal_type = FRACTYPE_MANDELBROT_FUNC;
		g_function_index[0] = FUNCTION_SINH;
		break;
	case FRACTYPE_OBSOLETE_LAMBDA_SINH_L:
		g_fractal_type = FRACTYPE_LAMBDA_FUNC;
		g_function_index[0] = FUNCTION_SINH;
		break;
	case FRACTYPE_OBSOLETE_MANDELBROT_COSH_L:
		g_fractal_type = FRACTYPE_MANDELBROT_FUNC;
		g_function_index[0] = FUNCTION_COSH;
		break;
	case FRACTYPE_OBSOLETE_LAMBDA_COSH_L:
		g_fractal_type = FRACTYPE_LAMBDA_FUNC;
		g_function_index[0] = FUNCTION_COSH;
		break;
	case FRACTYPE_OBSOLETE_MANDELBROT_EXP_L:
		g_fractal_type = FRACTYPE_MANDELBROT_FUNC;
		g_function_index[0] = FUNCTION_EXP;
		break;
	case FRACTYPE_OBSOLETE_LAMBDA_EXP_L:
		g_fractal_type = FRACTYPE_LAMBDA_FUNC;
		g_function_index[0] = FUNCTION_EXP;
		break;
	case FRACTYPE_OBSOLETE_DEM_MANDELBROT:
		g_fractal_type = FRACTYPE_MANDELBROT_FP;
		g_user_distance_test = (info->y_dots - 1)*2;
		break;
	case FRACTYPE_OBSOLETE_DEM_JULIA:
		g_fractal_type = FRACTYPE_JULIA_FP;
		g_user_distance_test = (info->y_dots - 1)*2;
		break;
	case FRACTYPE_MANDELBROT_LAMBDA:
		g_externs.SetUseInitialOrbitZ(INITIALZ_PIXEL);
		break;
	}
	g_current_fractal_specific = &g_fractal_specific[g_fractal_type];
}

// switch old bifurcation fractal types to new generalizations
void set_if_old_bif()
{
	/* set functions if not set already, may need to check 'g_function_preloaded'
		before calling this routine.  */
	switch (g_fractal_type)
	{
	case FRACTYPE_BIFURCATION:
	case FRACTYPE_BIFURCATION_L:
	case FRACTYPE_BIFURCATION_STEWART:
	case FRACTYPE_BIFURCATION_STEWART_L:
	case FRACTYPE_BIFURCATION_LAMBDA:
	case FRACTYPE_BIFURCATION_LAMBDA_L:
		set_function_array(0, "ident");
		break;

	case FRACTYPE_BIFURCATION_EQUAL_FUNC_PI:
	case FRACTYPE_BIFURCATION_EQUAL_FUNC_PI_L:
	case FRACTYPE_BIFURCATION_PLUS_FUNC_PI:
	case FRACTYPE_BIFURCATION_PLUS_FUNC_PI_L:
		set_function_array(0, "sin");
		break;
	}
}

// miscellaneous function variable defaults
void set_function_parm_defaults()
{
	switch (g_fractal_type)
	{
	case FRACTYPE_POPCORN_FP:
	case FRACTYPE_POPCORN_L:
	case FRACTYPE_POPCORN_JULIA_FP:
	case FRACTYPE_POPCORN_JULIA_L:
		set_function_array(0, "sin");
		set_function_array(1, "tan");
		set_function_array(2, "sin");
		set_function_array(3, "tan");
		break;
	case FRACTYPE_LATOOCARFIAN:
		set_function_array(0, "sin");
		set_function_array(1, "sin");
		set_function_array(2, "sin");
		set_function_array(3, "sin");
		break;
	}
}

void backwards_v18()
{
	if (!g_function_preloaded)
	{
		set_if_old_bif(); // old bifurcations need function set
	}
	if (g_user_float_flag
		&& (g_save_release < 1800)
		&& (g_externs.BailOut() == 0)
		&& ((g_fractal_type == FRACTYPE_MANDELBROT_FUNC) || (g_fractal_type == FRACTYPE_LAMBDA_FUNC)))
	{
		g_externs.SetBailOut(2500);
	}
}

void backwards_v19()
{
	if (g_fractal_type == FRACTYPE_MARKS_JULIA && g_save_release < 1825)
	{
		if (g_parameters[P2_REAL] == 0)
		{
			g_parameters[P2_REAL] = 2;
		}
		else
		{
			g_parameters[P2_REAL]++;
		}
	}
	if (g_fractal_type == FRACTYPE_MARKS_JULIA_FP && g_save_release < 1825)
	{
		if (g_parameters[P2_REAL] == 0)
		{
			g_parameters[P2_REAL] = 2;
		}
		else
		{
			g_parameters[P2_REAL]++;
		}
	}
	if (fractal_type_formula(g_fractal_type) && g_save_release < 1824)
	{
		g_inversion[0] = 0;
		g_inversion[1] = 0;
		g_inversion[2] = 0;
		g_invert = 0;
	}
	g_no_magnitude_calculation = fix_bof(); // fractal has old bof60/61 problem with magnitude
	g_use_old_periodicity = fix_period_bof(); // fractal uses old periodicity method
}

void backwards_v20()
{
	// Fractype == FP type is not seen from PAR file ?????
	// TODO: BadOutside is a compatability flag with buggy old code,
	// TODO: but the current code doesn't emulate the buggy behavior.
	// TODO: See calmanfp.asm and calmanfp5.asm in the DOS code.
	g_externs.SetBadOutside((fractal_type_mandelbrot(g_fractal_type) || fractal_type_julia(g_fractal_type))
		&& g_externs.Outside() <= COLORMODE_REAL
		&& g_externs.Outside() >= COLORMODE_SUM
		&& g_save_release <= 1960);
	g_use_old_complex_power = (fractal_type_formula(g_fractal_type)
		&& (g_save_release < 1900 || DEBUGMODE_OLD_POWER == g_debug_mode));
	if (g_externs.Inside() == COLORMODE_EPSILON_CROSS && g_save_release < 1961)
	{
		g_proximity = 0.01;
	}
	if (!g_function_preloaded)
	{
		set_function_parm_defaults();
	}
}

bool check_back()
{
	/*
		put the features that need to save the value in g_save_release for backwards
		compatibility in this routine
	*/
	return (g_fractal_type == FRACTYPE_LYAPUNOV
		|| g_fractal_type == FRACTYPE_FROTHY_BASIN
		|| g_fractal_type == FRACTYPE_FROTHY_BASIN_FP
		|| fix_bof()
		|| fix_period_bof()
		|| g_decomposition[0] == 2
		|| (fractal_type_formula(g_fractal_type) && g_save_release <= 1920)
		|| (g_log_palette_mode != 0 && g_save_release <= 2001)
		|| (g_fractal_type == FRACTYPE_FUNC_SQR && g_save_release < 1900)
		|| (g_externs.Inside() == COLORMODE_STAR_TRAIL && g_save_release < 1825)
		|| (g_max_iteration > 32767 && g_save_release <= 1950)
		|| (g_distance_test && g_save_release <= 1950)
		|| ((g_externs.Outside() <= COLORMODE_REAL && g_externs.Outside() >= COLORMODE_INVERSE_TANGENT) && g_save_release <= 1960)
		|| (g_fractal_type == FRACTYPE_POPCORN_FP && g_save_release <= 1960)
		|| (g_fractal_type == FRACTYPE_POPCORN_L && g_save_release <= 1960)
		|| (g_fractal_type == FRACTYPE_POPCORN_JULIA_FP && g_save_release <= 1960)
		|| (g_fractal_type == FRACTYPE_POPCORN_JULIA_L && g_save_release <= 1960)
		|| (g_externs.Inside() == COLORMODE_FLOAT_MODULUS_INTEGER && g_save_release <= 2000)
		|| ((g_externs.Inside() == COLORMODE_INVERSE_TANGENT_INTEGER || g_externs.Outside() == COLORMODE_INVERSE_TANGENT) && g_save_release <= 2005)
		|| (g_fractal_type == FRACTYPE_LAMBDA_FUNC_FP && g_function_index[0] == FUNCTION_EXP && g_save_release <= 2002)
		|| (fractal_type_julibrot(g_fractal_type)
			&& (g_new_orbit_type == FRACTYPE_QUATERNION_FP || g_new_orbit_type == FRACTYPE_HYPERCOMPLEX_FP)
			&& g_save_release <= 2002));
}

static bool fix_period_bof()
{
	// TODO: leftover from old save_release checking?
	return false;
}

static bool fix_bof()
{
	if (fix_period_bof())
	{
		if ((g_current_fractal_specific->calculate_type == standard_fractal
				&& (g_current_fractal_specific->flags & FRACTALFLAG_BAIL_OUT_TESTS) == 0)
			|| fractal_type_formula(g_fractal_type))
		{
			return true;
		}
	}
	return false;
}