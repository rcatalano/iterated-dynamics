#include <string>
#include <vector>

#include <string.h>
#include <time.h>

#ifndef XFRACT
#include <io.h>
#endif

#include <stdarg.h>

#include <ctype.h>
// see Fractint.c for a description of the "include"  hierarchy
#include "port.h"
#include "prototyp.h"
#include "fractype.h"
#include "helpdefs.h"
#include "drivers.h"

// routines in this module

static main_state evolver_menu_switch(int *kbdchar, bool *frommandel, bool *kbdmore, bool *stacked);
static void move_zoombox(int);
static bool fromtext_flag = false;      // = true if we're in graphics mode
static int call_line3d(BYTE *pixels, int linelen);
static  void move_zoombox(int keynum);
static  void cmp_line_cleanup();
static void restore_history_info(int);
static void save_history_info();

int finishrow = 0;    // save when this row is finished
EVOLUTION_INFO evolve_info = { 0 };
bool have_evolve_info = false;
char old_stdcalcmode;
static  int        historyptr = -1;     // user pointer into history tbl
static  int        saveptr = 0;         // save ptr into history tbl
static bool historyflag = false;        // are we backing off in history?
void (*outln_cleanup)();
bool g_virtual_screens = false;

main_state big_while_loop(bool *const kbdmore, bool *const stacked, bool const resumeflag)
{
    double  ftemp;                       // fp temp
    int     i = 0;                           // temporary loop counters
    int kbdchar;
    main_state mms_value;

#if defined(_WIN32)
    _ASSERTE(_CrtCheckMemory());
#endif
    bool frommandel = false;            // if julia entered from mandel
    if (resumeflag)
        goto resumeloop;

    while (1)                    // eternal loop
    {
#if defined(_WIN32)
        _ASSERTE(_CrtCheckMemory());
#endif

        if (calc_status != calc_status_value::RESUMABLE || show_file == 0)
        {
            memcpy((char *)&g_video_entry, (char *)&g_video_table[g_adapter],
                   sizeof(g_video_entry));
            dotmode = g_video_entry.dotmode;     // assembler dot read/write
            xdots   = g_video_entry.xdots;       // # dots across the screen
            ydots   = g_video_entry.ydots;       // # dots down the screen
            colors  = g_video_entry.colors;      // # colors available
            dotmode %= 100;
            sxdots  = xdots;
            sydots  = ydots;
            syoffs = 0;
            sxoffs = 0;
            rotate_hi = (rotate_hi < colors) ? rotate_hi : colors - 1;

            memcpy(old_dac_box, g_dac_box, 256*3); // save the DAC

            if (overlay_3d && (init_batch == batch_modes::NONE))
            {
                driver_unstack_screen();            // restore old graphics image
                overlay_3d = false;
            }
            else
            {
                driver_set_video_mode(&g_video_entry); // switch video modes
                // switching video modes may have changed drivers or disk flag...
                if (!g_good_mode)
                {
                    if (driver_diskp())
                    {
                        askvideo = true;
                    }
                    else
                    {
                        stopmsg(STOPMSG_NONE,
                            "That video mode is not available with your adapter.");
                        askvideo = true;
                    }
                    g_init_mode = -1;
                    driver_set_for_text(); // switch to text mode
                    return main_state::RESTORE_START;
                }

                if (g_virtual_screens && (xdots > sxdots || ydots > sydots))
                {
                    char buf[120];
                    static char msgxy1[] = {"Can't set virtual line that long, width cut down."};
                    static char msgxy2[] = {"Not enough video memory for that many lines, height cut down."};
                    if (xdots > sxdots && ydots > sydots)
                    {
                        sprintf(buf, "%s\n%s", msgxy1, msgxy2);
                        stopmsg(STOPMSG_NONE, buf);
                    }
                    else if (ydots > sydots)
                    {
                        stopmsg(STOPMSG_NONE, msgxy2);
                    }
                    else
                    {
                        stopmsg(STOPMSG_NONE, msgxy1);
                    }
                }
                xdots = sxdots;
                ydots = sydots;
                g_video_entry.xdots = xdots;
                g_video_entry.ydots = ydots;
            }

            if (savedac || colors_preloaded)
            {
                memcpy(g_dac_box, old_dac_box, 256*3); // restore the DAC
                spindac(0, 1);
                colors_preloaded = false;
            }
            else
            {   // reset DAC to defaults, which setvideomode has done for us
                if (map_specified)
                {
                    // but there's a map=, so load that
                    for (int i = 0; i < 256; ++i)
                    {
                        g_dac_box[i][0] = map_clut[i][0];
                        g_dac_box[i][1] = map_clut[i][1];
                        g_dac_box[i][2] = map_clut[i][2];
                    }
                    spindac(0, 1);
                }
                else if ((driver_diskp() && colors == 256) || !colors)
                {
                    // disk video, setvideomode via bios didn't get it right, so:
#if !defined(XFRACT) && !defined(_WIN32)
                    ValidateLuts("default"); // read the default palette file
#endif
                }
                colorstate = 0;
            }
            if (viewwindow)
            {
                // bypass for VESA virtual screen
                ftemp = finalaspectratio*(((double) sydots)/((double) sxdots)/screenaspect);
                xdots = viewxdots;
                if (xdots != 0)
                {   // xdots specified
                    ydots = viewydots;
                    if (ydots == 0) // calc ydots?
                    {
                        ydots = (int)((double)xdots * ftemp + 0.5);
                    }
                }
                else if (finalaspectratio <= screenaspect)
                {
                    xdots = (int)((double)sxdots / viewreduction + 0.5);
                    ydots = (int)((double)xdots * ftemp + 0.5);
                }
                else
                {
                    ydots = (int)((double)sydots / viewreduction + 0.5);
                    xdots = (int)((double)ydots / ftemp + 0.5);
                }
                if (xdots > sxdots || ydots > sydots)
                {
                    stopmsg(STOPMSG_NONE,
                        "View window too large; using full screen.");
                    viewwindow = false;
                    viewxdots = sxdots;
                    xdots = viewxdots;
                    viewydots = sydots;
                    ydots = viewydots;
                }
                else if (((xdots <= 1) // changed test to 1, so a 2x2 window will
                          || (ydots <= 1)) // work with the sound feature
                         && !(evolving&1))
                {   // so ssg works
                    // but no check if in evolve mode to allow lots of small views
                    stopmsg(STOPMSG_NONE,
                        "View window too small; using full screen.");
                    viewwindow = false;
                    xdots = sxdots;
                    ydots = sydots;
                }
                if ((evolving & 1) && (curfractalspecific->flags & INFCALC))
                {
                    stopmsg(STOPMSG_NONE,
                        "Fractal doesn't terminate! switching off evolution.");
                    evolving = evolving -1;
                    viewwindow = false;
                    xdots = sxdots;
                    ydots = sydots;
                }
                if (evolving & 1)
                {
                    xdots = (sxdots / evolve_image_grid_size)-!((evolving & NOGROUT)/NOGROUT);
                    xdots = xdots - (xdots % 4); // trim to multiple of 4 for SSG
                    ydots = (sydots / evolve_image_grid_size)-!((evolving & NOGROUT)/NOGROUT);
                    ydots = ydots - (ydots % 4);
                }
                else
                {
                    sxoffs = (sxdots - xdots) / 2;
                    syoffs = (sydots - ydots) / 3;
                }
            }
            x_size_d = xdots - 1;            // convert just once now
            y_size_d = ydots - 1;
        }
        // assume we save next time (except jb)
        savedac = (savedac == 0) ? 2 : 1;
        if (init_batch == batch_modes::NONE)
        {
            look_at_mouse = -FIK_PAGE_UP;        // mouse left button == pgup
        }

        if (show_file == 0)
        {   // loading an image
            outln_cleanup = nullptr;          // outln routine can set this
            if (display_3d != display_3d_modes::NONE)                 // set up 3D decoding
            {
                outln = call_line3d;
            }
            else if (comparegif)            // debug 50
            {
                outln = cmp_line;
            }
            else if (pot16bit)
            {   // .pot format input file
                if (pot_startdisk() < 0)
                {   // pot file failed?
                    show_file = 1;
                    potflag  = false;
                    pot16bit = false;
                    g_init_mode = -1;
                    calc_status = calc_status_value::RESUMABLE;         // "resume" without 16-bit
                    driver_set_for_text();
                    get_fracttype();
                    return main_state::IMAGE_START;
                }
                outln = pot_line;
            }
            else if ((soundflag & SOUNDFLAG_ORBITMASK) > SOUNDFLAG_BEEP && !evolving) // regular gif/fra input file
            {
                outln = sound_line;      // sound decoding
            }
            else
            {
                outln = out_line;        // regular decoding
            }
            if (debugflag == debug_flags::show_float_flag)
            {
                char msg[MSGLEN];
                sprintf(msg, "floatflag=%d", usr_floatflag ? 1 : 0);
                stopmsg(STOPMSG_NO_BUZZER, msg);
            }
            i = funny_glasses_call(gifview);
            if (outln_cleanup)              // cleanup routine defined?
            {
                (*outln_cleanup)();
            }
            if (i == 0)
            {
                driver_buzzer(buzzer_codes::COMPLETE);
            }
            else
            {
                calc_status = calc_status_value::NO_FRACTAL;
                if (driver_key_pressed())
                {
                    driver_buzzer(buzzer_codes::INTERRUPT);
                    while (driver_key_pressed())
                        driver_get_key();
                    texttempmsg("*** load incomplete ***");
                }
            }
        }

        zoomoff = true;                 // zooming is enabled
        if (driver_diskp() || (curfractalspecific->flags&NOZOOM) != 0)
        {
            zoomoff = false;            // for these cases disable zooming
        }
        if (!evolving)
        {
            calcfracinit();
        }
        driver_schedule_alarm(1);

        sxmin = xxmin; // save 3 corners for zoom.c ref points
        sxmax = xxmax;
        sx3rd = xx3rd;
        symin = yymin;
        symax = yymax;
        sy3rd = yy3rd;

        if (bf_math != bf_math_type::NONE)
        {
            copy_bf(bfsxmin, bfxmin);
            copy_bf(bfsxmax, bfxmax);
            copy_bf(bfsymin, bfymin);
            copy_bf(bfsymax, bfymax);
            copy_bf(bfsx3rd, bfx3rd);
            copy_bf(bfsy3rd, bfy3rd);
        }
        save_history_info();

        if (show_file == 0)
        {   // image has been loaded
            show_file = 1;
            if (init_batch == batch_modes::NORMAL && calc_status == calc_status_value::RESUMABLE)
            {
                init_batch = batch_modes::FINISH_CALC_BEFORE_SAVE;
            }
            if (loaded3d)      // 'r' of image created with '3'
            {
                display_3d = display_3d_modes::YES;  // so set flag for 'b' command
            }
        }
        else
        {   // draw an image
            if (initsavetime != 0          // autosave and resumable?
                    && (curfractalspecific->flags&NORESUME) == 0)
            {
                savebase = readticker(); // calc's start time
                saveticks = abs(initsavetime);
                saveticks *= 1092; // bios ticks/minute
                if ((saveticks & 65535L) == 0)
                {
                    ++saveticks; // make low word nonzero
                }
                finishrow = -1;
            }
            browsing = false;      // regenerate image, turn off browsing
            //rb
            name_stack_ptr = -1;   // reset pointer
            browse_name.clear();
            if (viewwindow && (evolving&1) && (calc_status != calc_status_value::COMPLETED))
            {
                // generate a set of images with varied parameters on each one
                int grout, ecount, tmpxdots, tmpydots, gridsqr;
                GENEBASE gene[NUMGENES];
                copy_genes_from_bank(gene);
                if (have_evolve_info && (calc_status == calc_status_value::RESUMABLE))
                {
                    evolve_x_parameter_range = evolve_info.x_parameter_range;
                    evolve_y_parameter_range = evolve_info.y_parameter_range;
                    evolve_new_x_parameter_offset = evolve_info.x_parameter_offset;
                    evolve_x_parameter_offset = evolve_new_x_parameter_offset;
                    evolve_new_y_parameter_offset = evolve_info.y_parameter_offset;
                    evolve_y_parameter_offset = evolve_new_y_parameter_offset;
                    evolve_new_discrete_x_parameter_offset = (char)evolve_info.discrete_x_parameter_offset;
                    evolve_discrete_x_parameter_offset = evolve_new_discrete_x_parameter_offset;
                    evolve_new_discrete_y_parameter_offset = (char)evolve_info.discrete_y_paramter_offset;
                    evolve_discrete_y_parameter_offset = evolve_new_discrete_y_parameter_offset;
                    px           = evolve_info.px;
                    py           = evolve_info.py;
                    sxoffs       = evolve_info.sxoffs;
                    syoffs       = evolve_info.syoffs;
                    xdots        = evolve_info.xdots;
                    ydots        = evolve_info.ydots;
                    evolve_image_grid_size = evolve_info.image_grid_size;
                    evolve_this_generation_random_seed = evolve_info.this_generation_random_seed;
                    evolve_max_random_mutation = evolve_info.max_random_mutation;
                    evolving     = evolve_info.evolving;
                    viewwindow = evolving != 0;
                    ecount       = evolve_info.ecount;
                    have_evolve_info = false;
                }
                else
                {   // not resuming, start from the beginning
                    int mid = evolve_image_grid_size / 2;
                    if ((px != mid) || (py != mid))
                    {
                        evolve_this_generation_random_seed = (unsigned int)clock_ticks(); // time for new set
                    }
                    param_history(0); // save old history
                    ecount = 0;
                    evolve_max_random_mutation = evolve_max_random_mutation * evolve_mutation_reduction_factor;
                    evolve_x_parameter_offset = evolve_new_x_parameter_offset;
                    evolve_y_parameter_offset = evolve_new_y_parameter_offset;
                    evolve_discrete_x_parameter_offset = evolve_new_discrete_x_parameter_offset;
                    evolve_discrete_y_parameter_offset = evolve_new_discrete_y_parameter_offset; // evolve_discrete_x_parameter_offset used for discrete parms like inside, outside, trigfn etc
                }
                param_box_count = 0;
                dpx = evolve_x_parameter_range /(evolve_image_grid_size -1);
                dpy = evolve_y_parameter_range /(evolve_image_grid_size -1);
                grout  = !((evolving & NOGROUT)/NOGROUT);
                tmpxdots = xdots+grout;
                tmpydots = ydots+grout;
                gridsqr = evolve_image_grid_size * evolve_image_grid_size;
                while (ecount < gridsqr)
                {
                    spiralmap(ecount); // sets px & py
                    sxoffs = tmpxdots * px;
                    syoffs = tmpydots * py;
                    param_history(1); // restore old history
                    fiddleparms(gene, ecount);
                    calcfracinit();
                    if (calcfract() == -1)
                    {
                        goto done;
                    }
                    ecount ++;
                }
done:
#if defined(_WIN32)
                _ASSERTE(_CrtCheckMemory());
#endif

                if (ecount == gridsqr)
                {
                    i = 0;
                    driver_buzzer(buzzer_codes::COMPLETE); // finished!!
                }
                else
                {
                    evolve_info.x_parameter_range = evolve_x_parameter_range;
                    evolve_info.y_parameter_range = evolve_y_parameter_range;
                    evolve_info.x_parameter_offset = evolve_x_parameter_offset;
                    evolve_info.y_parameter_offset = evolve_y_parameter_offset;
                    evolve_info.discrete_x_parameter_offset = (short) evolve_discrete_x_parameter_offset;
                    evolve_info.discrete_y_paramter_offset = (short) evolve_discrete_y_parameter_offset;
                    evolve_info.px              = (short)px;
                    evolve_info.py              = (short)py;
                    evolve_info.sxoffs          = (short)sxoffs;
                    evolve_info.syoffs          = (short)syoffs;
                    evolve_info.xdots           = (short)xdots;
                    evolve_info.ydots           = (short)ydots;
                    evolve_info.image_grid_size = (short) evolve_image_grid_size;
                    evolve_info.this_generation_random_seed = (short) evolve_this_generation_random_seed;
                    evolve_info.max_random_mutation = evolve_max_random_mutation;
                    evolve_info.evolving        = (short)evolving;
                    evolve_info.ecount          = (short) ecount;
                    have_evolve_info = true;
                }
                syoffs = 0;
                sxoffs = syoffs;
                xdots = sxdots;
                ydots = sydots; // otherwise save only saves a sub image and boxes get clipped

                // set up for 1st selected image, this reuses px and py
                py = evolve_image_grid_size /2;
                px = py;
                unspiralmap(); // first time called, w/above line sets up array
                param_history(1); // restore old history
                fiddleparms(gene, 0);
                copy_genes_to_bank(gene);
            }
            // end of evolution loop
            else
            {
                i = calcfract();       // draw the fractal using "C"
                if (i == 0)
                {
                    driver_buzzer(buzzer_codes::COMPLETE); // finished!!
                }
            }

            saveticks = 0;                 // turn off autosave timer
            if (driver_diskp() && i == 0) // disk-video
            {
                dvid_status(0, "Image has been completed");
            }
        }
#ifndef XFRACT
        boxcount = 0;                     // no zoom box yet
        zoom_box_width = 0;
#else
        if (!XZoomWaiting)
        {
            boxcount = 0;                 // no zoom box yet
            zoom_box_width = 0;
        }
#endif

        if (fractype == fractal_type::PLASMA)
        {
            cyclelimit = 256;              // plasma clouds need quick spins
            g_dac_count = 256;
            g_dac_learn = true;
        }

resumeloop:                             // return here on failed overlays
#if defined(_WIN32)
        _ASSERTE(_CrtCheckMemory());
#endif
        *kbdmore = true;
        while (*kbdmore)
        {   // loop through command keys
            if (timedsave != 0)
            {
                if (timedsave == 1)
                {   // woke up for timed save
                    driver_get_key();     // eat the dummy char
                    kbdchar = 's'; // do the save
                    resave_flag = 1;
                    timedsave = 2;
                }
                else
                {   // save done, resume
                    timedsave = 0;
                    resave_flag = 2;
                    kbdchar = FIK_ENTER;
                }
            }
            else if (init_batch == batch_modes::NONE)      // not batch mode
            {
#ifndef XFRACT
                look_at_mouse = (zoom_box_width == 0 && !g_video_scroll) ? -FIK_PAGE_UP : 3;
#else
                look_at_mouse = (zoom_box_width == 0) ? -FIK_PAGE_UP : 3;
#endif
                if (calc_status == calc_status_value::RESUMABLE && zoom_box_width == 0 && !driver_key_pressed())
                {
                    kbdchar = FIK_ENTER ;  // no visible reason to stop, continue
                }
                else      // wait for a real keystroke
                {
                    if (auto_browse && !no_sub_images)
                    {
                        kbdchar = 'l';
                    }
                    else
                    {
                        driver_wait_key_pressed(0);
                        kbdchar = driver_get_key();
                    }
                    if (kbdchar == FIK_ESC || kbdchar == 'm' || kbdchar == 'M')
                    {
                        if (kbdchar == FIK_ESC && escape_exit)
                        {
                            // don't ask, just get out
                            goodbye();
                        }
                        driver_stack_screen();
#ifndef XFRACT
                        kbdchar = main_menu(1);
#else
                        if (XZoomWaiting)
                        {
                            kbdchar = FIK_ENTER;
                        }
                        else
                        {
                            kbdchar = main_menu(1);
                            if (XZoomWaiting)
                            {
                                kbdchar = FIK_ENTER;
                            }
                        }
#endif
                        if (kbdchar == '\\' || kbdchar == FIK_CTL_BACKSLASH ||
                                kbdchar == 'h' || kbdchar == FIK_CTL_H ||
                                check_vidmode_key(0, kbdchar) >= 0)
                        {
                            driver_discard_screen();
                        }
                        else if (kbdchar == 'x' || kbdchar == 'y' ||
                                 kbdchar == 'z' || kbdchar == 'g' ||
                                 kbdchar == 'v' || kbdchar == FIK_CTL_B ||
                                 kbdchar == FIK_CTL_E || kbdchar == FIK_CTL_F)
                        {
                            fromtext_flag = true;
                        }
                        else
                        {
                            driver_unstack_screen();
                        }
                    }
                }
            }
            else          // batch mode, fake next keystroke
            {
                // init_batch == FINISH_CALC_BEFORE_SAVE        flag to finish calc before save
                // init_batch == NONE                           not in batch mode
                // init_batch == NORMAL                         normal batch mode
                // init_batch == SAVE                           was NORMAL, now do a save
                // init_batch == BAILOUT_ERROR_NO_SAVE          bailout with errorlevel == 2, error occurred, no save
                // init_batch == BAILOUT_INTERRUPTED_TRY_SAVE   bailout with errorlevel == 1, interrupted, try to save
                // init_batch == BAILOUT_INTERRUPTED_SAVE       was BAILOUT_INTERRUPTED_TRY_SAVE, now do a save

                if (init_batch == batch_modes::FINISH_CALC_BEFORE_SAVE)
                {
                    kbdchar = FIK_ENTER;
                    init_batch = batch_modes::NORMAL;
                }
                else if (init_batch == batch_modes::NORMAL || init_batch == batch_modes::BAILOUT_INTERRUPTED_TRY_SAVE)         // save-to-disk
                {
                    kbdchar = (debugflag == debug_flags::force_disk_restore_not_save) ? 'r' : 's';
                    if (init_batch == batch_modes::NORMAL)
                    {
                        init_batch = batch_modes::SAVE;
                    }
                    if (init_batch == batch_modes::BAILOUT_INTERRUPTED_TRY_SAVE)
                    {
                        init_batch = batch_modes::BAILOUT_INTERRUPTED_SAVE;
                    }
                }
                else
                {
                    if (calc_status != calc_status_value::COMPLETED)
                    {
                        init_batch = batch_modes::BAILOUT_ERROR_NO_SAVE; // bailout with error
                    }
                    goodbye();               // done, exit
                }
            }

#ifndef XFRACT
            if ('A' <= kbdchar && kbdchar <= 'Z')
            {
                kbdchar = tolower(kbdchar);
            }
#endif
            if (evolving)
            {
                mms_value = evolver_menu_switch(&kbdchar, &frommandel, kbdmore, stacked);
            }
            else
            {
                mms_value = main_menu_switch(&kbdchar, &frommandel, kbdmore, stacked);
            }
            if (quick_calc && (mms_value == main_state::IMAGE_START ||
                               mms_value == main_state::RESTORE_START ||
                               mms_value == main_state::RESTART))
            {
                quick_calc = false;
                usr_stdcalcmode = old_stdcalcmode;
            }
            if (quick_calc && calc_status != calc_status_value::COMPLETED)
            {
                usr_stdcalcmode = '1';
            }
            switch (mms_value)
            {
            case main_state::IMAGE_START:
                return main_state::IMAGE_START;
            case main_state::RESTORE_START:
                return main_state::RESTORE_START;
            case main_state::RESTART:
                return main_state::RESTART;
            case main_state::CONTINUE:
                continue;
            default:
                break;
            }
            if (zoomoff && *kbdmore) // draw/clear a zoom box?
            {
                drawbox(true);
            }
            if (driver_resize())
            {
                calc_status = calc_status_value::NO_FRACTAL;
            }
        }
    }
}

