/* A minimal GdkPixbuf for librsvg on Syllable, which has no GTK+.
 *
 * librsvg 2.22 uses GdkPixbuf as a plain RGB(A) buffer: for filter results,
 * for rsvg_handle_get_pixbuf() and for embedded raster images.  This is just
 * that buffer, as a GObject (librsvg refs and unrefs it), with the accessors
 * librsvg calls.  Image loading (GdkPixbufLoader) is not provided: embedded
 * <image> elements are skipped (rsvg-image.c). */
#ifndef _GDK_PIXBUF_H_
#define _GDK_PIXBUF_H_

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef void (*GdkPixbufDestroyNotify) (guchar *pixels, gpointer data);

typedef struct _GdkPixbuf GdkPixbuf;

typedef enum {
    GDK_COLORSPACE_RGB
} GdkColorspace;

#define GDK_TYPE_PIXBUF     (gdk_pixbuf_get_type ())
#define GDK_PIXBUF(o)       (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_PIXBUF, GdkPixbuf))
#define GDK_IS_PIXBUF(o)    (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_PIXBUF))

GType gdk_pixbuf_get_type (void);

GdkPixbuf *gdk_pixbuf_new (GdkColorspace colorspace, gboolean has_alpha,
                           int bits_per_sample, int width, int height);
GdkPixbuf *gdk_pixbuf_new_from_data (const guchar *data, GdkColorspace colorspace,
                                     gboolean has_alpha, int bits_per_sample,
                                     int width, int height, int rowstride,
                                     GdkPixbufDestroyNotify destroy_fn,
                                     gpointer destroy_fn_data);

GdkColorspace gdk_pixbuf_get_colorspace (const GdkPixbuf *pixbuf);
int       gdk_pixbuf_get_n_channels (const GdkPixbuf *pixbuf);
gboolean  gdk_pixbuf_get_has_alpha (const GdkPixbuf *pixbuf);
int       gdk_pixbuf_get_bits_per_sample (const GdkPixbuf *pixbuf);
guchar   *gdk_pixbuf_get_pixels (const GdkPixbuf *pixbuf);
int       gdk_pixbuf_get_width (const GdkPixbuf *pixbuf);
int       gdk_pixbuf_get_height (const GdkPixbuf *pixbuf);
int       gdk_pixbuf_get_rowstride (const GdkPixbuf *pixbuf);

void gdk_pixbuf_copy_area (const GdkPixbuf *src_pixbuf, int src_x, int src_y,
                           int width, int height, GdkPixbuf *dest_pixbuf,
                           int dest_x, int dest_y);

G_END_DECLS

#endif
