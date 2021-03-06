/*
        FRACTINT - The Ultimate Fractal Generator
                        Main Routine
*/
#include <cassert>
#include <vector>

#include <ctype.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#if defined(XFRACT)
#include <unistd.h>
#else
#include <io.h>
#endif

#include "port.h"
#include "prototyp.h"
#include "fractype.h"
#include "helpdefs.h"
#include "drivers.h"
#include "helpcom.h"

VIDEOINFO g_video_entry;
int help_mode;

int look_at_mouse = 0;  // see notes at mouseread routine

long timer_start, timer_interval;       // timer(...) start & total
int     g_adapter;                      // Video Adapter chosen from list in ...h
char const *fract_dir1 = "";
char const *fract_dir2 = "";

/*
   the following variables are out here only so
   that the calcfract() and assembler routines can get at them easily
*/
int     dotmode;                // video access method
int     textsafe2;              // textsafe override from g_video_table
int     sxdots, sydots;         // # of dots on the physical screen
int     sxoffs, syoffs;         // physical top left of logical screen
int     xdots, ydots;           // # of dots on the logical screen
double  x_size_d, y_size_d;         // xdots-1, ydots-1
int     colors = 256;           // maximum colors available
long    maxit;                  // try this many iterations
int     boxcount;               // 0 if no zoom-box yet
int     zoom_box_rotation;                // zoombox rotation
double  zbx, zby;               // topleft of zoombox
double  zoom_box_width, zoom_box_height, zoom_box_skew;  // zoombox size & shape

fractal_type fractype;               // if == 0, use Mandelbrot
char    stdcalcmode;            // '1', '2', 'g', 'b'
long    delx, dely;             // screen pixel increments
long    delx2, dely2;           // screen pixel increments
LDBL    delxx, delyy;           // screen pixel increments
LDBL    delxx2, delyy2;         // screen pixel increments
long    delmin;                 // for calcfrac/calcmand
double  ddelmin;                // same as a double
double  param[MAXPARAMS];       // parameters
double  potparam[3];            // three potential parameters
long    fudge;                  // 2**fudgefactor
long    l_at_rad;               // finite attractor radius
double  f_at_rad;               // finite attractor radius
int     bitshift;               // fudgefactor

int     g_bad_config = 0;          // 'fractint.cfg' ok?
bool hasinverse = false;
// note that integer grid is set when integerfractal && !invert;
// otherwise the floating point grid is set; never both at once
std::vector<long> lx0;              // x, y grid
std::vector<long> ly0;
std::vector<long> lx1;              // adjustment for rotate
std::vector<long> ly1;
// note that lx1 & ly1 values can overflow into sign bit; since
// they're used only to add to lx0/ly0, 2s comp straightens it out
std::vector<double> dx0;            // floating pt equivs
std::vector<double> dy0;
std::vector<double> dx1;
std::vector<double> dy1;
int     integerfractal;         // TRUE if fractal uses integer math

// usr_xxx is what the user wants, vs what we may be forced to do
char    usr_stdcalcmode;
int     usr_periodicitycheck;
long    usr_distest;
bool    usr_floatflag;

bool    viewwindow = false;     // false for full screen, true for window
float   viewreduction;          // window auto-sizing
bool    viewcrop = false;       // true to crop default coords
float   finalaspectratio;       // for view shape and rotation
int     viewxdots, viewydots;    // explicit view sizing
bool    video_cutboth = false;  // true to keep virtual aspect
bool    zscroll = false;        // screen/zoombox false fixed, true relaxed

// variables defined by the command line/files processor
bool    comparegif = false;             // compare two gif files flag
int     timedsave = 0;                    // when doing a timed save
int     resave_flag = 0;                  // tells encoder not to incr filename
bool    started_resaves = false;        // but incr on first resave
int     save_system;                    // from and for save files
bool    tab_mode = true;                 // tab display enabled