static bool look(bool *stacked)
{
    int old_help_mode;
    old_help_mode = help_mode;
    help_mode = HELPBROWSE;
    switch (fgetwindow())
    {
    case FIK_ENTER:
    case FIK_ENTER_2:
        show_file = 0;       // trigger load
        browsing = true;    // but don't ask for the file name as it's just been selected
        if (name_stack_ptr == 15)
        {   /* about to run off the end of the file
                * history stack so shift it all back one to
                * make room, lose the 1st one */
            for (int tmp = 1; tmp < 16; tmp++)
            {
                file_name_stack[tmp - 1] = file_name_stack[tmp];
            }
            name_stack_ptr = 14;
        }
        name_stack_ptr++;
        file_name_stack[name_stack_ptr] = browse_name;
        merge_pathnames(readname, browse_name.c_str(), cmd_file::AT_AFTER_STARTUP);
        if (askvideo)
        {
            driver_stack_screen();   // save graphics image
            *stacked = true;
        }
        return true;       // hop off and do it!!

    case '\\':
        if (name_stack_ptr >= 1)
        {
            // go back one file if somewhere to go (ie. browsing)
            name_stack_ptr--;
            while (file_name_stack[name_stack_ptr].empty()
                    && name_stack_ptr >= 0)
            {
                name_stack_ptr--;
            }
            if (name_stack_ptr < 0) // oops, must have deleted first one
            {
                break;
            }
            browse_name = file_name_stack[name_stack_ptr];
            merge_pathnames(readname, browse_name.c_str(), cmd_file::AT_AFTER_STARTUP);
            browsing = true;
            show_file = 0;
            if (askvideo)
            {
                driver_stack_screen();// save graphics image
                *stacked = true;
            }
            return true;
        }                   // otherwise fall through and turn off browsing
    case FIK_ESC:
    case 'l':              // turn it off
    case 'L':
        browsing = false;
        help_mode = old_help_mode;
        break;

    case 's':
        browsing = false;
        help_mode = old_help_mode;
        savetodisk(savename);
        break;

    default:               // or no files found, leave the state of browsing alone
        break;
    }

    return false;
}

