//#include <curses.h>
#include <string.h>

#include "port.h"
#include "prototyp.h"
#include "drivers.h"
/*
 * This file contains Unix versions of the routines in video.asm
 * Copyright 1992 Ken Shirriff
 */

extern unsigned char *xgetfont();
extern int startdisk();
extern int waitkeypressed(int);

//WINDOW *curwin;

bool fake_lut = false;
int dacnorm = 0;
int g_dac_count = 0;
int ShadowColors;
void (*dotwrite)(int, int, int);
// write-a-dot routine
int (*dotread)(int, int);   // read-a-dot routine
void (*linewrite)(int y, int x, int lastx, BYTE const *pixels);        // write-a-line routine
void (*lineread)(int y, int x, int lastx, BYTE *pixels);         // read-a-line routine

int videoflag = 0;      // special "your-own-video" flag

void (*swapsetup)() = nullptr;     // setfortext/graphics setup routine
int g_color_dark = 0;       // darkest color in palette
int g_color_bright = 0;     // brightest color in palette
int g_color_medium = 0;     /* nearest to medbright grey in palette
                   Zoom-Box values (2K x 2K screens max) */
int g_row_count = 0;        // row-counter for decoder and out_line
int video_type = 0;     /* actual video adapter type:
                   0  = type not yet determined
                   1  = Hercules
                   2  = CGA (assumed if nothing else)
                   3  = EGA
                   4  = MCGA
                   5  = VGA
                   6  = VESA (not yet checked)
                   11  = 8514/A (not yet checked)
                   12  = TIGA   (not yet checked)
                   13  = TARGA  (not yet checked)
                   100 = x monochrome
                   101 = x 256 colors
                 */
int svga_type = 0;      /*  (forced) SVGA type
                   1 = ahead "A" type
                   2 = ATI
                   3 = C&T
                   4 = Everex
                   5 = Genoa
                   6 = Ncr
                   7 = Oak-Tech
                   8 = Paradise
                   9 = Trident
                   10 = Tseng 3000
                   11 = Tseng 4000
                   12 = Video-7
                   13 = ahead "B" type
                   14 = "null" type (for testing only) */
int mode7text = 0;      // nonzero for egamono and hgc
int textaddr = 0xb800;      // b800 for mode 3, b000 for mode 7
int textsafe = 0;       /* 0 = default, runup chgs to 1
                   1 = yes
                   2 = no, use 640x200
                   3 = bios, yes plus use int 10h-1Ch
                   4 = save, save entire image */
int g_text_type = 1;        /* current mode's type of text:
                   0  = real text, mode 3 (or 7)
                   1  = 640x200x2, mode 6
                   2  = some other mode, graphics */
int g_text_row = 0;     // for putstring(-1,...)
int g_text_col = 0;     // for putstring(..,-1,...)
int g_text_rbase = 0;       // g_text_row is relative to this
int g_text_cbase = 0;       // g_text_col is relative to this

int g_vesa_detect = 1;      // set to 0 to disable VESA-detection
int g_video_start_x = 0;
int g_video_start_y = 0;
int g_vesa_x_res;
int g_vesa_y_res;
int chkd_vvs = 0;
int video_vram = 0;

void putstring(int row, int col, int attr, char const *msg);

/*

;       |--Adapter/Mode-Name------|-------Comments-----------|

;       |------INT 10H------|Dot-|--Resolution---|
;       |key|--AX---BX---CX---DX|Mode|--X-|--Y-|Color|
*/

VIDEOINFO x11_video_table[] = {
    {   "xfractint mode           ", "                         ",
        999, 0, 0, 0, 0, 19, 640, 480, 256
    },
};

void setforgraphics();

void
nullwrite(int a, int b, int c)
{
}

int
nullread(int a, int b)
{
    return 0;
}

void
setnullvideo()
{
    dotwrite = nullwrite;
    dotread = nullread;
}

#if 0
void
putprompt()
{
    wclear(curwin);       // ????
    putstring(0, 0, 0, "Press operation key, or <Esc> to return to Main Menu");
    wrefresh(curwin);
    return;
}
#endif

void
loaddac()
{
    readvideopalette();
}