// for historical reasons (before rotation):
//    top    left  corner of screen is (xxmin,yymax)
//    bottom left  corner of screen is (xx3rd,yy3rd)
//    bottom right corner of screen is (xxmax,yymin)
double  xxmin, xxmax, yymin, yymax, xx3rd, yy3rd; // selected screen corners
long    xmin, xmax, ymin, ymax, x3rd, y3rd;  // integer equivs
double  sxmin, sxmax, symin, symax, sx3rd, sy3rd; // displayed screen corners
double  plotmx1, plotmx2, plotmy1, plotmy2;     // real->screen multipliers

calc_status_value calc_status = calc_status_value::NO_FRACTAL;
// -1 no fractal
//  0 parms changed, recalc reqd
//  1 actively calculating
//  2 interrupted, resumable
//  3 interrupted, not resumable
//  4 completed
long calctime;

int max_colors;                         // maximum palette size
bool zoomoff = false;                   // false when zoom is disabled
int        savedac;                     // save-the-Video DAC flag
bool browsing = false;                  // browse mode flag
std::string file_name_stack[16];        // array of file names used while browsing
int name_stack_ptr ;
double smallest_window_display_size;
int smallest_box_size_shown;
bool no_sub_images = false;
bool auto_browse = false;
bool confirm_file_deletes = false;
bool browse_check_fractal_params = false;
bool browse_check_fractal_type = false;
std::string browse_mask;
int scale_map[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}; // array for mapping notes to a (user defined) scale


void check_samename()
{
    char drive[FILE_MAX_DRIVE];
    char dir[FILE_MAX_DIR];
    char fname[FILE_MAX_FNAME];
    char ext[FILE_MAX_EXT];
    char path[FILE_MAX_PATH];
    splitpath(savename.c_str(), drive, dir, fname, ext);
    if (strcmp(fname, "fract001"))
    {
        makepath(path, drive, dir, fname, "gif");
        if (access(path, 0) == 0)
            exit(0);
    }
}

// Do nothing if math error
static void my_floating_point_err(int sig)
{
    if (sig != 0)
        overflow = true;
}

/*
; ****************** Function initasmvars() *****************************
*/
void initasmvars()
{
    overflow = false;
}

static void main_restart(int const argc, char const *const argv[], bool &stacked)
{
#if defined(_WIN32)
    _ASSERTE(_CrtCheckMemory());
#endif
    auto_browse = false;
    browse_check_fractal_type = false;
    browse_check_fractal_params = true;
    confirm_file_deletes = true;
    no_sub_images = false;
    smallest_window_display_size = 6;
    smallest_box_size_shown = 3;
    browse_mask = "*.gif";
    browse_name = "            ";
    name_stack_ptr = -1; // init loaded files stack

    evolving = 0;
    evolve_x_parameter_range = 4;
    evolve_new_x_parameter_offset = -2.0;
    evolve_x_parameter_offset = -2.0;
    evolve_y_parameter_range = 3;
    evolve_new_y_parameter_offset = -1.5;
    evolve_y_parameter_offset = -1.5;
    evolve_discrete_y_parameter_offset = 0;
    evolve_discrete_x_parameter_offset = 0;
    evolve_image_grid_size = 9;
    evolve_max_random_mutation = 1;
    evolve_mutation_reduction_factor = 1.0;
    evolve_this_generation_random_seed = (unsigned int)clock_ticks();
    srand(evolve_this_generation_random_seed);
    initgene(); /*initialise pointers to lots of fractint variables for the evolution engine*/
    start_show_orbit = false;
    show_dot = -1; // turn off show_dot if entered with <g> command
    calc_status = calc_status_value::NO_FRACTAL;                    // no active fractal image

    fract_dir1 = getenv("FRACTDIR");
    if (fract_dir1 == nullptr)
    {
        fract_dir1 = ".";
    }
#ifdef SRCDIR
    fract_dir2 = SRCDIR;
#else
    fract_dir2 = ".";
#endif

    cmdfiles(argc, argv);         // process the command-line
    dopause(0);                  // pause for error msg if not batch
    init_msg("", nullptr, cmd_file::AT_CMD_LINE);  // this causes driver_get_key if init_msg called on runup

    history_init();

    if (debugflag == debug_flags::prevent_overwrite_savename && init_batch == batch_modes::NORMAL)   // abort if savename already exists
    {
        check_samename();
    }
    driver_window();
    memcpy(old_dac_box, g_dac_box, 256*3);      // save in case colors= present

    driver_set_for_text();                      // switch to text mode
    savedac = 0;                         // don't save the VGA DAC

#ifndef XFRACT
    if (g_bad_config < 0)                   // fractint.cfg bad, no msg yet
    {
        bad_fractint_cfg_msg();
    }
#endif

    max_colors = 256;                   // the Windows version is lower
    max_keyboard_check_interval = 80;                  // check the keyboard this often

    if (show_file && g_init_mode < 0)
    {
        intro();                          // display the credits screen
        if (driver_key_pressed() == FIK_ESC)
        {
            driver_get_key();
            goodbye();
        }
    }

    browsing = false;

    if (!new_bifurcation_functions_loaded)
    {
        set_if_old_bif();
    }
    stacked = false;
}