main_state main_menu_switch(int *kbdchar, bool *frommandel, bool *kbdmore, bool *stacked)
{
    int i, k;
    static double  jxxmin, jxxmax, jyymin, jyymax; // "Julia mode" entry point
    static double  jxx3rd, jyy3rd;
    long old_maxit;

    if (quick_calc && calc_status == calc_status_value::COMPLETED)
    {
        quick_calc = false;
        usr_stdcalcmode = old_stdcalcmode;
    }
    if (quick_calc && calc_status != calc_status_value::COMPLETED)
        usr_stdcalcmode = old_stdcalcmode;
    switch (*kbdchar)
    {
    case 't':                    // new fractal type
        julibrot = false;
        clear_zoombox();
        driver_stack_screen();
        i = get_fracttype();
        if (i >= 0)
        {
            driver_discard_screen();
            savedac = 0;
            save_release = g_release;
            no_mag_calc = false;
            use_old_period = false;
            bad_outside = false;
            ldcheck = false;
            set_current_params();
            evolve_new_discrete_y_parameter_offset = 0;
            evolve_new_discrete_x_parameter_offset = 0;
            evolve_discrete_y_parameter_offset = 0;
            evolve_discrete_x_parameter_offset = 0;
            evolve_max_random_mutation = 1;           // reset param evolution stuff
            set_orbit_corners = false;
            param_history(0); // save history
            if (i == 0)
            {
                g_init_mode = g_adapter;
                *frommandel = false;
            }
            else if (g_init_mode < 0) // it is supposed to be...
                driver_set_for_text();     // reset to text mode
            return main_state::IMAGE_START;
        }
        driver_unstack_screen();
        break;
    case FIK_CTL_X:                     // Ctl-X, Ctl-Y, CTL-Z do flipping
    case FIK_CTL_Y:
    case FIK_CTL_Z:
        flip_image(*kbdchar);
        break;
    case 'x':                    // invoke options screen
    case 'y':
    case 'p':                    // passes options
    case 'z':                    // type specific parms
    case 'g':
    case 'v':
    case FIK_CTL_B:
    case FIK_CTL_E:
    case FIK_CTL_F:
        old_maxit = maxit;
        clear_zoombox();
        if (fromtext_flag)
            fromtext_flag = false;
        else
            driver_stack_screen();
        if (*kbdchar == 'x')
            i = get_toggles();
        else if (*kbdchar == 'y')
            i = get_toggles2();
        else if (*kbdchar == 'p')
            i = passes_options();
        else if (*kbdchar == 'z')
            i = get_fract_params(1);
        else if (*kbdchar == 'v')
            i = get_view_params(); // get the parameters
        else if (*kbdchar == FIK_CTL_B)
            i = get_browse_params();
        else if (*kbdchar == FIK_CTL_E)
        {
            i = get_evolve_Parms();
            if (i > 0)
            {
                start_show_orbit = false;
                soundflag &= ~(SOUNDFLAG_X | SOUNDFLAG_Y | SOUNDFLAG_Z); // turn off only x,y,z
                Log_Auto_Calc = false; // turn it off
            }
        }
        else if (*kbdchar == FIK_CTL_F)
            i = get_sound_params();
        else
            i = get_cmd_string();
        driver_unstack_screen();
        if (evolving && truecolor)
            truecolor = false;          // truecolor doesn't play well with the evolver
        if (maxit > old_maxit && inside >= COLOR_BLACK && calc_status == calc_status_value::COMPLETED &&
                curfractalspecific->calctype == StandardFractal && !LogFlag &&
                !truecolor &&    // recalc not yet implemented with truecolor
                !(usr_stdcalcmode == 't' && fillcolor > -1) &&
                // tesseral with fill doesn't work
                !(usr_stdcalcmode == 'o') &&
                i == 1 && // nothing else changed
                outside != ATAN)
        {
            quick_calc = true;
            old_stdcalcmode = usr_stdcalcmode;
            usr_stdcalcmode = '1';
            *kbdmore = false;
            calc_status = calc_status_value::RESUMABLE;
        }
        else if (i > 0)
        {                               // time to redraw?
            quick_calc = false;
            param_history(0);           // save history
            *kbdmore = false;
            calc_status = calc_status_value::PARAMS_CHANGED;
        }
        break;
#ifndef XFRACT
    case '@':                    // execute commands
    case '2':                    // execute commands
#else
    case FIK_F2:                     // execute commands
#endif
        driver_stack_screen();
        i = get_commands();
        if (g_init_mode != -1)
        {   // video= was specified
            g_adapter = g_init_mode;
            g_init_mode = -1;
            i |= CMDARG_FRACTAL_PARAM;
            savedac = 0;
        }
        else if (colors_preloaded)
        {   // colors= was specified
            spindac(0, 1);
            colors_preloaded = false;
        }
        else if (i & CMDARG_RESET)         // reset was specified
            savedac = 0;
        if (i & CMDARG_3D_YES)
        {   // 3d = was specified
            *kbdchar = '3';
            driver_unstack_screen();
            goto do_3d_transform;  // pretend '3' was keyed
        }
        if (i & CMDARG_FRACTAL_PARAM)
        {   // fractal parameter changed
            driver_discard_screen();
            *kbdmore = false;
            calc_status = calc_status_value::PARAMS_CHANGED;
        }
        else
            driver_unstack_screen();
        break;
    case 'f':                    // floating pt toggle
        if (!usr_floatflag)
            usr_floatflag = true;
        else if (stdcalcmode != 'o') // don't go there
            usr_floatflag = false;
        g_init_mode = g_adapter;
        return main_state::IMAGE_START;
    case 'i':                    // 3d fractal parms
        if (get_fract3d_params() >= 0)    // get the parameters
        {
            calc_status = calc_status_value::PARAMS_CHANGED;
            *kbdmore = 0;    // time to redraw
        }
        break;
    case FIK_CTL_A:                     // ^a Ant
        clear_zoombox();
        {
            int err;
            double oldparm[MAXPARAMS];
            fractal_type oldtype = fractype;
            for (int i = 0; i < MAXPARAMS; ++i)
                oldparm[i] = param[i];
            if (fractype != fractal_type::ANT)
            {
                fractype = fractal_type::ANT;
                curfractalspecific = &fractalspecific[static_cast<int>(fractype)];
                load_params(fractype);
            }
            if (!fromtext_flag)
                driver_stack_screen();
            fromtext_flag = false;
            err = get_fract_params(2);
            if (err >= 0)
            {
                driver_unstack_screen();
                if (ant() >= 0)
                    calc_status = calc_status_value::PARAMS_CHANGED;
            }
            else
                driver_unstack_screen();
            fractype = oldtype;
            for (int i = 0; i < MAXPARAMS; ++i)
                param[i] = oldparm[i];
            if (err >= 0)
                return main_state::CONTINUE;
        }
        break;
    case 'k':                    // ^s is irritating, give user a single key
    case FIK_CTL_S:                     // ^s RDS
        clear_zoombox();
        if (get_rds_params() >= 0)
        {
            if (do_AutoStereo())
                calc_status = calc_status_value::PARAMS_CHANGED;
            return main_state::CONTINUE;
        }
        break;
    case 'a':                    // starfield parms
        clear_zoombox();
        if (get_starfield_params() >= 0)
        {
            if (starfield() >= 0)
                calc_status = calc_status_value::PARAMS_CHANGED;
            return main_state::CONTINUE;
        }
        break;
    case FIK_CTL_O:                     // ctrl-o
    case 'o':
        // must use standard fractal and have a float variant
        if ((fractalspecific[static_cast<int>(fractype)].calctype == StandardFractal
                || fractalspecific[static_cast<int>(fractype)].calctype == calcfroth) &&
                (fractalspecific[static_cast<int>(fractype)].isinteger == 0 ||
                 fractalspecific[static_cast<int>(fractype)].tofloat != fractal_type::NOFRACTAL) &&
                (bf_math == bf_math_type::NONE) && // for now no arbitrary precision support
                !(g_is_true_color && truemode))
        {
            clear_zoombox();
            Jiim(jiim_types::ORBIT);
        }
        break;
    case FIK_SPACE:                  // spacebar, toggle mand/julia
        if (bf_math != bf_math_type::NONE || evolving)
            break;
        if (fractype == fractal_type::CELLULAR)
        {
            nxtscreenflag = !nxtscreenflag;
            calc_status = calc_status_value::RESUMABLE;
            *kbdmore = false;
        }
        else
        {
            if (fractype == fractal_type::FORMULA || fractype == fractal_type::FFORMULA)
            {
                if (ismand)
                {
                    fractalspecific[static_cast<int>(fractype)].tojulia = fractype;
                    fractalspecific[static_cast<int>(fractype)].tomandel = fractal_type::NOFRACTAL;
                    ismand = false;
                }
                else
                {
                    fractalspecific[static_cast<int>(fractype)].tojulia = fractal_type::NOFRACTAL;
                    fractalspecific[static_cast<int>(fractype)].tomandel = fractype;
                    ismand = true;
                }
            }
            if (curfractalspecific->tojulia != fractal_type::NOFRACTAL
                    && param[0] == 0.0 && param[1] == 0.0)
            {
                // switch to corresponding Julia set
                int key;
                if ((fractype == fractal_type::MANDEL || fractype == fractal_type::MANDELFP) && bf_math == bf_math_type::NONE)
                    hasinverse = true;
                else
                    hasinverse = false;
                clear_zoombox();
                Jiim(jiim_types::JIIM);
                key = driver_get_key();    // flush keyboard buffer
                if (key != FIK_SPACE)
                {
                    driver_unget_key(key);
                    break;
                }
                fractype = curfractalspecific->tojulia;
                curfractalspecific = &fractalspecific[static_cast<int>(fractype)];
                if (xcjul == BIG || ycjul == BIG)
                {
                    param[0] = (xxmax + xxmin) / 2;
                    param[1] = (yymax + yymin) / 2;
                }
                else
                {
                    param[0] = xcjul;
                    param[1] = ycjul;
                    ycjul = BIG;
                    xcjul = ycjul;
                }
                jxxmin = sxmin;
                jxxmax = sxmax;
                jyymax = symax;
                jyymin = symin;
                jxx3rd = sx3rd;
                jyy3rd = sy3rd;
                *frommandel = true;
                xxmin = curfractalspecific->xmin;
                xxmax = curfractalspecific->xmax;
                yymin = curfractalspecific->ymin;
                yymax = curfractalspecific->ymax;
                xx3rd = xxmin;
                yy3rd = yymin;
                if (usr_distest == 0 && usr_biomorph != -1 && bitshift != 29)
                {
                    xxmin *= 3.0;
                    xxmax *= 3.0;
                    yymin *= 3.0;
                    yymax *= 3.0;
                    xx3rd *= 3.0;
                    yy3rd *= 3.0;
                }
                zoomoff = true;
                calc_status = calc_status_value::PARAMS_CHANGED;
                *kbdmore = false;
            }
            else if (curfractalspecific->tomandel != fractal_type::NOFRACTAL)
            {
                // switch to corresponding Mandel set
                fractype = curfractalspecific->tomandel;
                curfractalspecific = &fractalspecific[static_cast<int>(fractype)];
                if (*frommandel)
                {
                    xxmin = jxxmin;
                    xxmax = jxxmax;
                    yymin = jyymin;
                    yymax = jyymax;
                    xx3rd = jxx3rd;
                    yy3rd = jyy3rd;
                }
                else
                {
                    xx3rd = curfractalspecific->xmin;
                    xxmin = xx3rd;
                    xxmax = curfractalspecific->xmax;
                    yy3rd = curfractalspecific->ymin;
                    yymin = yy3rd;
                    yymax = curfractalspecific->ymax;
                }
                SaveC.x = param[0];
                SaveC.y = param[1];
                param[0] = 0;
                param[1] = 0;
                zoomoff = true;
                calc_status = calc_status_value::PARAMS_CHANGED;
                *kbdmore = false;
            }
            else
                driver_buzzer(buzzer_codes::PROBLEM);          // can't switch
        }                         // end of else for if == cellular
        break;
    case 'j':                    // inverse julia toggle
        // if the inverse types proliferate, something more elegant will be needed
        if (fractype == fractal_type::JULIA || fractype == fractal_type::JULIAFP || fractype == fractal_type::INVERSEJULIA)
        {
            static fractal_type oldtype = fractal_type::NOFRACTAL;
            if (fractype == fractal_type::JULIA || fractype == fractal_type::JULIAFP)
            {
                oldtype = fractype;
                fractype = fractal_type::INVERSEJULIA;
            }
            else if (fractype == fractal_type::INVERSEJULIA)
            {
                if (oldtype != fractal_type::NOFRACTAL)
                    fractype = oldtype;
                else
                    fractype = fractal_type::JULIA;
            }
            curfractalspecific = &fractalspecific[static_cast<int>(fractype)];
            zoomoff = true;
            calc_status = calc_status_value::PARAMS_CHANGED;
            *kbdmore = false;
        }
        else
            driver_buzzer(buzzer_codes::PROBLEM);
        break;
    case '\\':                   // return to prev image
    case FIK_CTL_BACKSLASH:
    case 'h':
    case FIK_BACKSPACE:
        if (name_stack_ptr >= 1)
        {
            // go back one file if somewhere to go (ie. browsing)
            name_stack_ptr--;
            while (file_name_stack[name_stack_ptr].empty()
                    && name_stack_ptr >= 0)
                name_stack_ptr--;
            if (name_stack_ptr < 0) // oops, must have deleted first one
                break;
            browse_name = file_name_stack[name_stack_ptr];
            merge_pathnames(readname, browse_name.c_str(), cmd_file::AT_AFTER_STARTUP);
            browsing = true;
            no_sub_images = false;
            show_file = 0;
            if (askvideo)
            {
                driver_stack_screen();      // save graphics image
                *stacked = true;
            }
            return main_state::RESTORE_START;
        }
        else if (maxhistory > 0 && bf_math == bf_math_type::NONE)
        {
            if (*kbdchar == '\\' || *kbdchar == 'h')
                if (--historyptr < 0)
                    historyptr = maxhistory - 1;
            if (*kbdchar == FIK_CTL_BACKSLASH || *kbdchar == FIK_BACKSPACE)
                if (++historyptr >= maxhistory)
                    historyptr = 0;
            restore_history_info(historyptr);
            zoomoff = true;
            g_init_mode = g_adapter;
            if (curfractalspecific->isinteger != 0 &&
                    curfractalspecific->tofloat != fractal_type::NOFRACTAL)
                usr_floatflag = false;
            if (curfractalspecific->isinteger == 0 &&
                    curfractalspecific->tofloat != fractal_type::NOFRACTAL)
                usr_floatflag = true;
            historyflag = true;         // avoid re-store parms due to rounding errs
            return main_state::IMAGE_START;
        }
        break;
    case 'd':                    // shell to MS-DOS
        driver_stack_screen();
        driver_shell();
        driver_unstack_screen();
        break;

    case 'c':                    // switch to color cycling
    case '+':                    // rotate palette
    case '-':                    // rotate palette
        clear_zoombox();
        memcpy(old_dac_box, g_dac_box, 256 * 3);
        rotate((*kbdchar == 'c') ? 0 : ((*kbdchar == '+') ? 1 : -1));
        if (memcmp(old_dac_box, g_dac_box, 256 * 3))
        {
            colorstate = 1;
            save_history_info();
        }
        return main_state::CONTINUE;
    case 'e':                    // switch to color editing
        if (g_is_true_color && (init_batch == batch_modes::NONE))
        { // don't enter palette editor
            if (!load_palette())
            {
                *kbdmore = false;
                calc_status = calc_status_value::PARAMS_CHANGED;
                break;
            }
            else
                return main_state::CONTINUE;
        }
        clear_zoombox();
        if (g_dac_box[0][0] != 255 && colors >= 16 && !driver_diskp())
        {
            int old_help_mode;
            old_help_mode = help_mode;
            memcpy(old_dac_box, g_dac_box, 256 * 3);
            help_mode = HELPXHAIR;
            EditPalette();
            help_mode = old_help_mode;
            if (memcmp(old_dac_box, g_dac_box, 256 * 3))
            {
                colorstate = 1;
                save_history_info();
            }
        }
        return main_state::CONTINUE;
    case 's':                    // save-to-disk
        if (driver_diskp() && disktarga)
            return main_state::CONTINUE;  // disk video and targa, nothing to save
        savetodisk(savename);
        return main_state::CONTINUE;
    case '#':                    // 3D overlay
#ifdef XFRACT
    case FIK_F3:                     // 3D overlay
#endif
        clear_zoombox();
        overlay_3d = true;
    case '3':                    // restore-from (3d)
do_3d_transform:
        if (overlay_3d)
            display_3d = display_3d_modes::B_COMMAND;         // for <b> command
        else
            display_3d = display_3d_modes::YES;
    case 'r':                    // restore-from
        comparegif = false;
        *frommandel = false;
        browsing = false;
        if (*kbdchar == 'r')
        {
            if (debugflag == debug_flags::force_disk_restore_not_save)
            {
                comparegif = true;
                overlay_3d = true;
                if (init_batch == batch_modes::SAVE)
                {
                    driver_stack_screen();   // save graphics image
                    readname = savename;
                    show_file = 0;
                    return main_state::RESTORE_START;
                }
            }
            else
            {
                comparegif = false;
                overlay_3d = false;
            }
            display_3d = display_3d_modes::NONE;
        }
        driver_stack_screen();            // save graphics image
        if (overlay_3d)
            *stacked = false;
        else
            *stacked = true;
        if (resave_flag)
        {
            updatesavename(savename);      // do the pending increment
            resave_flag = 0;
            started_resaves = false;
        }
        show_file = -1;
        return main_state::RESTORE_START;
    case 'l':
    case 'L':                    // Look for other files within this view
        if ((zoom_box_width != 0) || driver_diskp())
        {
            browsing = false;
            driver_buzzer(buzzer_codes::PROBLEM);             // can't browse if zooming or disk video
        }
        else if (look(stacked))
        {
            return main_state::RESTORE_START;
        }
        break;
    case 'b':                    // make batch file
        make_batch_file();
        break;
    case FIK_CTL_P:                    // print current image
        driver_buzzer(buzzer_codes::INTERRUPT);
        return main_state::CONTINUE;
    case FIK_ENTER:                  // Enter
    case FIK_ENTER_2:                // Numeric-Keypad Enter
#ifdef XFRACT
        XZoomWaiting = false;
#endif
        if (zoom_box_width != 0.0)
        {   // do a zoom
            init_pan_or_recalc(0);
            *kbdmore = false;
        }
        if (calc_status != calc_status_value::COMPLETED)     // don't restart if image complete
            *kbdmore = false;
        break;
    case FIK_CTL_ENTER:              // control-Enter
    case FIK_CTL_ENTER_2:            // Control-Keypad Enter
        init_pan_or_recalc(1);
        *kbdmore = false;
        zoomout();                // calc corners for zooming out
        break;
    case FIK_INSERT:         // insert
        driver_set_for_text();           // force text mode
        return main_state::RESTART;
    case FIK_LEFT_ARROW:             // cursor left
    case FIK_RIGHT_ARROW:            // cursor right
    case FIK_UP_ARROW:               // cursor up
    case FIK_DOWN_ARROW:             // cursor down
        move_zoombox(*kbdchar);
        break;
    case FIK_CTL_LEFT_ARROW:           // Ctrl-cursor left
    case FIK_CTL_RIGHT_ARROW:          // Ctrl-cursor right
    case FIK_CTL_UP_ARROW:             // Ctrl-cursor up
    case FIK_CTL_DOWN_ARROW:           // Ctrl-cursor down
        move_zoombox(*kbdchar);
        break;
    case FIK_CTL_HOME:               // Ctrl-home
        if (boxcount && (curfractalspecific->flags & NOROTATE) == 0)
        {
            i = key_count(FIK_CTL_HOME);
            if ((zoom_box_skew -= 0.02 * i) < -0.48)
                zoom_box_skew = -0.48;
        }
        break;
    case FIK_CTL_END:                // Ctrl-end
        if (boxcount && (curfractalspecific->flags & NOROTATE) == 0)
        {
            i = key_count(FIK_CTL_END);
            if ((zoom_box_skew += 0.02 * i) > 0.48)
                zoom_box_skew = 0.48;
        }
        break;
    case FIK_CTL_PAGE_UP:            // Ctrl-pgup
        if (boxcount)
            chgboxi(0, -2 * key_count(FIK_CTL_PAGE_UP));
        break;
    case FIK_CTL_PAGE_DOWN:          // Ctrl-pgdn
        if (boxcount)
            chgboxi(0, 2 * key_count(FIK_CTL_PAGE_DOWN));
        break;

    case FIK_PAGE_UP:                // page up
        if (zoomoff)
        {
            if (zoom_box_width == 0)
            {   // start zoombox
                zoom_box_height = 1;
                zoom_box_width = zoom_box_height;
                zoom_box_rotation = 0;
                zoom_box_skew = zoom_box_rotation;
                zby = 0;
                zbx = zby;
                find_special_colors();
                boxcolor = g_color_bright;
                py = evolve_image_grid_size /2;
                px = py;
                moveboxf(0.0, 0.0); // force scrolling
            }
            else
                resizebox(0 - key_count(FIK_PAGE_UP));
        }
        break;
    case FIK_PAGE_DOWN:              // page down
        if (boxcount)
        {
            if (zoom_box_width >= .999 && zoom_box_height >= 0.999) // end zoombox
                zoom_box_width = 0;
            else
                resizebox(key_count(FIK_PAGE_DOWN));
        }
        break;
    case FIK_CTL_MINUS:              // Ctrl-kpad-
        if (boxcount && (curfractalspecific->flags & NOROTATE) == 0)
            zoom_box_rotation += key_count(FIK_CTL_MINUS);
        break;
    case FIK_CTL_PLUS:               // Ctrl-kpad+
        if (boxcount && (curfractalspecific->flags & NOROTATE) == 0)
            zoom_box_rotation -= key_count(FIK_CTL_PLUS);
        break;
    case FIK_CTL_INSERT:             // Ctrl-ins
        boxcolor += key_count(FIK_CTL_INSERT);
        break;
    case FIK_CTL_DEL:                // Ctrl-del
        boxcolor -= key_count(FIK_CTL_DEL);
        break;

    case FIK_ALT_1: // alt + number keys set mutation level and start evolution engine
    case FIK_ALT_2:
    case FIK_ALT_3:
    case FIK_ALT_4:
    case FIK_ALT_5:
    case FIK_ALT_6:
    case FIK_ALT_7:
        evolving = 1;
        viewwindow = true;
        set_mutation_level(*kbdchar - FIK_ALT_1 + 1);
        param_history(0); // save parameter history
        *kbdmore = false;
        calc_status = calc_status_value::PARAMS_CHANGED;
        break;

    case FIK_DELETE:         // select video mode from list
    {
        driver_stack_screen();
        *kbdchar = select_video_mode(g_adapter);
        if (check_vidmode_key(0, *kbdchar) >= 0)  // picked a new mode?
            driver_discard_screen();
        else
            driver_unstack_screen();
        // fall through
    }
    default:                     // other (maybe a valid Fn key)
        k = check_vidmode_key(0, *kbdchar);
        if (k >= 0)
        {
            g_adapter = k;
            if (g_video_table[g_adapter].colors != colors)
                savedac = 0;
            calc_status = calc_status_value::PARAMS_CHANGED;
            *kbdmore = false;
            return main_state::CONTINUE;
        }
        break;
    }                            // end of the big switch
    return main_state::NOTHING;
}

