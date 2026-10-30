/* PDCurses */

/* Overlay images: bitmaps that are painted on top of the character cells.

   The application keeps a placeholder (e.g. a block of spaces) in the text
   and tells PDCurses which cells are covered by which image (see
   PDC_set_overlays). After the cells have been drawn (doupdate), the images
   are painted over them.

   PDCurses only redraws the cells that changed, so the image pixels stay on
   the screen surface until the cells below are redrawn. Therefore a run is
   (re)painted when
     - it is new (not painted by the previous update, e.g. after scrolling),
     - one of its cells was redrawn by PDC_transform_line.
   Runs that disappeared get their cells redrawn as text. */

#include "pdcsdl.h"

#include <stdlib.h>
#include <string.h>

typedef struct
{
    SDL_Surface *src;       /* the image, SDL_PIXELFORMAT_RGBA32 */
    SDL_Surface *scaled;    /* cache: the image scaled for the screen */
    int scaled_w, scaled_h; /* requested size of 'scaled' */
    bool opaque;
} PdcImage;

static PdcImage *images = NULL;
static int nimages = 0;

static PdcOverlayRun *runs = NULL;     /* runs to show */
static int nruns = 0, cap_runs = 0;
static PdcOverlayRun *painted = NULL;  /* runs painted by the last update */
static int npainted = 0, cap_painted = 0;

static unsigned char *touched = NULL;  /* cells redrawn since last paint */
static int touched_rows = 0, touched_cols = 0;

static PdcImage *_image(int handle)
{
    if (handle <= 0 || handle > nimages || !images[handle - 1].src)
        return NULL;

    return &images[handle - 1];
}

static bool _reserve(PdcOverlayRun **list, int *cap, int count)
{
    if (count > *cap)
    {
        int ncap = count < 64 ? 64 : count * 2;
        PdcOverlayRun *n = realloc(*list, ncap * sizeof(PdcOverlayRun));

        if (!n)
            return FALSE;

        *list = n;
        *cap = ncap;
    }

    return TRUE;
}

/* the touched map must match the current screen size */

static bool _ensure_touched(void)
{
    int rows, cols;

    if (!SP || !curscr)
        return FALSE;

    rows = SP->lines;
    cols = SP->cols;

    if (rows <= 0 || cols <= 0)
        return FALSE;

    if (rows != touched_rows || cols != touched_cols || !touched)
    {
        unsigned char *n = calloc((size_t)rows * cols, 1);

        if (!n)
            return FALSE;

        free(touched);
        touched = n;
        touched_rows = rows;
        touched_cols = cols;
    }

    return TRUE;
}

/* clamps a run to the screen; returns FALSE if nothing remains */

static bool _clamp(const PdcOverlayRun *r, int *row, int *col, int *n)
{
    int c0 = r->col, c1 = r->col + r->ncols;

    if (r->row < 0 || r->row >= touched_rows)
        return FALSE;

    if (c0 < 0)
        c0 = 0;
    if (c1 > touched_cols)
        c1 = touched_cols;
    if (c0 >= c1)
        return FALSE;

    *row = r->row;
    *col = c0;
    *n = c1 - c0;
    return TRUE;
}

static bool _same(const PdcOverlayRun *a, const PdcOverlayRun *b)
{
    return a->image == b->image && a->row == b->row && a->col == b->col &&
           a->ncols == b->ncols && a->place_x == b->place_x &&
           a->place_y == b->place_y && a->place_w == b->place_w &&
           a->place_h == b->place_h;
}

static bool _contains(const PdcOverlayRun *list, int count,
                      const PdcOverlayRun *r)
{
    int i;

    for (i = 0; i < count; i++)
        if (_same(&list[i], r))
            return TRUE;

    return FALSE;
}

static bool _is_touched(const PdcOverlayRun *r)
{
    int row, col, n, i;

    if (!_clamp(r, &row, &col, &n))
        return FALSE;

    for (i = 0; i < n; i++)
        if (touched[row * touched_cols + col + i])
            return TRUE;

    return FALSE;
}