/*
; **************** Function setvideomode(ax, bx, cx, dx) ****************
;       This function sets the (alphanumeric or graphic) video mode
;       of the monitor.   Called with the proper values of AX thru DX.
;       No returned values, as there is no particular standard to
;       adhere to in this case.

;       (SPECIAL "TWEAKED" VGA VALUES:  if AX==BX==CX==0, assume we have a
;       genuine VGA or register compatable adapter and program the registers
;       directly using the coded value in DX)

; Unix: We ignore ax,bx,cx,dx.  dotmode is the "mode" field in the video
; table.  We use mode 19 for the X window.
*/

#if 0
void
setvideomode(int ax, int bx, int cx, int dx)
{
    if (g_disk_flag)
    {
        enddisk();
    }
    if (videoflag)
    {
        endvideo();
        videoflag = 0;
    }
    g_good_mode = true;
    switch (dotmode)
    {
    case 0:         // text
        clear();
        /*
           touchwin(curwin);
         */
        wrefresh(curwin);
        break;
    case 11:
        startdisk();
        dotwrite = writedisk;
        dotread = readdisk;
        lineread = normalineread;
        linewrite = normaline;
        break;
    case 19:            // X window
        putprompt();
        dotwrite = writevideo;
        dotread = readvideo;
        lineread = readvideoline;
        linewrite = writevideoline;
        videoflag = 1;
        startvideo();
        setforgraphics();
        break;
    default:
        printf("Bad mode %d\n", dotmode);
        exit(-1);
    }
    if (dotmode != 0)
    {
        loaddac();
        g_and_color = colors - 1;
        boxcount = 0;
    }
    g_vesa_x_res = sxdots;
    g_vesa_y_res = sydots;
}
#endif


/*
; **************** Function getcolor(xdot, ydot) *******************

;       Return the color on the screen at the (xdot,ydot) point
*/
int
getcolor(int xdot, int ydot)
{
    int x1, y1;
    x1 = xdot + sxoffs;
    y1 = ydot + syoffs;
    if (x1 < 0 || y1 < 0 || x1 >= sxdots || y1 >= sydots)
        return 0;
    return dotread(x1, y1);
}

/*
; ************** Function putcolor_a(xdot, ydot, color) *******************

;       write the color on the screen at the (xdot,ydot) point
*/
void
putcolor_a(int xdot, int ydot, int color)
{
    dotwrite(xdot + sxoffs, ydot + syoffs, color & g_and_color);
}

/*
; **************** Function movecursor(row, col)  **********************

;       Move the cursor (called before printfs)
*/
#if 0
void
movecursor(int row, int col)
{
    if (row == -1)
    {
        row = g_text_row;
    }
    else
    {
        g_text_row = row;
    }
    if (col == -1)
    {
        col = g_text_col;
    }
    else
    {
        g_text_col = col;
    }
    wmove(curwin, row, col);
}
#endif

/*
; **************** Function keycursor(row, col)  **********************

;       Subroutine to wait cx ticks, or till keystroke pending
*/
#if 0
int
keycursor(int row, int col)
{
    movecursor(row, col);
    wrefresh(curwin);
    waitkeypressed(0);
    return getakey();
}
#endif

/*
; PUTSTR.asm puts a string directly to video display memory. Called from C by:
;    putstring(row, col, attr, string) where
;         row, col = row and column to start printing.
;         attr = color attribute.
;         string = far pointer to the null terminated string to print.
*/
#if 0
void
putstring(int row, int col, int attr, char const *msg)
{
    int so = 0;

    if (row != -1)
        g_text_row = row;
    if (col != -1)
        g_text_col = col;

    if (attr & INVERSE || attr & BRIGHT)
    {
        wstandout(curwin);
        so = 1;
    }
    wmove(curwin, g_text_row + g_text_rbase, g_text_col + g_text_cbase);
    while (1)
    {
        if (*msg == '\0')
            break;
        if (*msg == '\n')
        {
            g_text_col = 0;
            g_text_row++;
            wmove(curwin, g_text_row + g_text_rbase, g_text_col + g_text_cbase);
        }
        else
        {
            char const *ptr;
            ptr = strchr(msg, '\n');
            if (ptr == nullptr)
            {
                waddstr(curwin, msg);
                break;
            }
            else
            {
                waddch(curwin, *msg);
            }
        }
        msg++;
    }
    if (so)
    {
        wstandend(curwin);
    }

    wrefresh(curwin);
    fflush(stdout);
    getyx(curwin, g_text_row, g_text_col);
    g_text_row -= g_text_rbase;
    g_text_col -= g_text_cbase;
}
#endif