static bool main_restore_start(bool &stacked, bool &resumeflag)
{
#if defined(_WIN32)
    _ASSERTE(_CrtCheckMemory());
#endif

    if (colors_preloaded)
    {
        memcpy(g_dac_box, old_dac_box, 256*3);   // restore in case colors= present
    }

    look_at_mouse = 0;                     // ignore mouse

    while (show_file <= 0)              // image is to be loaded
    {
        char const *hdg;
        tab_mode = false;
        if (!browsing)      /*RB*/
        {
            if (overlay_3d)
            {
                hdg = "Select File for 3D Overlay";
                help_mode = HELP3DOVLY;
            }
            else if (display_3d != display_3d_modes::NONE)
            {
                hdg = "Select File for 3D Transform";
                help_mode = HELP3D;
            }
            else
            {
                hdg = "Select File to Restore";
                help_mode = HELPSAVEREST;
            }
            if (show_file < 0 && getafilename(hdg, gifmask.c_str(), readname))
            {
                show_file = 1;               // cancelled
                g_init_mode = -1;
                break;
            }

            name_stack_ptr = 0; // 'r' reads first filename for browsing
            file_name_stack[name_stack_ptr] = browse_name;
        }

        evolving = 0;
        viewwindow = false;
        show_file = 0;
        help_mode = -1;
        tab_mode = true;
        if (stacked)
        {
            driver_discard_screen();
            driver_set_for_text();
            stacked = false;
        }
        if (read_overlay() == 0)       // read hdr, get video mode
        {
            break;                      // got it, exit
        }
        if (browsing) // break out of infinite loop, but lose your mind
        {
            show_file = 1;
        }
        else
        {
            show_file = -1;                 // retry
        }
    }

    help_mode = HELPMENU;                 // now use this help mode
    tab_mode = true;
    look_at_mouse = 0;                     // ignore mouse

    if (((overlay_3d && (init_batch == batch_modes::NONE)) || stacked) && g_init_mode < 0)        // overlay command failed
    {
        driver_unstack_screen();                  // restore the graphics screen
        stacked = false;
        overlay_3d = false;              // forget overlays
        display_3d = display_3d_modes::NONE;
        if (calc_status == calc_status_value::NON_RESUMABLE)
            calc_status = calc_status_value::PARAMS_CHANGED;
        resumeflag = true;
        return true;
    }

    savedac = 0;                         // don't save the VGA DAC

    return false;
}