/* redraws the text of the cells of a run (removes the image pixels) */

static void _redraw_cells(const PdcOverlayRun *r, bool only_untouched)
{
    int row, col, n, i;

    if (!_clamp(r, &row, &col, &n))
        return;

    for (i = 0; i < n; )
    {
        int len = 0;

        if (only_untouched && touched[row * touched_cols + col + i])
        {
            i++;
            continue;
        }

        while (i + len < n &&
               !(only_untouched && touched[row * touched_cols + col + i + len]))
            len++;

        PDC_transform_line(row, col + i, len, curscr->_y[row] + col + i);
        i += len;
    }
}

/* scaling ---------------------------------------------------------------- */

/* averages the source pixels covered by every destination pixel (used when
   shrinking, which is the common case) */

static void _scale_box(const SDL_Surface *src, SDL_Surface *dst)
{
    int sw = src->w, sh = src->h, dw = dst->w, dh = dst->h;
    int x, y;

    for (y = 0; y < dh; y++)
    {
        int y0 = (int)((long long)y * sh / dh);
        int y1 = (int)((long long)(y + 1) * sh / dh);
        Uint8 *out = (Uint8 *)dst->pixels + (size_t)y * dst->pitch;

        if (y1 <= y0)
            y1 = y0 + 1;

        for (x = 0; x < dw; x++)
        {
            int x0 = (int)((long long)x * sw / dw);
            int x1 = (int)((long long)(x + 1) * sw / dw);
            unsigned long sum[4] = {0, 0, 0, 0};
            unsigned long count;
            int sx, sy;

            if (x1 <= x0)
                x1 = x0 + 1;

            for (sy = y0; sy < y1; sy++)
            {
                const Uint8 *in = (const Uint8 *)src->pixels +
                                  (size_t)sy * src->pitch + (size_t)x0 * 4;

                for (sx = x0; sx < x1; sx++, in += 4)
                {
                    sum[0] += in[0];
                    sum[1] += in[1];
                    sum[2] += in[2];
                    sum[3] += in[3];
                }
            }

            count = (unsigned long)(x1 - x0) * (unsigned long)(y1 - y0);
            out[0] = (Uint8)(sum[0] / count);
            out[1] = (Uint8)(sum[1] / count);
            out[2] = (Uint8)(sum[2] / count);
            out[3] = (Uint8)(sum[3] / count);
            out += 4;
        }
    }
}

/* bilinear interpolation (used when enlarging) */

static void _scale_bilinear(const SDL_Surface *src, SDL_Surface *dst)
{
    int sw = src->w, sh = src->h, dw = dst->w, dh = dst->h;
    int x, y, c;

    for (y = 0; y < dh; y++)
    {
        float fy = ((float)y + 0.5f) * sh / dh - 0.5f;
        int y0, y1;
        float wy;
        Uint8 *out = (Uint8 *)dst->pixels + (size_t)y * dst->pitch;
        const Uint8 *row0, *row1;

        if (fy < 0)
            fy = 0;
        y0 = (int)fy;
        if (y0 > sh - 1)
            y0 = sh - 1;
        y1 = y0 + 1 < sh ? y0 + 1 : y0;
        wy = fy - (float)y0;

        row0 = (const Uint8 *)src->pixels + (size_t)y0 * src->pitch;
        row1 = (const Uint8 *)src->pixels + (size_t)y1 * src->pitch;

        for (x = 0; x < dw; x++)
        {
            float fx = ((float)x + 0.5f) * sw / dw - 0.5f;
            int x0, x1;
            float wx;

            if (fx < 0)
                fx = 0;
            x0 = (int)fx;
            if (x0 > sw - 1)
                x0 = sw - 1;
            x1 = x0 + 1 < sw ? x0 + 1 : x0;
            wx = fx - (float)x0;

            for (c = 0; c < 4; c++)
            {
                float top = row0[x0 * 4 + c] * (1 - wx) + row0[x1 * 4 + c] * wx;
                float bot = row1[x0 * 4 + c] * (1 - wx) + row1[x1 * 4 + c] * wx;
                float v = top * (1 - wy) + bot * wy + 0.5f;

                out[c] = (Uint8)(v < 0 ? 0 : (v > 255 ? 255 : v));
            }

            out += 4;
        }
    }
}

