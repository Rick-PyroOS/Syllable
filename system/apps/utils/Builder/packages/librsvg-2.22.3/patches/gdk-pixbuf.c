/* A minimal GdkPixbuf for librsvg on Syllable: see gdk-pixbuf.h */
#include <string.h>
#include "gdk-pixbuf.h"

struct _GdkPixbuf {
    GObject parent;
    GdkColorspace colorspace;
    gboolean has_alpha;
    int bits_per_sample, n_channels;
    int width, height, rowstride;
    guchar *pixels;
    GdkPixbufDestroyNotify destroy_fn;
    gpointer destroy_fn_data;
};

typedef struct {
    GObjectClass parent_class;
} GdkPixbufClass;

G_DEFINE_TYPE (GdkPixbuf, gdk_pixbuf, G_TYPE_OBJECT)

static void
gdk_pixbuf_finalize (GObject *object)
{
    GdkPixbuf *pixbuf = GDK_PIXBUF (object);

    if (pixbuf->destroy_fn)
        pixbuf->destroy_fn (pixbuf->pixels, pixbuf->destroy_fn_data);
    G_OBJECT_CLASS (gdk_pixbuf_parent_class)->finalize (object);
}

static void
gdk_pixbuf_class_init (GdkPixbufClass *klass)
{
    G_OBJECT_CLASS (klass)->finalize = gdk_pixbuf_finalize;
}

static void
gdk_pixbuf_init (GdkPixbuf *pixbuf)
{
}

static void
free_buffer (guchar *pixels, gpointer data)
{
    g_free (pixels);
}

GdkPixbuf *
gdk_pixbuf_new_from_data (const guchar *data, GdkColorspace colorspace,
                          gboolean has_alpha, int bits_per_sample,
                          int width, int height, int rowstride,
                          GdkPixbufDestroyNotify destroy_fn, gpointer destroy_fn_data)
{
    GdkPixbuf *pixbuf;

    g_return_val_if_fail (data != NULL, NULL);
    g_return_val_if_fail (colorspace == GDK_COLORSPACE_RGB, NULL);
    g_return_val_if_fail (bits_per_sample == 8, NULL);
    g_return_val_if_fail (width > 0 && height > 0, NULL);

    pixbuf = g_object_new (GDK_TYPE_PIXBUF, NULL);
    pixbuf->colorspace = colorspace;
    pixbuf->has_alpha = has_alpha ? TRUE : FALSE;
    pixbuf->bits_per_sample = bits_per_sample;
    pixbuf->n_channels = has_alpha ? 4 : 3;
    pixbuf->width = width;
    pixbuf->height = height;
    pixbuf->rowstride = rowstride;
    pixbuf->pixels = (guchar *) data;
    pixbuf->destroy_fn = destroy_fn;
    pixbuf->destroy_fn_data = destroy_fn_data;
    return pixbuf;
}

GdkPixbuf *
gdk_pixbuf_new (GdkColorspace colorspace, gboolean has_alpha,
                int bits_per_sample, int width, int height)
{
    int channels = has_alpha ? 4 : 3;
    int rowstride;
    guchar *pixels;

    g_return_val_if_fail (width > 0 && height > 0, NULL);
    g_return_val_if_fail (width <= (G_MAXINT - 3) / channels, NULL);

    /* rows 4-byte aligned, as GTK+'s */
    rowstride = (width * channels + 3) & ~3;
    if (height > G_MAXINT / rowstride)
        return NULL;
    pixels = g_try_malloc (height * rowstride);
    if (!pixels)
        return NULL;

    return gdk_pixbuf_new_from_data (pixels, colorspace, has_alpha, bits_per_sample,
                                     width, height, rowstride, free_buffer, NULL);
}

GdkColorspace
gdk_pixbuf_get_colorspace (const GdkPixbuf *pixbuf)
{
    g_return_val_if_fail (pixbuf != NULL, GDK_COLORSPACE_RGB);
    return pixbuf->colorspace;
}

int
gdk_pixbuf_get_n_channels (const GdkPixbuf *pixbuf)
{
    g_return_val_if_fail (pixbuf != NULL, -1);
    return pixbuf->n_channels;
}

gboolean
gdk_pixbuf_get_has_alpha (const GdkPixbuf *pixbuf)
{
    g_return_val_if_fail (pixbuf != NULL, FALSE);
    return pixbuf->has_alpha;
}

int
gdk_pixbuf_get_bits_per_sample (const GdkPixbuf *pixbuf)
{
    g_return_val_if_fail (pixbuf != NULL, -1);
    return pixbuf->bits_per_sample;
}

guchar *
gdk_pixbuf_get_pixels (const GdkPixbuf *pixbuf)
{
    g_return_val_if_fail (pixbuf != NULL, NULL);
    return pixbuf->pixels;
}

int
gdk_pixbuf_get_width (const GdkPixbuf *pixbuf)
{
    g_return_val_if_fail (pixbuf != NULL, -1);
    return pixbuf->width;
}

int
gdk_pixbuf_get_height (const GdkPixbuf *pixbuf)
{
    g_return_val_if_fail (pixbuf != NULL, -1);
    return pixbuf->height;
}

int
gdk_pixbuf_get_rowstride (const GdkPixbuf *pixbuf)
{
    g_return_val_if_fail (pixbuf != NULL, -1);
    return pixbuf->rowstride;
}

/* Copies a rectangle between two pixbufs of the same format; the areas may
 * overlap (memmove), as GTK+'s may when src and dest are the same pixbuf. */
void
gdk_pixbuf_copy_area (const GdkPixbuf *src, int src_x, int src_y, int width, int height,
                      GdkPixbuf *dest, int dest_x, int dest_y)
{
    int y, bpp, row;

    g_return_if_fail (src != NULL && dest != NULL);
    g_return_if_fail (src->n_channels == dest->n_channels);
    g_return_if_fail (src_x >= 0 && src_y >= 0 && dest_x >= 0 && dest_y >= 0);
    g_return_if_fail (src_x + width <= src->width && src_y + height <= src->height);
    g_return_if_fail (dest_x + width <= dest->width && dest_y + height <= dest->height);

    bpp = src->n_channels;
    row = width * bpp;
    if (src == dest && dest_y > src_y) {
        for (y = height - 1; y >= 0; y--)
            memmove (dest->pixels + (dest_y + y) * dest->rowstride + dest_x * bpp,
                     src->pixels + (src_y + y) * src->rowstride + src_x * bpp, row);
    } else {
        for (y = 0; y < height; y++)
            memmove (dest->pixels + (dest_y + y) * dest->rowstride + dest_x * bpp,
                     src->pixels + (src_y + y) * src->rowstride + src_x * bpp, row);
    }
}