static main_state main_image_start(bool &stacked, bool &resumeflag)
{
#if defined(_WIN32)
    _ASSERTE(_CrtCheckMemory());
#endif

    if (stacked)
    {
        driver_discard_screen();
        stacked = false;
    }
#ifdef XFRACT
    usr_floatflag = true;
#endif
    got_status = -1;                     // for tab_display

    if (show_file)
        if (calc_status > calc_status_value::PARAMS_CHANGED)              // goto imagestart implies re-calc
            calc_status = calc_status_value::PARAMS_CHANGED;

    if (init_batch == batch_modes::NONE)
        look_at_mouse = -FIK_PAGE_UP;           // just mouse left button, == pgup

    cyclelimit = initcyclelimit;         // default cycle limit
    g_adapter = g_init_mode;                  // set the video adapter up
    g_init_mode = -1;                       // (once)

    while (g_adapter < 0)                // cycle through instructions
    {
        if (init_batch != batch_modes::NONE)                          // batch, nothing to do
        {
            init_batch = batch_modes::BAILOUT_INTERRUPTED_TRY_SAVE;                 // exit with error condition set
            goodbye();
        }
        int kbdchar = main_menu(0);
        if (kbdchar == FIK_INSERT)
            return main_state::RESTART;      // restart pgm on Insert Key
        if (kbdchar == FIK_DELETE)                    // select video mode list
            kbdchar = select_video_mode(-1);
        g_adapter = check_vidmode_key(0, kbdchar);
        if (g_adapter >= 0)
            break;                                 // got a video mode now
#ifndef XFRACT
        if ('A' <= kbdchar && kbdchar <= 'Z')
            kbdchar = tolower(kbdchar);
#endif
        if (kbdchar == 'd')
        {                     // shell to DOS
            driver_set_clear();
            driver_shell();
            return main_state::IMAGE_START;
        }

#ifndef XFRACT
        if (kbdchar == '@' || kbdchar == '2')
        {    // execute commands
#else
        if (kbdchar == FIK_F2 || kbdchar == '@')
        {     // We mapped @ to F2
#endif
            if ((get_commands() & CMDARG_3D_YES) == 0)
                return main_state::IMAGE_START;
            kbdchar = '3';                         // 3d=y so fall thru '3' code
        }
#ifndef XFRACT
        if (kbdchar == 'r' || kbdchar == '3' || kbdchar == '#')
        {
#else
        if (kbdchar == 'r' || kbdchar == '3' || kbdchar == FIK_F3)
        {
#endif
            display_3d = display_3d_modes::NONE;
            if (kbdchar == '3' || kbdchar == '#' || kbdchar == FIK_F3)
                display_3d = display_3d_modes::YES;
            if (colors_preloaded)
                memcpy(old_dac_box, g_dac_box, 256*3); // save in case colors= present
            driver_set_for_text(); // switch to text mode
            show_file = -1;
            return main_state::RESTORE_START;
        }
        if (kbdchar == 't')
        {                   // set fractal type
            julibrot = false;
            get_fracttype();
            return main_state::IMAGE_START;
        }
        if (kbdchar == 'x')
        {                   // generic toggle switch
            get_toggles();
            return main_state::IMAGE_START;
        }
        if (kbdchar == 'y')
        {                   // generic toggle switch
            get_toggles2();
            return main_state::IMAGE_START;
        }
        if (kbdchar == 'z')
        {                   // type specific parms
            get_fract_params(1);
            return main_state::IMAGE_START;
        }
        if (kbdchar == 'v')
        {                   // view parameters
            get_view_params();
            return main_state::IMAGE_START;
        }
        if (kbdchar == FIK_CTL_B)
        {             /* ctrl B = browse parms*/
            get_browse_params();
            return main_state::IMAGE_START;
        }
        if (kbdchar == FIK_CTL_F)
        {             /* ctrl f = sound parms*/
            get_sound_params();
            return main_state::IMAGE_START;
        }
        if (kbdchar == 'f')
        {                   // floating pt toggle
            if (!usr_floatflag)
                usr_floatflag = true;
            else
                usr_floatflag = false;
            return main_state::IMAGE_START;
        }
        if (kbdchar == 'i')
        {                     // set 3d fractal parms
            get_fract3d_params(); // get the parameters
            return main_state::IMAGE_START;
        }
        if (kbdchar == 'g')
        {
            get_cmd_string(); // get command string
            return main_state::IMAGE_START;
        }
    }

    zoomoff = true;                     // zooming is enabled
    help_mode = HELPMAIN;                // now use this help mode
    resumeflag = false;                 // allows taking goto inside big_while_loop()

    return main_state::CONTINUE;
}

int main(int argc, char **argv)
{
    bool resumeflag = false;
    bool kbdmore = false;               // continuation variable
    bool stacked = false;               // flag to indicate screen stacked

    // this traps non-math library floating point errors
    signal(SIGFPE, my_floating_point_err);

    initasmvars();                       // initialize ASM stuff
    InitMemory();

    // let drivers add their video modes
    if (! init_drivers(&argc, argv))
    {
        init_failure("Sorry, I couldn't find any working video drivers for your system\n");
        exit(-1);
    }
    // load fractint.cfg, match against driver supplied modes
    load_fractint_config();
    init_help();

restart:   // insert key re-starts here
    main_restart(argc, argv, stacked);

restorestart:
    if (main_restore_start(stacked, resumeflag))
    {
        goto resumeloop;                // ooh, this is ugly
    }

imagestart:                             // calc/display a new image
    switch (main_image_start(stacked, resumeflag))
    {
    case main_state::RESTORE_START:
        goto restorestart;

    case main_state::IMAGE_START:
        goto imagestart;

    case main_state::RESTART:
        goto restart;

    default:
        break;
    }

resumeloop:
#if defined(_WIN32)
    _ASSERTE(_CrtCheckMemory());
#endif
    param_history(0); // save old history
    // this switch processes gotos that are now inside function
    switch (big_while_loop(&kbdmore, &stacked, resumeflag))
    {
    case main_state::RESTART:
        goto restart;

    case main_state::IMAGE_START:
        goto imagestart;

    case main_state::RESTORE_START:
        goto restorestart;

    default:
        break;
    }

    return 0;
}

bool check_key()
{
    int key = driver_key_pressed();
    if (key != 0)
    {
        if (show_orbit)
        {
            scrub_orbit();
        }
        if (key != 'o' && key != 'O')
        {
            return true;
        }
        driver_get_key();
        if (!driver_diskp())
        {
            show_orbit = !show_orbit;
        }
    }
    return false;
}

/* timer function:
     timer(0,(*fractal)())              fractal engine
     timer(1,nullptr,int width)         decoder
     timer(2)                           encoder
  */
int timer(int timertype, int(*subrtn)(), ...)
{
    va_list arg_marker;  // variable arg list
    char *timestring;
    time_t ltime;
    FILE *fp = nullptr;
    int out = 0;
    int i;

    va_start(arg_marker, subrtn);

    bool do_bench = timerflag; // record time?
    if (timertype == 2)   // encoder, record time only if debug flag set
        do_bench = (debugflag == debug_flags::benchmark_encoder);
    if (do_bench)
        fp = dir_fopen(workdir.c_str(), "bench", "a");
    timer_start = clock_ticks();
    switch (timertype)
    {
    case 0:
        out = (*(int(*)())subrtn)();
        break;
    case 1:
        i = va_arg(arg_marker, int);
        out = (int)decoder((short)i); // not indirect, safer with overlays
        break;
    case 2:
        out = encoder();            // not indirect, safer with overlays
        break;
    }
    // next assumes CLOCKS_PER_SEC is 10^n, n>=2
    timer_interval = (clock_ticks() - timer_start) / (CLOCKS_PER_SEC/100);

    if (do_bench)
    {
        time(&ltime);
        timestring = ctime(&ltime);
        timestring[24] = 0; //clobber newline in time string
        switch (timertype)
        {
        case 1:
            fprintf(fp, "decode ");
            break;
        case 2:
            fprintf(fp, "encode ");
            break;
        }
        fprintf(fp, "%s type=%s resolution = %dx%d maxiter=%ld",
                timestring,
                curfractalspecific->name,
                xdots,
                ydots,
                maxit);
        fprintf(fp, " time= %ld.%02ld secs\n", timer_interval/100, timer_interval%100);
        if (fp != nullptr)
            fclose(fp);
    }
    return out;
}