/* returns the image scaled to w x h pixels (cached) */

static SDL_Surface *_scaled(PdcImage *img, int w, int h)
{
    SDL_Surface *dst;

    if (img->scaled && img->scaled_w == w && img->scaled_h == h)
        return img->scaled;

    if (img->scaled)
    {
        SDL_FreeSurface(img->scaled);
        img->scaled = NULL;
    }

    if (w == img->src->w && h == img->src->h)
        dst = SDL_ConvertSurface(img->src, img->src->format, 0);
    else
    {
        dst = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32,
                                             SDL_PIXELFORMAT_RGBA32);
        if (!dst)
            return NULL;

        SDL_LockSurface(img->src);
        SDL_LockSurface(dst);

        if (w <= img->src->w && h <= img->src->h)
            _scale_box(img->src, dst);
        else
            _scale_bilinear(img->src, dst);

        SDL_UnlockSurface(dst);
        SDL_UnlockSurface(img->src);
    }

    if (!dst)
        return NULL;

    if (img->opaque && pdc_screen)
    {
        /* blitting without blending and conversion is a lot faster */

        SDL_Surface *conv = SDL_ConvertSurface(dst, pdc_screen->format, 0);

        if (conv)
        {
            SDL_FreeSurface(dst);
            dst = conv;
        }

        SDL_SetSurfaceBlendMode(dst, SDL_BLENDMODE_NONE);
    }
    else
        SDL_SetSurfaceBlendMode(dst, SDL_BLENDMODE_BLEND);

    img->scaled = dst;
    img->scaled_w = w;
    img->scaled_h = h;
    return dst;
}

/* painting --------------------------------------------------------------- */

static void _paint(const PdcOverlayRun *r)
{
    PdcImage *img = _image(r->image);
    SDL_Rect place, dest, clip, old_clip, cells;
    SDL_Surface *s;
    int row, col, n, iw, ih, dw, dh;

    if (!img || !pdc_screen || !_clamp(r, &row, &col, &n))
        return;

    if (r->place_w <= 0 || r->place_h <= 0)
        return;

    place.x = r->place_x * pdc_fwidth + pdc_xoffset;
    place.y = r->place_y * pdc_fheight + pdc_yoffset;
    place.w = r->place_w * pdc_fwidth;
    place.h = r->place_h * pdc_fheight;

    /* fit the image in the placement, keeping its aspect ratio */

    iw = img->src->w;
    ih = img->src->h;

    if ((long long)place.w * ih <= (long long)place.h * iw)
    {
        dw = place.w;
        dh = (int)((long long)ih * place.w / iw);
    }
    else
    {
        dh = place.h;
        dw = (int)((long long)iw * place.h / ih);
    }

    if (dw < 1)
        dw = 1;
    if (dh < 1)
        dh = 1;

    dest.x = place.x + (place.w - dw) / 2;
    dest.y = place.y + (place.h - dh) / 2;
    dest.w = dw;
    dest.h = dh;

    cells.x = col * pdc_fwidth + pdc_xoffset;
    cells.y = row * pdc_fheight + pdc_yoffset;
    cells.w = n * pdc_fwidth;
    cells.h = pdc_fheight;

    if (!SDL_IntersectRect(&cells, &dest, &clip))
        return;

    s = _scaled(img, dw, dh);
    if (!s)
        return;

    SDL_GetClipRect(pdc_screen, &old_clip);
    SDL_SetClipRect(pdc_screen, &clip);
    SDL_BlitSurface(s, NULL, pdc_screen, &dest);
    SDL_SetClipRect(pdc_screen, &old_clip);

    PDC_add_update_rect(cells);
}

