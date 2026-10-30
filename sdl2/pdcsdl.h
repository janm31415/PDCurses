/* PDCurses */

#include <SDL.h>
#ifdef PDC_WIDE
# include <SDL_ttf.h>
#endif

#include <curspriv.h>

#ifdef PDC_WIDE
PDCEX  TTF_Font *pdc_ttffont;
PDCEX  int pdc_font_size;
#endif
PDCEX  SDL_Window *pdc_window;
PDCEX  SDL_Surface *pdc_screen, *pdc_font, *pdc_icon, *pdc_back;
PDCEX  int pdc_sheight, pdc_swidth, pdc_yoffset, pdc_xoffset;

extern SDL_Surface *pdc_tileback;    /* used to regenerate the background
                                        of "transparent" cells */
extern SDL_Color pdc_color[PDC_MAXCOL];  /* colors for font palette */
extern Uint32 pdc_mapped[PDC_MAXCOL];    /* colors for FillRect(), as
                                            used in _highlight() */
                                            
extern int pdc_fheight, pdc_fwidth;  /* font height and width */
extern int pdc_fthick;               /* thickness for highlights and
                                        rendered ACS glyphs */
extern int pdc_flastc;               /* font palette's last color
                                        (treated as the foreground) */
extern bool pdc_own_window;          /* if pdc_window was not set
                                        before initscr(), PDCurses is
                                        responsible for (owns) it */

PDCEX  void PDC_update_rects(void);
PDCEX  void PDC_retile(void);

extern void PDC_pump_and_peep(void);
extern void PDC_blink_text(void);

/* Direct 24-bit RGB side buffer: cells drawn with COLOR_PAIR(PDC_RGB_PAIR)
   take their colours from pdc_rgb instead of the pair table. */

#define PDC_RGB_PAIR 255

typedef struct { Uint32 fg, bg; bool on; } PdcRgb;

extern PdcRgb *pdc_rgb;              /* pdc_rgb_rows * pdc_rgb_cols */
extern int pdc_rgb_rows, pdc_rgb_cols;

/* returns nonzero if the cell's RGB value changed (cell is invalidated) */
PDCEX int  PDC_set_cell_rgb(int y, int x, Uint32 fg, Uint32 bg);
PDCEX void PDC_free_rgb(void);

/* Overlay images (see pdcimg.c): bitmaps that are painted on top of the
   character cells, after the cells have been drawn.

   An image is placed on a rectangle of cells (place_*, may extend beyond
   the screen); it is scaled to fit that rectangle (keeping its aspect
   ratio) and centred in it. Only the cells of the visible runs are covered
   by the image: a run is a horizontal span of cells (row, col .. col+ncols)
   that the caller verified to still show the image's placeholder. */

typedef struct
{
    int image;                      /* handle from PDC_image_create */
    int row, col, ncols;            /* covered cells */
    int place_x, place_y;           /* placement of the whole image, in */
    int place_w, place_h;           /* cells */
} PdcOverlayRun;

/* creates an image from 8-bit RGBA pixels (copied); returns a handle > 0,
   or 0 on failure */
PDCEX int  PDC_image_create(const void *rgba, int width, int height);
PDCEX void PDC_image_destroy(int image);

/* replaces the list of visible overlay runs; they are (re)painted by the
   next PDC_doupdate */
PDCEX void PDC_set_overlays(const PdcOverlayRun *runs, int count);

/* size of a character cell in pixels */
PDCEX void PDC_get_cell_size(int *width, int *height);

/* internal: cells redrawn by PDC_transform_line lose their image pixels */
void PDC_overlay_touch(int lineno, int x, int len);
/* internal: paints the overlays, called by PDC_doupdate */
void PDC_paint_overlays(void);
/* internal: queues a rect for SDL_UpdateWindowSurfaceRects */
void PDC_add_update_rect(SDL_Rect rect);
