/*
    rotate.c - Routines that manipulate the Video DAC on VGA Adapters
*/
#include <float.h>
#include <string.h>
#include <time.h>

#include "port.h"
#include "prototyp.h"
#include "helpdefs.h"
#include "drivers.h"

// routines in this module

static void pauserotate();
static void set_palette(BYTE start[3], BYTE finish[3]);
static void set_palette2(BYTE start[3], BYTE finish[3]);
static void set_palette3(BYTE start[3], BYTE middle[3], BYTE finish[3]);

static bool paused = false;             // rotate-is-paused flag
static BYTE Red[3]    = {63, 0, 0};     // for shifted-Fkeys
static BYTE Green[3]  = { 0, 63, 0};
static BYTE Blue[3]   = { 0, 0, 63};
static BYTE Black[3]  = { 0, 0, 0};
static BYTE White[3]  = {63, 63, 63};
static BYTE Yellow[3] = {63, 63, 0};
static BYTE Brown[3]  = {31, 31, 0};

char mapmask[13] = {"*.map"};

BYTE g_dac_box[256][3];
BYTE old_dac_box[256][3];
bool g_dac_learn = false;
bool g_got_real_dac = false;            // true if loaddac has a dacbox

void rotate(int direction)      // rotate-the-palette routine
{
    int  kbdchar, last, next;
    int fkey, step, fstep, jstep, oldstep;
    int incr, fromred = 0, fromblue = 0, fromgreen = 0, tored = 0, toblue = 0, togreen = 0;
    int changecolor, changedirection;
    int old_help_mode;
    int rotate_max, rotate_size;

    static int fsteps[] = {2, 4, 8, 12, 16, 24, 32, 40, 54, 100}; // (for Fkeys)

#ifndef XFRACT
    if (!g_got_real_dac                 // ??? no DAC to rotate!
#else
    if (!(g_got_real_dac || fake_lut)   // ??? no DAC to rotate!
#endif
            || colors < 16)
    {                                   // strange things happen in 2x modes
        driver_buzzer(buzzer_codes::PROBLEM);
        return;
    }

    old_help_mode = help_mode;              // save the old help mode
    help_mode = HELPCYCLING;              // new help mode

    paused = false;                      // not paused
    fkey = 0;                            // no random coloring
    step = 1;
    oldstep = step;                      // single-step
    fstep = 1;
    changecolor = -1;                    // no color (rgb) to change
    changedirection = 0;                 // no color direction to change
    incr = 999;                          // ready to randomize
    srand((unsigned)time(nullptr));         // randomize things

    if (direction == 0)
    {                // firing up in paused mode?
        pauserotate();                    // then force a pause
        direction = 1;                    // and set a rotate direction
    }

    rotate_max = (rotate_hi < colors) ? rotate_hi : colors-1;
    rotate_size = rotate_max - rotate_lo + 1;
    last = rotate_max;                   // last box that was filled
    next = rotate_lo;                    // next box to be filled
    if (direction < 0)
    {
        last = rotate_lo;
        next = rotate_max;
    }

    bool more = true;
    while (more)
    {
        if (driver_diskp())
        {
            if (!paused)
                pauserotate();
        }
        else
            while (!driver_key_pressed())
            { // rotate until key hit, at least once so step=oldstep ok
                if (fkey > 0)
                {                // randomizing is on
                    for (int istep = 0; istep < step; istep++)
                    {
                        jstep = next + (istep * direction);
                        while (jstep < rotate_lo)
                            jstep += rotate_size;
                        while (jstep > rotate_max)
                            jstep -= rotate_size;
                        if (++incr > fstep)
                        {    // time to randomize
                            incr = 1;
                            fstep = ((fsteps[fkey-1]* (rand15() >> 8)) >> 6) + 1;
                            fromred   = g_dac_box[last][0];
                            fromgreen = g_dac_box[last][1];
                            fromblue  = g_dac_box[last][2];
                            tored     = rand15() >> 9;
                            togreen   = rand15() >> 9;
                            toblue    = rand15() >> 9;
                        }
                        g_dac_box[jstep][0] = (BYTE)(fromred   + (((tored    - fromred)*incr)/fstep));
                        g_dac_box[jstep][1] = (BYTE)(fromgreen + (((togreen - fromgreen)*incr)/fstep));
                        g_dac_box[jstep][2] = (BYTE)(fromblue  + (((toblue  - fromblue)*incr)/fstep));
                    }
                }
                if (step >= rotate_size)
                    step = oldstep;
                spindac(direction, step);
            }
        if (step >= rotate_size)
            step = oldstep;
        kbdchar = driver_get_key();
        if (paused && (kbdchar != ' '
                       && kbdchar != 'c'
                       && kbdchar != FIK_HOME
                       && kbdchar != 'C'))
            paused = false;                 // clear paused condition
        switch (kbdchar)
        {
        case '+':                      // '+' means rotate forward
        case FIK_RIGHT_ARROW:              // RightArrow = rotate fwd
            fkey = 0;
            direction = 1;
            last = rotate_max;
            next = rotate_lo;
            incr = 999;
            break;
        case '-':                      // '-' means rotate backward
        case FIK_LEFT_ARROW:               // LeftArrow = rotate bkwd
            fkey = 0;
            direction = -1;
            last = rotate_lo;
            next = rotate_max;
            incr = 999;
            break;
        case FIK_UP_ARROW:                 // UpArrow means speed up
            g_dac_learn = true;
            if (++g_dac_count >= colors)
                --g_dac_count;
            break;
        case FIK_DOWN_ARROW:               // DownArrow means slow down
            g_dac_learn = true;
            if (g_dac_count > 1)
                g_dac_count--;
            break;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            step = kbdchar - '0';   // change step-size
            if (step > rotate_size)
                step = rotate_size;
            break;
        case FIK_F1:                       // FIK_F1 - FIK_F10:
        case FIK_F2:                       // select a shading factor
        case FIK_F3:
        case FIK_F4:
        case FIK_F5:
        case FIK_F6:
        case FIK_F7:
        case FIK_F8:
        case FIK_F9:
        case FIK_F10:
#ifndef XFRACT
            fkey = kbdchar-1058;
#else
            switch (kbdchar)
            {
            case FIK_F1:
                fkey = 1;
                break;
            case FIK_F2:
                fkey = 2;
                break;
            case FIK_F3:
                fkey = 3;
                break;
            case FIK_F4:
                fkey = 4;
                break;
            case FIK_F5:
                fkey = 5;
                break;
            case FIK_F6:
                fkey = 6;
                break;
            case FIK_F7:
                fkey = 7;
                break;
            case FIK_F8:
                fkey = 8;
                break;
            case FIK_F9:
                fkey = 9;
                break;
            case FIK_F10:
                fkey = 10;
                break;
            }
#endif
            fstep = 1;
            incr = 999;
            break;
        case FIK_ENTER:                    // enter key: randomize all colors
        case FIK_ENTER_2:                  // also the Numeric-Keypad Enter
            fkey = rand15()/3277 + 1;
            fstep = 1;
            incr = 999;
            oldstep = step;
            step = rotate_size;
            break;
        case 'r':                      // color changes
            if (changecolor    == -1)
                changecolor = 0;
        case 'g':                      // color changes
            if (changecolor    == -1)
                changecolor = 1;
        case 'b':                      // color changes
            if (changecolor    == -1)
                changecolor = 2;
            if (changedirection == 0)
                changedirection = -1;
        case 'R':                      // color changes
            if (changecolor    == -1)
                changecolor = 0;
        case 'G':                      // color changes
            if (changecolor    == -1)
                changecolor = 1;
        case 'B':                      // color changes
            if (driver_diskp())
                break;
            if (changecolor    == -1)
                changecolor = 2;
            if (changedirection == 0)
                changedirection = 1;
            for (int i = 1; i < 256; i++)
            {
                g_dac_box[i][changecolor] = (BYTE)(g_dac_box[i][changecolor] + changedirection);
                if (g_dac_box[i][changecolor] == 64)
                    g_dac_box[i][changecolor] = 63;
                if (g_dac_box[i][changecolor] == 255)
                    g_dac_box[i][changecolor] = 0;
            }
            changecolor    = -1;        // clear flags for next time
            changedirection = 0;
            paused          = false;    // clear any pause
        case ' ':                      // use the spacebar as a "pause" toggle
        case 'c':                      // for completeness' sake, the 'c' too
        case 'C':
            pauserotate();              // pause
            break;
        case '>':                      // single-step
        case '.':
        case '<':
        case ',':
            if (kbdchar == '>' || kbdchar == '.')
            {
                direction = -1;
                last = rotate_lo;
                next = rotate_max;
                incr = 999;
            }
            else
            {
                direction = 1;
                last = rotate_max;
                next = rotate_lo;
                incr = 999;
            }
            fkey = 0;
            spindac(direction, 1);
            if (! paused)
                pauserotate();           // pause
            break;
        case 'd':                      // load colors from "default.map"
        case 'D':
            if (ValidateLuts("default"))
                break;
            fkey = 0;                   // disable random generation
            pauserotate();              // update palette and pause
            break;
        case 'a':                      // load colors from "altern.map"
        case 'A':
            if (ValidateLuts("altern"))
                break;
            fkey = 0;                   // disable random generation
            pauserotate();              // update palette and pause
            break;
        case 'l':                      // load colors from a specified map
#ifndef XFRACT // L is used for FIK_RIGHT_ARROW in Unix keyboard mapping
        case 'L':
#endif
            load_palette();
            fkey = 0;                   // disable random generation
            pauserotate();              // update palette and pause
            break;
        case 's':                      // save the palette
        case 'S':
            save_palette();
            fkey = 0;                   // disable random generation
            pauserotate();              // update palette and pause
            break;
        case FIK_ESC:                      // escape
            more = false;                   // time to bail out
            break;
        case FIK_HOME:                     // restore palette
            memcpy(g_dac_box, old_dac_box, 256*3);
            pauserotate();              // pause
            break;
        default:                       // maybe a new palette
            fkey = 0;                   // disable random generation
            if (kbdchar == FIK_SF1)
                set_palette(Black, White);
            if (kbdchar == FIK_SF2)
                set_palette(Red, Yellow);
            if (kbdchar == FIK_SF3)
                set_palette(Blue, Green);
            if (kbdchar == FIK_SF4)
                set_palette(Black, Yellow);
            if (kbdchar == FIK_SF5)
                set_palette(Black, Red);
            if (kbdchar == FIK_SF6)
                set_palette(Black, Blue);
            if (kbdchar == FIK_SF7)
                set_palette(Black, Green);
            if (kbdchar == FIK_SF8)
                set_palette(Blue, Yellow);
            if (kbdchar == FIK_SF9)
                set_palette(Red, Green);
            if (kbdchar == FIK_SF10)
                set_palette(Green, White);
            if (kbdchar == FIK_CTL_F1)
                set_palette2(Black, White);
            if (kbdchar == FIK_CTL_F2)
                set_palette2(Red, Yellow);
            if (kbdchar == FIK_CTL_F3)
                set_palette2(Blue, Green);
            if (kbdchar == FIK_CTL_F4)
                set_palette2(Black, Yellow);
            if (kbdchar == FIK_CTL_F5)
                set_palette2(Black, Red);
            if (kbdchar == FIK_CTL_F6)
                set_palette2(Black, Blue);
            if (kbdchar == FIK_CTL_F7)
                set_palette2(Black, Green);
            if (kbdchar == FIK_CTL_F8)
                set_palette2(Blue, Yellow);
            if (kbdchar == FIK_CTL_F9)
                set_palette2(Red, Green);
            if (kbdchar == FIK_CTL_F10)
                set_palette2(Green, White);
            if (kbdchar == FIK_ALT_F1)
                set_palette3(Blue, Green, Red);
            if (kbdchar == FIK_ALT_F2)
                set_palette3(Blue, Yellow, Red);
            if (kbdchar == FIK_ALT_F3)
                set_palette3(Red, White, Blue);
            if (kbdchar == FIK_ALT_F4)
                set_palette3(Red, Yellow, White);
            if (kbdchar == FIK_ALT_F5)
                set_palette3(Black, Brown, Yellow);
            if (kbdchar == FIK_ALT_F6)
                set_palette3(Blue, Brown, Green);
            if (kbdchar == FIK_ALT_F7)
                set_palette3(Blue, Green, Green);
            if (kbdchar == FIK_ALT_F8)
                set_palette3(Blue, Green, White);
            if (kbdchar == FIK_ALT_F9)
                set_palette3(Green, Green, White);
            if (kbdchar == FIK_ALT_F10)
                set_palette3(Red, Blue, White);
            pauserotate();  // update palette and pause
            break;
        }
    }

    help_mode = old_help_mode;              // return to previous help mode
}

static void pauserotate()               // pause-the-rotate routine
{
    if (paused)                          // if already paused , just clear
        paused = false;
    else
    {                               // else set border, wait for a key
        int olddaccount = g_dac_count;  // saved dac-count value goes here
        BYTE olddac0 = g_dac_box[0][0];
        BYTE olddac1 = g_dac_box[0][1];
        BYTE olddac2 = g_dac_box[0][2];
        g_dac_count = 256;
        g_dac_box[0][0] = 48;
        g_dac_box[0][1] = 48;
        g_dac_box[0][2] = 48;
        spindac(0, 1);                     // show white border
        if (driver_diskp())
        {
            dvid_status(100, " Paused in \"color cycling\" mode ");
        }
        driver_wait_key_pressed(0);                // wait for any key

        if (driver_diskp())
            dvid_status(0, "");
        g_dac_box[0][0] = olddac0;
        g_dac_box[0][1] = olddac1;
        g_dac_box[0][2] = olddac2;
        spindac(0, 1);                     // show black border
        g_dac_count = olddaccount;
        paused = true;
    }
}

static void set_palette(BYTE start[3], BYTE finish[3])
{
    g_dac_box[0][2] = 0;
    g_dac_box[0][1] = g_dac_box[0][2];
    g_dac_box[0][0] = g_dac_box[0][1];
    for (int i = 1; i <= 255; i++)                  // fill the palette
        for (int j = 0; j < 3; j++)
            g_dac_box[i][j] = (BYTE)((i*start[j] + (256-i)*finish[j])/255);
}

static void set_palette2(BYTE start[3], BYTE finish[3])
{
    g_dac_box[0][2] = 0;
    g_dac_box[0][1] = g_dac_box[0][2];
    g_dac_box[0][0] = g_dac_box[0][1];
    for (int i = 1; i <= 128; i++)
        for (int j = 0; j < 3; j++)
        {
            g_dac_box[i][j]     = (BYTE)((i*finish[j] + (128-i)*start[j])/128);
            g_dac_box[i+127][j] = (BYTE)((i*start[j]  + (128-i)*finish[j])/128);
        }
}

static void set_palette3(BYTE start[3], BYTE middle[3], BYTE finish[3])
{
    g_dac_box[0][2] = 0;
    g_dac_box[0][1] = g_dac_box[0][2];
    g_dac_box[0][0] = g_dac_box[0][1];
    for (int i = 1; i <= 85; i++)
        for (int j = 0; j < 3; j++)
        {
            g_dac_box[i][j]     = (BYTE)((i*middle[j] + (86-i)*start[j])/85);
            g_dac_box[i+85][j]  = (BYTE)((i*finish[j] + (86-i)*middle[j])/85);
            g_dac_box[i+170][j] = (BYTE)((i*start[j]  + (86-i)*finish[j])/85);
        }
}


void save_palette()
{
    char palname[FILE_MAX_PATH];
    FILE *dacfile;
    int i, old_help_mode;
    strcpy(palname, MAP_name.c_str());
    old_help_mode = help_mode;
    driver_stack_screen();
    char filename[256] = { 0 };
    help_mode = HELPCOLORMAP;
    i = field_prompt("Name of map file to write", nullptr, filename, 60, nullptr);
    driver_unstack_screen();
    if (i != -1 && filename[0])
    {
        if (strchr(filename, '.') == nullptr)
            strcat(filename, ".map");
        merge_pathnames(palname, filename, cmd_file::AT_AFTER_STARTUP);
        dacfile = fopen(palname, "w");
        if (dacfile == nullptr)
            driver_buzzer(buzzer_codes::PROBLEM);
        else
        {
#ifndef XFRACT
            for (i = 0; i < colors; i++)
#else
            for (i = 0; i < 256; i++)
#endif
                fprintf(dacfile, "%3d %3d %3d\n",
                        g_dac_box[i][0] << 2,
                        g_dac_box[i][1] << 2,
                        g_dac_box[i][2] << 2);
            memcpy(old_dac_box, g_dac_box, 256*3);
            colorstate = 2;
            colorfile = filename;
        }
        fclose(dacfile);
    }
    help_mode = old_help_mode;
}


bool load_palette()
{
    int old_help_mode;
    char filename[FILE_MAX_PATH];
    old_help_mode = help_mode;
    strcpy(filename, MAP_name.c_str());
    driver_stack_screen();
    help_mode = HELPCOLORMAP;
    bool i = getafilename("Select a MAP File", mapmask, filename);
    driver_unstack_screen();
    if (!i)
    {
        if (!ValidateLuts(filename))
            memcpy(old_dac_box, g_dac_box, 256*3);
        merge_pathnames(MAP_name, filename, cmd_file::AT_CMD_LINE);
    }
    help_mode = old_help_mode;
    return i;
}