static main_state evolver_menu_switch(int *kbdchar, bool *frommandel, bool *kbdmore, bool *stacked)
{
    int i, k;

    switch (*kbdchar)
    {
    case 't':                    // new fractal type
        julibrot = false;
        clear_zoombox();
        driver_stack_screen();
        i = get_fracttype();
        if (i >= 0)
        {
            driver_discard_screen();
            savedac = 0;
            save_release = g_release;
            no_mag_calc = false;
            use_old_period = false;
            bad_outside = false;
            ldcheck = false;
            set_current_params();
            evolve_new_discrete_y_parameter_offset = 0;
            evolve_new_discrete_x_parameter_offset = 0;
            evolve_discrete_y_parameter_offset = 0;
            evolve_discrete_x_parameter_offset = 0;
            evolve_max_random_mutation = 1;           // reset param evolution stuff
            set_orbit_corners = false;
            param_history(0); // save history
            if (i == 0)
            {
                g_init_mode = g_adapter;
                *frommandel = false;
            }
            else if (g_init_mode < 0) // it is supposed to be...
                driver_set_for_text();     // reset to text mode
            return main_state::IMAGE_START;
        }
        driver_unstack_screen();
        break;
    case 'x':                    // invoke options screen
    case 'y':
    case 'p':                    // passes options
    case 'z':                    // type specific parms
    case 'g':
    case FIK_CTL_E:
    case FIK_SPACE:
        clear_zoombox();
        if (fromtext_flag)
            fromtext_flag = false;
        else
            driver_stack_screen();
        if (*kbdchar == 'x')
            i = get_toggles();
        else if (*kbdchar == 'y')
            i = get_toggles2();
        else if (*kbdchar == 'p')
            i = passes_options();
        else if (*kbdchar == 'z')
            i = get_fract_params(1);
        else if (*kbdchar == FIK_CTL_E || *kbdchar == FIK_SPACE)
            i = get_evolve_Parms();
        else
            i = get_cmd_string();
        driver_unstack_screen();
        if (evolving && truecolor)
            truecolor = false;          // truecolor doesn't play well with the evolver
        if (i > 0)
        {              // time to redraw?
            param_history(0); // save history
            *kbdmore = false;
            calc_status = calc_status_value::PARAMS_CHANGED;
        }
        break;
    case 'b': // quick exit from evolve mode
        evolving = 0;
        viewwindow = false;
        param_history(0); // save history
        *kbdmore = false;
        calc_status = calc_status_value::PARAMS_CHANGED;
        break;

    case 'f':                    // floating pt toggle
        if (!usr_floatflag)
            usr_floatflag = true;
        else if (stdcalcmode != 'o') // don't go there
            usr_floatflag = false;
        g_init_mode = g_adapter;
        return main_state::IMAGE_START;
    case '\\':                   // return to prev image
    case FIK_CTL_BACKSLASH:
    case 'h':
    case FIK_BACKSPACE:
        if (maxhistory > 0 && bf_math == bf_math_type::NONE)
        {
            if (*kbdchar == '\\' || *kbdchar == 'h')
                if (--historyptr < 0)
                    historyptr = maxhistory - 1;
            if (*kbdchar == FIK_CTL_BACKSLASH || *kbdchar == 8)
                if (++historyptr >= maxhistory)
                    historyptr = 0;
            restore_history_info(historyptr);
            zoomoff = true;
            g_init_mode = g_adapter;
            if (curfractalspecific->isinteger != 0 &&
                    curfractalspecific->tofloat != fractal_type::NOFRACTAL)
                usr_floatflag = false;
            if (curfractalspecific->isinteger == 0 &&
                    curfractalspecific->tofloat != fractal_type::NOFRACTAL)
                usr_floatflag = true;
            historyflag = true;         // avoid re-store parms due to rounding errs
            return main_state::IMAGE_START;
        }
        break;
    case 'c':                    // switch to color cycling
    case '+':                    // rotate palette
    case '-':                    // rotate palette
        clear_zoombox();
        memcpy(old_dac_box, g_dac_box, 256 * 3);
        rotate((*kbdchar == 'c') ? 0 : ((*kbdchar == '+') ? 1 : -1));
        if (memcmp(old_dac_box, g_dac_box, 256 * 3))
        {
            colorstate = 1;
            save_history_info();
        }
        return main_state::CONTINUE;
    case 'e':                    // switch to color editing
        if (g_is_true_color && (init_batch == batch_modes::NONE))
        { // don't enter palette editor
            if (!load_palette())
            {
                *kbdmore = false;
                calc_status = calc_status_value::PARAMS_CHANGED;
                break;
            }
            else
                return main_state::CONTINUE;
        }
        clear_zoombox();
        if (g_dac_box[0][0] != 255 && colors >= 16 && !driver_diskp())
        {
            int old_help_mode;
            old_help_mode = help_mode;
            memcpy(old_dac_box, g_dac_box, 256 * 3);
            help_mode = HELPXHAIR;
            EditPalette();
            help_mode = old_help_mode;
            if (memcmp(old_dac_box, g_dac_box, 256 * 3))
            {
                colorstate = 1;
                save_history_info();
            }
        }
        return main_state::CONTINUE;
    case 's':                    // save-to-disk
    {
        if (driver_diskp() && disktarga)
            return main_state::CONTINUE;  // disk video and targa, nothing to save

        int oldsxoffs, oldsyoffs, oldxdots, oldydots, oldpx, oldpy;
        GENEBASE gene[NUMGENES];
        copy_genes_from_bank(gene);
        oldsxoffs = sxoffs;
        oldsyoffs = syoffs;
        oldxdots = xdots;
        oldydots = ydots;
        oldpx = px;
        oldpy = py;
        syoffs = 0;
        sxoffs = syoffs;
        xdots = sxdots;
        ydots = sydots; // for full screen save and pointer move stuff
        py = evolve_image_grid_size / 2;
        px = py;
        param_history(1); // restore old history
        fiddleparms(gene, 0);
        drawparmbox(1);
        savetodisk(savename);
        px = oldpx;
        py = oldpy;
        param_history(1); // restore old history
        fiddleparms(gene, unspiralmap());
        sxoffs = oldsxoffs;
        syoffs = oldsyoffs;
        xdots = oldxdots;
        ydots = oldydots;
        copy_genes_to_bank(gene);
        return main_state::CONTINUE;
    }

    case 'r':                    // restore-from
        comparegif = false;
        *frommandel = false;
        browsing = false;
        if (*kbdchar == 'r')
        {
            if (debugflag == debug_flags::force_disk_restore_not_save)
            {
                comparegif = true;
                overlay_3d = true;
                if (init_batch == batch_modes::SAVE)
                {
                    driver_stack_screen();   // save graphics image
                    readname = savename;
                    show_file = 0;
                    return main_state::RESTORE_START;
                }
            }
            else
            {
                comparegif = false;
                overlay_3d = false;
            }
            display_3d = display_3d_modes::NONE;
        }
        driver_stack_screen();            // save graphics image
        if (overlay_3d)
            *stacked = false;
        else
            *stacked = true;
        if (resave_flag)
        {
            updatesavename(savename);      // do the pending increment
            resave_flag = 0;
            started_resaves = false;
        }
        show_file = -1;
        return main_state::RESTORE_START;
    case FIK_ENTER:                  // Enter
    case FIK_ENTER_2:                // Numeric-Keypad Enter
#ifdef XFRACT
        XZoomWaiting = false;
#endif
        if (zoom_box_width != 0.0)
        {   // do a zoom
            init_pan_or_recalc(0);
            *kbdmore = false;
        }
        if (calc_status != calc_status_value::COMPLETED)     // don't restart if image complete
            *kbdmore = false;
        break;
    case FIK_CTL_ENTER:              // control-Enter
    case FIK_CTL_ENTER_2:            // Control-Keypad Enter
        init_pan_or_recalc(1);
        *kbdmore = false;
        zoomout();                // calc corners for zooming out
        break;
    case FIK_INSERT:         // insert
        driver_set_for_text();           // force text mode
        return main_state::RESTART;
    case FIK_LEFT_ARROW:             // cursor left
    case FIK_RIGHT_ARROW:            // cursor right
    case FIK_UP_ARROW:               // cursor up
    case FIK_DOWN_ARROW:             // cursor down
        move_zoombox(*kbdchar);
        break;
    case FIK_CTL_LEFT_ARROW:           // Ctrl-cursor left
    case FIK_CTL_RIGHT_ARROW:          // Ctrl-cursor right
    case FIK_CTL_UP_ARROW:             // Ctrl-cursor up
    case FIK_CTL_DOWN_ARROW:           // Ctrl-cursor down
        // borrow ctrl cursor keys for moving selection box
        // in evolver mode
        if (boxcount)
        {
            GENEBASE gene[NUMGENES];
            copy_genes_from_bank(gene);
            if (evolving & 1)
            {
                if (*kbdchar == FIK_CTL_LEFT_ARROW)
                {
                    px--;
                }
                if (*kbdchar == FIK_CTL_RIGHT_ARROW)
                {
                    px++;
                }
                if (*kbdchar == FIK_CTL_UP_ARROW)
                {
                    py--;
                }
                if (*kbdchar == FIK_CTL_DOWN_ARROW)
                {
                    py++;
                }
                if (px <0)
                    px = evolve_image_grid_size -1;
                if (px > (evolve_image_grid_size -1))
                    px = 0;
                if (py < 0)
                    py = evolve_image_grid_size -1;
                if (py > (evolve_image_grid_size -1))
                    py = 0;
                int grout = !((evolving & NOGROUT)/NOGROUT) ;
                sxoffs = px * (int)(x_size_d+1+grout);
                syoffs = py * (int)(y_size_d+1+grout);

                param_history(1); // restore old history
                fiddleparms(gene, unspiralmap()); // change all parameters
                // to values appropriate to the image selected
                set_evolve_ranges();
                chgboxi(0, 0);
                drawparmbox(0);
            }
            copy_genes_to_bank(gene);
        }
        else                       // if no zoombox, scroll by arrows
            move_zoombox(*kbdchar);
        break;
    case FIK_CTL_HOME:               // Ctrl-home
        if (boxcount && (curfractalspecific->flags & NOROTATE) == 0)
        {
            i = key_count(FIK_CTL_HOME);
            if ((zoom_box_skew -= 0.02 * i) < -0.48)
                zoom_box_skew = -0.48;
        }
        break;
    case FIK_CTL_END:                // Ctrl-end
        if (boxcount && (curfractalspecific->flags & NOROTATE) == 0)
        {
            i = key_count(FIK_CTL_END);
            if ((zoom_box_skew += 0.02 * i) > 0.48)
                zoom_box_skew = 0.48;
        }
        break;
    case FIK_CTL_PAGE_UP:
        if (param_box_count)
        {
            parmzoom -= 1.0;
            if (parmzoom < 1.0)
                parmzoom = 1.0;
            drawparmbox(0);
            set_evolve_ranges();
        }
        break;
    case FIK_CTL_PAGE_DOWN:
        if (param_box_count)
        {
            parmzoom += 1.0;
            if (parmzoom > (double) evolve_image_grid_size /2.0)
                parmzoom = (double) evolve_image_grid_size /2.0;
            drawparmbox(0);
            set_evolve_ranges();
        }
        break;

    case FIK_PAGE_UP:                // page up
        if (zoomoff)
        {
            if (zoom_box_width == 0)
            {   // start zoombox
                zoom_box_height = 1;
                zoom_box_width = zoom_box_height;
                zoom_box_rotation = 0;
                zoom_box_skew = zoom_box_rotation;
                zby = 0;
                zbx = zby;
                find_special_colors();
                boxcolor = g_color_bright;
                if (evolving&1)
                {
                    // set screen view params back (previously changed to allow full screen saves in viewwindow mode)
                    int grout = !((evolving & NOGROUT) / NOGROUT);
                    sxoffs = px * (int)(x_size_d+1+grout);
                    syoffs = py * (int)(y_size_d+1+grout);
                    SetupParamBox();
                    drawparmbox(0);
                }
                moveboxf(0.0, 0.0); // force scrolling
            }
            else
                resizebox(0 - key_count(FIK_PAGE_UP));
        }
        break;
    case FIK_PAGE_DOWN:              // page down
        if (boxcount)
        {
            if (zoom_box_width >= .999 && zoom_box_height >= 0.999)
            { // end zoombox
                zoom_box_width = 0;
                if (evolving&1)
                {
                    drawparmbox(1); // clear boxes off screen
                    ReleaseParamBox();
                }
            }
            else
                resizebox(key_count(FIK_PAGE_DOWN));
        }
        break;
    case FIK_CTL_MINUS:              // Ctrl-kpad-
        if (boxcount && (curfractalspecific->flags & NOROTATE) == 0)
            zoom_box_rotation += key_count(FIK_CTL_MINUS);
        break;
    case FIK_CTL_PLUS:               // Ctrl-kpad+
        if (boxcount && (curfractalspecific->flags & NOROTATE) == 0)
            zoom_box_rotation -= key_count(FIK_CTL_PLUS);
        break;
    case FIK_CTL_INSERT:             // Ctrl-ins
        boxcolor += key_count(FIK_CTL_INSERT);
        break;
    case FIK_CTL_DEL:                // Ctrl-del
        boxcolor -= key_count(FIK_CTL_DEL);
        break;

    /* grabbed a couple of video mode keys, user can change to these using
        delete and the menu if necessary */

    case FIK_F2: // halve mutation params and regen
        evolve_max_random_mutation = evolve_max_random_mutation / 2;
        evolve_x_parameter_range = evolve_x_parameter_range / 2;
        evolve_new_x_parameter_offset = evolve_x_parameter_offset + evolve_x_parameter_range / 2;
        evolve_y_parameter_range = evolve_y_parameter_range / 2;
        evolve_new_y_parameter_offset = evolve_y_parameter_offset + evolve_y_parameter_range / 2;
        *kbdmore = false;
        calc_status = calc_status_value::PARAMS_CHANGED;
        break;

    case FIK_F3: //double mutation parameters and regenerate
    {
        double centerx, centery;
        evolve_max_random_mutation = evolve_max_random_mutation * 2;
        centerx = evolve_x_parameter_offset + evolve_x_parameter_range / 2;
        evolve_x_parameter_range = evolve_x_parameter_range * 2;
        evolve_new_x_parameter_offset = centerx - evolve_x_parameter_range / 2;
        centery = evolve_y_parameter_offset + evolve_y_parameter_range / 2;
        evolve_y_parameter_range = evolve_y_parameter_range * 2;
        evolve_new_y_parameter_offset = centery - evolve_y_parameter_range / 2;
        *kbdmore = false;
        calc_status = calc_status_value::PARAMS_CHANGED;
        break;
    }

    case FIK_F4: //decrement  gridsize and regen
        if (evolve_image_grid_size > 3)
        {
            evolve_image_grid_size = evolve_image_grid_size - 2;  // evolve_image_grid_size must have odd value only
            *kbdmore = false;
            calc_status = calc_status_value::PARAMS_CHANGED;
        }
        break;

    case FIK_F5: // increment gridsize and regen
        if (evolve_image_grid_size < (sxdots / (MINPIXELS << 1)))
        {
            evolve_image_grid_size = evolve_image_grid_size + 2;
            *kbdmore = false;
            calc_status = calc_status_value::PARAMS_CHANGED;
        }
        break;

    case FIK_F6: /* toggle all variables selected for random variation to center weighted variation and vice versa */
        for (auto &elem : gene_bank)
        {
            if (elem.mutate == variations::RANDOM)
            {
                elem.mutate = variations::WEIGHTED_RANDOM;
                continue;
            }
            if (elem.mutate == variations::WEIGHTED_RANDOM)
                elem.mutate = variations::RANDOM;
        }
        *kbdmore = false;
        calc_status = calc_status_value::PARAMS_CHANGED;
        break;

    case FIK_ALT_1: // alt + number keys set mutation level
    case FIK_ALT_2:
    case FIK_ALT_3:
    case FIK_ALT_4:
    case FIK_ALT_5:
    case FIK_ALT_6:
    case FIK_ALT_7:
        set_mutation_level(*kbdchar-1119);
        param_history(1); // restore old history
        *kbdmore = false;
        calc_status = calc_status_value::PARAMS_CHANGED;
        break;

    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
        set_mutation_level(*kbdchar-(int)'0');
        param_history(1); // restore old history
        *kbdmore = false;
        calc_status = calc_status_value::PARAMS_CHANGED;
        break;

    case '0': // mutation level 0 == turn off evolving
        evolving = 0;
        viewwindow = false;
        *kbdmore = false;
        calc_status = calc_status_value::PARAMS_CHANGED;
        break;

    case FIK_DELETE:         // select video mode from list
        driver_stack_screen();
        *kbdchar = select_video_mode(g_adapter);
        if (check_vidmode_key(0, *kbdchar) >= 0)  // picked a new mode?
            driver_discard_screen();
        else
            driver_unstack_screen();
        // fall through

    default:             // other (maybe valid Fn key
        k = check_vidmode_key(0, *kbdchar);
        if (k >= 0)
        {
            g_adapter = k;
            if (g_video_table[g_adapter].colors != colors)
                savedac = 0;
            calc_status = calc_status_value::PARAMS_CHANGED;
            *kbdmore = false;
            return main_state::CONTINUE;
        }
        break;
    }                            // end of the big evolver switch
    return main_state::NOTHING;
}