/*
; setattr(row, col, attr, count) where
;         row, col = row and column to start printing.
;         attr = color attribute.
;         count = number of characters to set
;         This routine works only in real color text mode.
*/
#if 0
void
setattr(int row, int col, int attr, int count)
{
    movecursor(row, col);
}
#endif

/*
; **************** Function home()  ********************************

;       Home the cursor (called before printfs)
*/
#if 0
void
home()
{
    wmove(curwin, 0, 0);
    g_text_row = 0;
    g_text_col = 0;
}
#endif

/*
; ************* Function scrollup(toprow, botrow) ******************

;       Scroll the screen up (from toprow to botrow)
*/
#if 0
void
scrollup(int top, int bot)
{
    wmove(curwin, top, 0);
    wdeleteln(curwin);
    wmove(curwin, bot, 0);
    winsertln(curwin);
    wrefresh(curwin);
}
#endif

/*
; *************** Function spindac(direction, rstep) ********************

;       Rotate the MCGA/VGA DAC in the (plus or minus) "direction"
;       in "rstep" increments - or, if "direction" is 0, just replace it.
*/
void
spindac(int dir, int inc)
{
    unsigned char tmp[3];
    unsigned char *dacbot;
    if (colors < 16)
        return;
    if (g_is_true_color && truemode)
        return;
    if (dir != 0 && rotate_lo < colors && rotate_lo < rotate_hi)
    {
        int top = rotate_hi > colors ? colors - 1 : rotate_hi;
        dacbot = (unsigned char *) g_dac_box + 3 * rotate_lo;
        int len = (top - rotate_lo) * 3 * sizeof(unsigned char);
        if (dir > 0)
        {
            for (int i = 0; i < inc; i++)
            {
                bcopy(dacbot, tmp, 3 * sizeof(unsigned char));
                bcopy(dacbot + 3 * sizeof(unsigned char), dacbot, len);
                bcopy(tmp, dacbot + len, 3 * sizeof(unsigned char));
            }
        }
        else
        {
            for (int i = 0; i < inc; i++)
            {
                bcopy(dacbot + len, tmp, 3 * sizeof(unsigned char));
                bcopy(dacbot, dacbot + 3 * sizeof(unsigned char), len);
                bcopy(tmp, dacbot, 3 * sizeof(unsigned char));
            }
        }
    }
    writevideopalette();
    driver_delay(colors - g_dac_count - 1);
}

/*
; ---- Help (Video) Support
; ********* Functions setfortext() and setforgraphics() ************

;       setfortext() resets the video for text mode and saves graphics data
;       setforgraphics() restores the graphics mode and data
;       setclear() clears the screen after setfortext()
*/
void
setfortext()
{
}

#if 0
void
setclear()
{
    wclear(curwin);
    wrefresh(curwin);
}
#endif

void
setforgraphics()
{
    startvideo();
    spindac(0, 1);
}

unsigned char *fontTab = nullptr;

/*
; ************** Function findfont(n) ******************************

;       findfont(0) returns pointer to 8x8 font table if it can
;                   find it, nullptr otherwise;
;                   nonzero parameter reserved for future use
*/
BYTE *
findfont(int fontparm)
{
    if (fontTab == nullptr)
    {
        fontTab = xgetfont();
    }
    return (BYTE *) fontTab;
}

/*
; ******************** Zoombox functions **************************
*/

/*
 * The IBM method is that boxx[],boxy[] is a set of locations, and boxvalues
 * is the values in these locations.
 * Instead of using this box save/restore technique, we'll put the corners
 * in boxx[0],boxy[0],1,2,3 and then use xor.
 */

