/* calmanfp.c
 * This file contains routines to replace calmanfp.asm.
 *
 * This file Copyright 1992 Ken Shirriff.  It may be used according to the
 * fractint license conditions, blah blah blah.
 */
#include <float.h>
#include "port.h"
#include "prototyp.h"
#include "fractype.h"
#include "drivers.h"

extern int atan_colors;
extern long firstsavedand;
extern int nextsavedincr;

static int inside_color, periodicity_color;

void calcmandfpasmstart()
{
    inside_color = (inside < COLOR_BLACK) ? maxit : inside;
    periodicity_color = (periodicitycheck < 0) ? 7 : inside_color;
    oldcoloriter = 0;
}

#define ABS(x) ((x) < 0?-(x):(x))

/* If USE_NEW is 1, the magnitude is used for periodicity checking instead
   of the x and y values.  This is experimental. */
#define USE_NEW 0

long calcmandfpasm()
{
    long cx;
    long savedand;
    int savedincr;
    long tmpfsd;
#if USE_NEW
    double x, y, x2, y2, xy, Cx, Cy, savedmag;
#else
    double x, y, x2, y2, xy, Cx, Cy, savedx, savedy;
#endif

    if (periodicitycheck == 0)
    {
        oldcoloriter = 0;      // don't check periodicity
    }
    else if (reset_periodicity)
    {
        oldcoloriter = maxit - 255;
    }

    tmpfsd = maxit - firstsavedand;
    if (oldcoloriter > tmpfsd) // this defeats checking periodicity immediately
    {
        oldcoloriter = tmpfsd; // but matches the code in StandardFractal()
    }

    // initparms
#if USE_NEW
    savedmag = 0;
#else
    savedx = 0;
    savedy = 0;
#endif
    orbit_ptr = 0;
    savedand = firstsavedand;
    savedincr = 1;             // start checking the very first time
    keyboard_check_interval--;                // Only check the keyboard sometimes
    if (keyboard_check_interval < 0)
    {
        int key;
        keyboard_check_interval = 1000;
        key = driver_key_pressed();
        if (key)
        {
            if (key == 'o' || key == 'O')
            {
                driver_get_key();
                show_orbit = !show_orbit;
            }
            else
            {
                coloriter = -1;
                return -1;
            }
        }
    }

    cx = maxit;
    if (fractype != fractal_type::JULIAFP && fractype != fractal_type::JULIA)
    {
        // Mandelbrot_87
        Cx = init.x;
        Cy = init.y;
        x = parm.x+Cx;
        y = parm.y+Cy;
    }
    else
    {
        // dojulia_87
        Cx = parm.x;
        Cy = parm.y;
        x = init.x;
        y = init.y;
        x2 = x*x;
        y2 = y*y;
        xy = x*y;
        x = x2-y2+Cx;
        y = 2*xy+Cy;
    }
    x2 = x*x;
    y2 = y*y;
    xy = x*y;

    // top_of_cs_loop_87
    while (--cx > 0)
    {
        x = x2-y2+Cx;
        y = 2*xy+Cy;
        x2 = x*x;
        y2 = y*y;
        xy = x*y;
        magnitude = x2+y2;

        if (magnitude >= rqlim)
        {
            goto over_bailout_87;
        }

        // no_save_new_xy_87
        if (cx < oldcoloriter)  // check periodicity
        {
            if (((maxit - cx) & savedand) == 0)
            {
#if USE_NEW
                savedmag = magnitude;
#else
                savedx = x;
                savedy = y;
#endif
                savedincr--;
                if (savedincr == 0)
                {
                    savedand = (savedand << 1) + 1;
                    savedincr = nextsavedincr;
                }
            }
            else
            {
#if USE_NEW
                if (ABS(magnitude-savedmag) < closenuff)
                {
#else
                if (ABS(savedx-x) < closenuff && ABS(savedy-y) < closenuff)
                {
#endif
                    //          oldcoloriter = 65535;
                    oldcoloriter = maxit;
                    realcoloriter = maxit;
                    keyboard_check_interval = keyboard_check_interval -(maxit-cx);
                    coloriter = periodicity_color;
                    goto pop_stack;
                }
            }
        }
        // no_periodicity_check_87
        if (show_orbit)
        {
            plot_orbit(x, y, -1);
        }
        // no_show_orbit_87
    } // while (--cx > 0)

    // reached maxit
    // check periodicity immediately next time, remember we count down from maxit
    oldcoloriter = maxit;
    keyboard_check_interval -= maxit;
    realcoloriter = maxit;
    coloriter = inside_color;

pop_stack:
    if (orbit_ptr)
    {
        scrub_orbit();
    }
    return coloriter;

over_bailout_87:
    if (outside <= REAL)
    {
        g_new.x = x;
        g_new.y = y;
    }
    if (cx-10 > 0)
    {
        oldcoloriter = cx-10;
    }
    else
    {
        oldcoloriter = 0;
    }
    realcoloriter = maxit-cx;
    coloriter = realcoloriter;
    if (coloriter == 0)
    {
        coloriter = 1;
    }
    keyboard_check_interval -= realcoloriter;
    if (outside == ITER)
    {
    }
    else if (outside > REAL)
    {
        coloriter = outside;
    }
    else
    {
        // special_outside
        if (outside == REAL)
        {
            coloriter += (long) g_new.x + 7;
        }
        else if (outside == IMAG)
        {
            coloriter += (long) g_new.y + 7;
        }
        else if (outside == MULT && g_new.y != 0.0)
        {
            coloriter = (long)((double) coloriter * (g_new.x/g_new.y));
        }
        else if (outside == SUM)
        {
            coloriter += (long)(g_new.x + g_new.y);
        }
        else if (outside == ATAN)
        {
            coloriter = (long) fabs(atan2(g_new.y, g_new.x)*atan_colors/PI);
        }
        // check_color
        if ((coloriter <= 0 || coloriter > maxit) && outside != FMOD)
        {
            if (save_release < 1961)
            {
                coloriter = 0;
            }
            else
            {
                coloriter = 1;
            }
        }
    }

    goto pop_stack;
}