static int call_line3d(BYTE *pixels, int linelen)
{
    // this routine exists because line3d might be in an overlay
    return line3d(pixels, linelen);
}

// do all pending movement at once for smooth mouse diagonal moves
static void move_zoombox(int keynum)
{
    int vertical, horizontal, getmore;
    horizontal = 0;
    vertical = horizontal;
    getmore = 1;
    while (getmore)
    {
        switch (keynum)
        {
        case FIK_LEFT_ARROW:               // cursor left
            --horizontal;
            break;
        case FIK_RIGHT_ARROW:              // cursor right
            ++horizontal;
            break;
        case FIK_UP_ARROW:                 // cursor up
            --vertical;
            break;
        case FIK_DOWN_ARROW:               // cursor down
            ++vertical;
            break;
        case FIK_CTL_LEFT_ARROW:             // Ctrl-cursor left
            horizontal -= 8;
            break;
        case FIK_CTL_RIGHT_ARROW:             // Ctrl-cursor right
            horizontal += 8;
            break;
        case FIK_CTL_UP_ARROW:               // Ctrl-cursor up
            vertical -= 8;
            break;
        case FIK_CTL_DOWN_ARROW:             // Ctrl-cursor down
            vertical += 8;
            break;                      // += 8 needed by VESA scrolling
        default:
            getmore = 0;
        }
        if (getmore)
        {
            if (getmore == 2)              // eat last key used
                driver_get_key();
            getmore = 2;
            keynum = driver_key_pressed();         // next pending key
        }
    }
    if (boxcount)
    {
        moveboxf((double)horizontal/x_size_d, (double)vertical/y_size_d);
    }
#ifndef XFRACT
    else                                 // if no zoombox, scroll by arrows
        scroll_relative(horizontal, vertical);
#endif
}