void
dispbox()
{
    if (boxcount)
    {
        setlinemode(1);
        drawline(boxx[0], boxy[0], boxx[1], boxy[1]);
        drawline(boxx[1], boxy[1], boxx[2], boxy[2]);
        drawline(boxx[2], boxy[2], boxx[3], boxy[3]);
        drawline(boxx[3], boxy[3], boxx[0], boxy[0]);
        setlinemode(0);
        xsync();
    }
}

void
clearbox()
{
    dispbox();
}

/*
; Passing this routine 0 turns off shadow, nonzero turns it on.
*/
int
ShadowVideo(int on)
{
    return 0;
}

int
SetupShadowVideo()
{
    return 0;
}

/*
; *************** Function find_special_colors ********************

;       Find the darkest and brightest colors in palette, and a medium
;       color which is reasonably bright and reasonably grey.
*/
void
find_special_colors()
{
    int maxb = 0;
    int minb = 9999;
    int med = 0;
    int maxgun, mingun;
    int brt;

    g_color_dark = 0;
    g_color_medium = 7;
    g_color_bright = 15;

    if (colors == 2)
    {
        g_color_medium = 1;
        g_color_bright = 1;
        return;
    }

    if (!(g_got_real_dac || fake_lut))
        return;

    for (int i = 0; i < colors; i++)
    {
        brt = (int) g_dac_box[i][0] + (int) g_dac_box[i][1] + (int) g_dac_box[i][2];
        if (brt > maxb)
        {
            maxb = brt;
            g_color_bright = i;
        }
        if (brt < minb)
        {
            minb = brt;
            g_color_dark = i;
        }
        if (brt < 150 && brt > 80)
        {
            mingun = (int) g_dac_box[i][0];
            maxgun = mingun;
            if ((int) g_dac_box[i][1] > (int) g_dac_box[i][0])
            {
                maxgun = (int) g_dac_box[i][1];
            }
            else
            {
                mingun = (int) g_dac_box[i][1];
            }
            if ((int) g_dac_box[i][2] > maxgun)
            {
                maxgun = (int) g_dac_box[i][2];
            }
            if ((int) g_dac_box[i][2] < mingun)
            {
                mingun = (int) g_dac_box[i][2];
            }
            if (brt - (maxgun - mingun) / 2 > med)
            {
                g_color_medium = i;
                med = brt - (maxgun - mingun) / 2;
            }
        }
    }
}

/*
; *************** Functions get_a_char, put_a_char ********************

;       Get and put character and attribute at cursor
;       Hi nybble=character, low nybble attribute. Text mode only
*/
char
get_a_char()
{
    return (char) getakey();
}

void
put_a_char(int ch)
{
}

/*
; ***Function get_line(int row,int startcol,int stopcol, unsigned char *pixels)

;       This routine is a 'line' analog of 'getcolor()', and gets a segment
;       of a line from the screen and stores it in pixels[] at one byte per
;       pixel
;       Called by the GIF decoder
*/

void get_line(int row, int startcol, int stopcol, BYTE *pixels)
{
    if (startcol + sxoffs >= sxdots || row + syoffs >= sydots)
        return;
    lineread(row + syoffs, startcol + sxoffs, stopcol + sxoffs, pixels);
}

/*
; ***Function put_line(int row,int startcol,int stopcol, unsigned char *pixels)

;       This routine is a 'line' analog of 'putcolor()', and puts a segment
;       of a line from the screen and stores it in pixels[] at one byte per
;       pixel
;       Called by the GIF decoder
*/

void
put_line(int row, int startcol, int stopcol, BYTE const *pixels)
{
    if (startcol + sxoffs >= sxdots || row + syoffs > sydots)
        return;
    linewrite(row + syoffs, startcol + sxoffs, stopcol + sxoffs, pixels);
}

/*
; ***************Function out_line(pixels,linelen) *********************

;       This routine is a 'line' analog of 'putcolor()', and sends an
;       entire line of pixels to the screen (0 <= xdot < xdots) at a clip
;       Called by the GIF decoder
*/
int
out_line(BYTE *pixels, int linelen)
{
    if (g_row_count + syoffs >= sydots)
        return 0;
    linewrite(g_row_count + syoffs, sxoffs, linelen + sxoffs - 1, pixels);
    g_row_count++;
    return 0;
}