void PDC_paint_overlays(void)
{
    int i;

    if (!nruns && !npainted)
        return;

    if (!_ensure_touched())
        return;

    /* runs that disappeared: show the text of their cells again */

    for (i = 0; i < npainted; i++)
        if (!_contains(runs, nruns, &painted[i]))
            _redraw_cells(&painted[i], TRUE);

    /* new runs and runs whose cells were redrawn */

    for (i = 0; i < nruns; i++)
    {
        const PdcOverlayRun *r = &runs[i];
        PdcImage *img = _image(r->image);

        if (!img)
            continue;

        if (_contains(painted, npainted, r) && !_is_touched(r))
            continue;

        /* images with transparency are blended over the cells, which must
           not contain image pixels from a previous paint */

        if (!img->opaque)
            _redraw_cells(r, TRUE);

        _paint(r);
    }

    if (_reserve(&painted, &cap_painted, nruns))
    {
        if (nruns)
            memcpy(painted, runs, nruns * sizeof(PdcOverlayRun));
        npainted = nruns;
    }
    else
        npainted = 0;

    memset(touched, 0, (size_t)touched_rows * touched_cols);
}

void PDC_overlay_touch(int lineno, int x, int len)
{
    if (!nruns && !npainted)
        return;

    if (!_ensure_touched() || lineno < 0 || lineno >= touched_rows)
        return;

    if (x < 0)
    {
        len += x;
        x = 0;
    }

    if (x + len > touched_cols)
        len = touched_cols - x;

    if (len > 0)
        memset(touched + lineno * touched_cols + x, 1, len);
}

/* public interface ------------------------------------------------------- */

int PDC_image_create(const void *rgba, int width, int height)
{
    const Uint8 *p = (const Uint8 *)rgba;
    SDL_Surface *s;
    bool opaque = TRUE;
    size_t i, total;
    int y, slot;

    if (!rgba || width <= 0 || height <= 0)
        return 0;

    s = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32,
                                       SDL_PIXELFORMAT_RGBA32);
    if (!s)
        return 0;

    SDL_LockSurface(s);
    for (y = 0; y < height; y++)
        memcpy((Uint8 *)s->pixels + (size_t)y * s->pitch,
               p + (size_t)y * width * 4, (size_t)width * 4);
    SDL_UnlockSurface(s);

    total = (size_t)width * height * 4;
    for (i = 3; i < total; i += 4)
        if (p[i] != 255)
        {
            opaque = FALSE;
            break;
        }

    for (slot = 0; slot < nimages; slot++)
        if (!images[slot].src)
            break;

    if (slot == nimages)
    {
        PdcImage *n = realloc(images, (nimages + 1) * sizeof(PdcImage));

        if (!n)
        {
            SDL_FreeSurface(s);
            return 0;
        }

        images = n;
        nimages++;
    }

    images[slot].src = s;
    images[slot].scaled = NULL;
    images[slot].scaled_w = images[slot].scaled_h = 0;
    images[slot].opaque = opaque;

    return slot + 1;
}

void PDC_image_destroy(int image)
{
    PdcImage *img = _image(image);
    int i, j;

    if (!img)
        return;

    SDL_FreeSurface(img->src);
    if (img->scaled)
        SDL_FreeSurface(img->scaled);

    img->src = NULL;
    img->scaled = NULL;

    /* don't show it anymore; the painted runs get their text back during
       the next update */

    for (i = 0, j = 0; i < nruns; i++)
        if (runs[i].image != image)
            runs[j++] = runs[i];
    nruns = j;

    /* the handle may be reused by a new image: the painted runs must never
       match the runs of that image */

    for (i = 0; i < npainted; i++)
        if (painted[i].image == image)
            painted[i].image = -image;
}

void PDC_set_overlays(const PdcOverlayRun *list, int count)
{
    int i;

    if (count < 0 || !_reserve(&runs, &cap_runs, count))
        count = 0;

    nruns = 0;
    for (i = 0; i < count; i++)
        if (_image(list[i].image) && list[i].ncols > 0)
            runs[nruns++] = list[i];
}

void PDC_get_cell_size(int *width, int *height)
{
    if (width)
        *width = pdc_fwidth;
    if (height)
        *height = pdc_fheight;
}