// displays differences between current image file and new image
static FILE *cmp_fp;
static int errcount;
int cmp_line(BYTE *pixels, int linelen)
{
    int row;
    int oldcolor;
    row = g_row_count++;
    if (row == 0)
    {
        errcount = 0;
        cmp_fp = dir_fopen(workdir.c_str(), "cmperr", (init_batch != batch_modes::NONE) ? "a" : "w");
        outln_cleanup = cmp_line_cleanup;
    }
    if (pot16bit)
    {                                   // 16 bit info, ignore odd numbered rows
        if ((row & 1) != 0)
            return 0;
        row >>= 1;
    }
    for (int col = 0; col < linelen; col++)
    {
        oldcolor = getcolor(col, row);
        if (oldcolor == (int)pixels[col])
            putcolor(col, row, 0);
        else
        {
            if (oldcolor == 0)
                putcolor(col, row, 1);
            ++errcount;
            if (init_batch == batch_modes::NONE)
                fprintf(cmp_fp, "#%5d col %3d row %3d old %3d new %3d\n",
                        errcount, col, row, oldcolor, pixels[col]);
        }
    }
    return 0;
}

static void cmp_line_cleanup()
{
    char *timestring;
    time_t ltime;
    if (init_batch != batch_modes::NONE)
    {
        time(&ltime);
        timestring = ctime(&ltime);
        timestring[24] = 0; //clobber newline in time string
        fprintf(cmp_fp, "%s compare to %s has %5d errs\n",
                timestring, readname.c_str(), errcount);
    }
    fclose(cmp_fp);
}

