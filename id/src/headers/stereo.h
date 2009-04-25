#if !defined(STEREO_H)
#define STEREO_H

extern std::string g_stereo_map_name;

extern int auto_stereo();
extern int out_line_stereo(BYTE const *pixels, int linelen);
extern int get_random_dot_stereogram_parameters();

#endif