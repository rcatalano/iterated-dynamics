#if !defined(FRAC_SUB_R_H)
#define FRAC_SUB_R_H

extern void calculate_fractal_initialize();
extern void adjust_corner();
extern void sleep_ms(long);
extern void reset_clock();
extern void plot_orbit_i(long, long, int);
extern void plot_orbit(double, double, int);
extern void orbit_scrub();
extern void get_julia_attractor(double, double);
extern int solid_guess_block_size();
extern void fractal_float_to_bf();
extern void adjust_corner_bf();

#endif