void clear_zoombox()
{
    zoom_box_width = 0;
    drawbox(false);
    reset_zoom_corners();
}

void reset_zoom_corners()
{
    xxmin = sxmin;
    xxmax = sxmax;
    xx3rd = sx3rd;
    yymax = symax;
    yymin = symin;
    yy3rd = sy3rd;
    if (bf_math != bf_math_type::NONE)
    {
        copy_bf(bfxmin, bfsxmin);
        copy_bf(bfxmax, bfsxmax);
        copy_bf(bfymin, bfsymin);
        copy_bf(bfymax, bfsymax);
        copy_bf(bfx3rd, bfsx3rd);
        copy_bf(bfy3rd, bfsy3rd);
    }
}

// read keystrokes while = specified key, return 1+count;
// used to catch up when moving zoombox is slower than keyboard
int key_count(int keynum)
{   int ctr;
    ctr = 1;
    while (driver_key_pressed() == keynum)
    {
        driver_get_key();
        ++ctr;
    }
    return ctr;
}

static std::vector<HISTORY> history;
int maxhistory = 10;

void history_init()
{
    history.resize(maxhistory);
}

static void save_history_info()
{
    if (maxhistory <= 0 || bf_math != bf_math_type::NONE)
        return;
    HISTORY last = history[saveptr];

    HISTORY current;
    memset((void *)&current, 0, sizeof(HISTORY));
    current.fractal_type         = (short)fractype                  ;
    current.xmin                 = xxmin                     ;
    current.xmax                 = xxmax                     ;
    current.ymin                 = yymin                     ;
    current.ymax                 = yymax                     ;
    current.creal                = param[0]                  ;
    current.cimag                = param[1]                  ;
    current.dparm3               = param[2]                  ;
    current.dparm4               = param[3]                  ;
    current.dparm5               = param[4]                  ;
    current.dparm6               = param[5]                  ;
    current.dparm7               = param[6]                  ;
    current.dparm8               = param[7]                  ;
    current.dparm9               = param[8]                  ;
    current.dparm10              = param[9]                  ;
    current.fillcolor            = (short)fillcolor                 ;
    current.potential[0]         = potparam[0]               ;
    current.potential[1]         = potparam[1]               ;
    current.potential[2]         = potparam[2]               ;
    current.rflag                = (short) (rflag ? 1 : 0);
    current.rseed                = (short)rseed                     ;
    current.inside               = (short)inside                    ;
    current.logmap               = LogFlag                   ;
    current.invert[0]            = inversion[0]              ;
    current.invert[1]            = inversion[1]              ;
    current.invert[2]            = inversion[2]              ;
    current.decomp               = (short)decomp[0];                ;
    current.biomorph             = (short)biomorph                  ;
    current.symmetry             = (short)forcesymmetry             ;
    current.init3d[0]            = (short)init3d[0]                 ;
    current.init3d[1]            = (short)init3d[1]                 ;
    current.init3d[2]            = (short)init3d[2]                 ;
    current.init3d[3]            = (short)init3d[3]                 ;
    current.init3d[4]            = (short)init3d[4]                 ;
    current.init3d[5]            = (short)init3d[5]                 ;
    current.init3d[6]            = (short)init3d[6]                 ;
    current.init3d[7]            = (short)init3d[7]                 ;
    current.init3d[8]            = (short)init3d[8]                 ;
    current.init3d[9]            = (short)init3d[9]                 ;
    current.init3d[10]           = (short)init3d[10]               ;
    current.init3d[11]           = (short)init3d[12]               ;
    current.init3d[12]           = (short)init3d[13]               ;
    current.init3d[13]           = (short)init3d[14]               ;
    current.init3d[14]           = (short)init3d[15]               ;
    current.init3d[15]           = (short)init3d[16]               ;
    current.previewfactor        = (short)previewfactor             ;
    current.xtrans               = (short)xtrans                    ;
    current.ytrans               = (short)ytrans                    ;
    current.red_crop_left        = (short)red_crop_left             ;
    current.red_crop_right       = (short)red_crop_right            ;
    current.blue_crop_left       = (short)blue_crop_left            ;
    current.blue_crop_right      = (short)blue_crop_right           ;
    current.red_bright           = (short)red_bright                ;
    current.blue_bright          = (short)blue_bright               ;
    current.xadjust              = (short)xadjust                   ;
    current.yadjust              = (short)yadjust                   ;
    current.eyeseparation        = (short)g_eye_separation             ;
    current.glassestype          = (short)g_glasses_type               ;
    current.outside              = (short)outside                   ;
    current.x3rd                 = xx3rd                     ;
    current.y3rd                 = yy3rd                     ;
    current.stdcalcmode          = usr_stdcalcmode               ;
    current.three_pass           = three_pass ? 1 : 0;
    current.stoppass             = (short)stoppass;
    current.distest              = distest                   ;
    current.trigndx[0]           = static_cast<BYTE>(trigndx[0]);
    current.trigndx[1]           = static_cast<BYTE>(trigndx[1]);
    current.trigndx[2]           = static_cast<BYTE>(trigndx[2]);
    current.trigndx[3]           = static_cast<BYTE>(trigndx[3]);
    current.finattract           = (short) (finattract ? 1 : 0);
    current.initorbit[0]         = initorbit.x               ;
    current.initorbit[1]         = initorbit.y               ;
    current.useinitorbit         = useinitorbit              ;
    current.periodicity          = (short)periodicitycheck          ;
    current.pot16bit             = (short) (disk16bit ? 1 : 0);
    current.release              = (short)g_release                   ;
    current.save_release         = (short)save_release              ;
    current.display_3d           = display_3d;
    current.ambient              = (short)Ambient                   ;
    current.randomize            = (short)RANDOMIZE                 ;
    current.haze                 = (short)haze                      ;
    current.transparent[0]       = (short)transparent[0]            ;
    current.transparent[1]       = (short)transparent[1]            ;
    current.rotate_lo            = (short)rotate_lo                 ;
    current.rotate_hi            = (short)rotate_hi                 ;
    current.distestwidth         = (short)distestwidth              ;
    current.mxmaxfp              = mxmaxfp                   ;
    current.mxminfp              = mxminfp                   ;
    current.mymaxfp              = mymaxfp                   ;
    current.myminfp              = myminfp                   ;
    current.zdots                = (short)zdots                         ;
    current.originfp             = originfp                  ;
    current.depthfp              = depthfp                      ;
    current.heightfp             = heightfp                  ;
    current.widthfp              = widthfp                      ;
    current.distfp               = distfp                       ;
    current.eyesfp               = eyesfp                       ;
    current.orbittype            = (short)neworbittype              ;
    current.juli3Dmode           = (short)juli3Dmode                ;
    current.maxfn                = maxfn                     ;
    current.major_method         = (short)major_method              ;
    current.minor_method         = (short)minor_method              ;
    current.bailout              = bailout                   ;
    current.bailoutest           = (short)bailoutest                ;
    current.iterations           = maxit                     ;
    current.old_demm_colors      = (short) (old_demm_colors ? 1 : 0);
    current.logcalc              = (short)Log_Fly_Calc;
    current.ismand               = (short) (ismand ? 1 : 0);
    current.closeprox            = closeprox;
    current.nobof                = (short)nobof;
    current.orbit_delay          = (short)orbit_delay;
    current.orbit_interval       = orbit_interval;
    current.oxmin                = oxmin;
    current.oxmax                = oxmax;
    current.oymin                = oymin;
    current.oymax                = oymax;
    current.ox3rd                = ox3rd;
    current.oy3rd                = oy3rd;
    current.keep_scrn_coords     = (short) (keep_scrn_coords ? 1 : 0);
    current.drawmode             = drawmode;
    memcpy(current.dac, g_dac_box, 256*3);
    switch (fractype)
    {
    case fractal_type::FORMULA:
    case fractal_type::FFORMULA:
        strncpy(current.filename, FormFileName.c_str(), FILE_MAX_PATH);
        strncpy(current.itemname, FormName.c_str(), ITEMNAMELEN+1);
        break;
    case fractal_type::IFS:
    case fractal_type::IFS3D:
        strncpy(current.filename, IFSFileName.c_str(), FILE_MAX_PATH);
        strncpy(current.itemname, IFSName.c_str(), ITEMNAMELEN+1);
        break;
    case fractal_type::LSYSTEM:
        strncpy(current.filename, LFileName.c_str(), FILE_MAX_PATH);
        strncpy(current.itemname, LName.c_str(), ITEMNAMELEN+1);
        break;
    default:
        *(current.filename) = 0;
        *(current.itemname) = 0;
        break;
    }
    if (historyptr == -1)        // initialize the history file
    {
        for (int i = 0; i < maxhistory; i++)
            history[i] = current;
        historyflag = false;
        historyptr = 0;
        saveptr = 0;   // initialize history ptr
    }
    else if (historyflag)
        historyflag = false;            // coming from user history command, don't save
    else if (memcmp(&current, &last, sizeof(HISTORY)))
    {
        if (++saveptr >= maxhistory)  // back to beginning of circular buffer
            saveptr = 0;
        if (++historyptr >= maxhistory)  // move user pointer in parallel
            historyptr = 0;
        history[saveptr] = current;
    }
}

