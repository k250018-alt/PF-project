#include <gdk-pixbuf/gdk-pixbuf.h>
#include <glib.h>
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc < 3) {
        g_printerr("Usage: %s <input.png> <output.png>\n", argv[0]);
        return 1;
    }

    const char *in = argv[1];
    const char *out = argv[2];
    GError *error = NULL;

    g_type_init(); // safe to call even if deprecated on newer glib

    GdkPixbuf *pix = gdk_pixbuf_new_from_file(in, &error);
    if (!pix) {
        g_printerr("Failed to load %s: %s\n", in, error ? error->message : "unknown");
        if (error) g_error_free(error);
        return 2;
    }

    GdkPixbuf *rot = gdk_pixbuf_rotate_simple(pix, GDK_PIXBUF_ROTATE_CLOCKWISE);
    if (!rot) {
        g_printerr("Failed to rotate image\n");
        g_object_unref(pix);
        return 3;
    }

    if (!gdk_pixbuf_save(rot, out, "png", &error, NULL)) {
        g_printerr("Failed to save %s: %s\n", out, error ? error->message : "unknown");
        if (error) g_error_free(error);
        g_object_unref(pix);
        g_object_unref(rot);
        return 4;
    }

    g_print("Saved rotated image to %s\n", out);

    g_object_unref(pix);
    g_object_unref(rot);
    return 0;
}