static void restore_history_info(int i)
{
    if (maxhistory <= 0 || bf_math != bf_math_type::NONE)
        return;
    HISTORY last = history[i];
    invert = 0;
    calc_status = calc_status_value::PARAMS_CHANGED;
    resuming = false;
    fractype              = static_cast<fractal_type>(last.fractal_type);
    xxmin                 = last.xmin           ;
    xxmax                 = last.xmax           ;
    yymin                 = last.ymin           ;
    yymax                 = last.ymax           ;
    param[0]              = last.creal          ;
    param[1]              = last.cimag          ;
    param[2]              = last.dparm3         ;
    param[3]              = last.dparm4         ;
    param[4]              = last.dparm5         ;
    param[5]              = last.dparm6         ;
    param[6]              = last.dparm7         ;
    param[7]              = last.dparm8         ;
    param[8]              = last.dparm9         ;
    param[9]              = last.dparm10        ;
    fillcolor             = last.fillcolor      ;
    potparam[0]           = last.potential[0]   ;
    potparam[1]           = last.potential[1]   ;
    potparam[2]           = last.potential[2]   ;
    rflag                 = last.rflag != 0;
    rseed                 = last.rseed          ;
    inside                = last.inside         ;
    LogFlag               = last.logmap         ;
    inversion[0]          = last.invert[0]      ;
    inversion[1]          = last.invert[1]      ;
    inversion[2]          = last.invert[2]      ;
    decomp[0]             = last.decomp         ;
    usr_biomorph          = last.biomorph       ;
    biomorph              = last.biomorph       ;
    forcesymmetry         = static_cast<symmetry_type>(last.symmetry);
    init3d[0]             = last.init3d[0]      ;
    init3d[1]             = last.init3d[1]      ;
    init3d[2]             = last.init3d[2]      ;
    init3d[3]             = last.init3d[3]      ;
    init3d[4]             = last.init3d[4]      ;
    init3d[5]             = last.init3d[5]      ;
    init3d[6]             = last.init3d[6]      ;
    init3d[7]             = last.init3d[7]      ;
    init3d[8]             = last.init3d[8]      ;
    init3d[9]             = last.init3d[9]      ;
    init3d[10]            = last.init3d[10]     ;
    init3d[12]            = last.init3d[11]     ;
    init3d[13]            = last.init3d[12]     ;
    init3d[14]            = last.init3d[13]     ;
    init3d[15]            = last.init3d[14]     ;
    init3d[16]            = last.init3d[15]     ;
    previewfactor         = last.previewfactor  ;
    xtrans                = last.xtrans         ;
    ytrans                = last.ytrans         ;
    red_crop_left         = last.red_crop_left  ;
    red_crop_right        = last.red_crop_right ;
    blue_crop_left        = last.blue_crop_left ;
    blue_crop_right       = last.blue_crop_right;
    red_bright            = last.red_bright     ;
    blue_bright           = last.blue_bright    ;
    xadjust               = last.xadjust        ;
    yadjust               = last.yadjust        ;
    g_eye_separation      = last.eyeseparation  ;
    g_glasses_type        = last.glassestype    ;
    outside               = last.outside        ;
    xx3rd                 = last.x3rd           ;
    yy3rd                 = last.y3rd           ;
    usr_stdcalcmode       = last.stdcalcmode    ;
    stdcalcmode           = last.stdcalcmode    ;
    three_pass            = last.three_pass != 0;
    stoppass              = last.stoppass       ;
    distest               = last.distest        ;
    usr_distest           = last.distest        ;
    trigndx[0]            = static_cast<trig_fn>(last.trigndx[0]);
    trigndx[1]            = static_cast<trig_fn>(last.trigndx[1]);
    trigndx[2]            = static_cast<trig_fn>(last.trigndx[2]);
    trigndx[3]            = static_cast<trig_fn>(last.trigndx[3]);
    finattract            = last.finattract != 0;
    initorbit.x           = last.initorbit[0]   ;
    initorbit.y           = last.initorbit[1]   ;
    useinitorbit          = last.useinitorbit   ;
    periodicitycheck      = last.periodicity    ;
    usr_periodicitycheck  = last.periodicity    ;
    disk16bit             = last.pot16bit != 0;
    g_release             = last.release        ;
    save_release          = last.save_release   ;
    display_3d = last.display_3d;
    Ambient               = last.ambient        ;
    RANDOMIZE             = last.randomize      ;
    haze                  = last.haze           ;
    transparent[0]        = last.transparent[0] ;
    transparent[1]        = last.transparent[1] ;
    rotate_lo             = last.rotate_lo      ;
    rotate_hi             = last.rotate_hi      ;
    distestwidth          = last.distestwidth   ;
    mxmaxfp               = last.mxmaxfp        ;
    mxminfp               = last.mxminfp        ;
    mymaxfp               = last.mymaxfp        ;
    myminfp               = last.myminfp        ;
    zdots                 = last.zdots          ;
    originfp              = last.originfp       ;
    depthfp               = last.depthfp        ;
    heightfp              = last.heightfp       ;
    widthfp               = last.widthfp        ;
    distfp                = last.distfp         ;
    eyesfp                = last.eyesfp         ;
    neworbittype          = static_cast<fractal_type>(last.orbittype);
    juli3Dmode            = last.juli3Dmode     ;
    maxfn                 = last.maxfn          ;
    major_method          = static_cast<Major>(last.major_method);
    minor_method          = static_cast<Minor>(last.minor_method);
    bailout               = last.bailout        ;
    bailoutest            = static_cast<bailouts>(last.bailoutest);
    maxit                 = last.iterations     ;
    old_demm_colors       = last.old_demm_colors != 0;
    curfractalspecific    = &fractalspecific[static_cast<int>(fractype)];
    potflag               = (potparam[0] != 0.0);
    if (inversion[0] != 0.0)
        invert = 3;
    Log_Fly_Calc = last.logcalc;
    ismand = last.ismand != 0;
    closeprox = last.closeprox;
    nobof = last.nobof != 0;
    orbit_delay = last.orbit_delay;
    orbit_interval = last.orbit_interval;
    oxmin = last.oxmin;
    oxmax = last.oxmax;
    oymin = last.oymin;
    oymax = last.oymax;
    ox3rd = last.ox3rd;
    oy3rd = last.oy3rd;
    keep_scrn_coords = last.keep_scrn_coords != 0;
    if (keep_scrn_coords)
        set_orbit_corners = true;
    drawmode = last.drawmode;
    usr_floatflag = curfractalspecific->isinteger ? false : true;
    memcpy(g_dac_box, last.dac, 256*3);
    memcpy(old_dac_box, last.dac, 256*3);
    if (map_specified)
    {
        for (int i = 0; i < 256; ++i)
        {
            map_clut[i][0] = last.dac[i][0];
            map_clut[i][1] = last.dac[i][1];
            map_clut[i][2] = last.dac[i][2];
        }
    }
    spindac(0, 1);
    if (fractype == fractal_type::JULIBROT || fractype == fractal_type::JULIBROTFP)
        savedac = 0;
    else
        savedac = 1;
    switch (fractype)
    {
    case fractal_type::FORMULA:
    case fractal_type::FFORMULA:
        FormFileName = last.filename;
        FormName = last.itemname;
        if (FormName.length() > ITEMNAMELEN)
        {
            FormName.resize(ITEMNAMELEN);
        }
        break;
    case fractal_type::IFS:
    case fractal_type::IFS3D:
        IFSFileName = last.filename;
        IFSName = last.itemname;
        if (IFSName.length() > ITEMNAMELEN)
        {
            IFSName.resize(ITEMNAMELEN);
        }
        break;
    case fractal_type::LSYSTEM:
        LFileName = last.filename;
        LName = last.itemname;
        if (LName.length() > ITEMNAMELEN)
        {
            LName.resize(ITEMNAMELEN);
        }
        break;
    default:
        break;
    }
}
