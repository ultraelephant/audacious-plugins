/*
 * cd-menu-items.c
 * Copyright 2009-2011 John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#include <libaudcore/audstrings.h>
#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>
#include <gtk/gtk.h>

#define N_ITEMS 1
#define N_MENUS 3

class NPFilter : public GeneralPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("Now Playing Filter"),
        PACKAGE
    };

    constexpr NPFilter () : GeneralPlugin (info, true) {}

    bool init ();
    void cleanup ();

    void * get_gtk_widget ();
};

EXPORT NPFilter aud_plugin_instance;

static constexpr const char * titles[N_ITEMS] = {N_("Hide")};

static constexpr AudMenuID menus[N_MENUS] = {
    AudMenuID::Main,
    AudMenuID::PlaylistAdd,
    AudMenuID::Playlist
};

static void hide_track () {aud_drct_pl_open ("cdda://"); }
static void (* funcs[N_ITEMS]) () = {hide_track};

bool NPFilter::init ()
{
    for (int m = 0; m < N_MENUS; m ++)
        for (int i = 0; i < N_ITEMS; i ++)
            aud_plugin_menu_add (menus[m], funcs[i], _(titles[i]), "media-optical");

    return true;
}

void NPFilter::cleanup ()
{
    for (int m = 0; m < N_MENUS; m ++)
        for (int i = 0; i < N_ITEMS; i ++)
            aud_plugin_menu_remove (menus[m], funcs[i]);
}

static GtkTextView * textview;
static GtkTextBuffer * textbuffer;

static GtkWidget * build_widget ()
{
    textview = (GtkTextView *) gtk_text_view_new ();
    gtk_text_view_set_cursor_visible (textview, false);
    gtk_text_view_set_left_margin (textview, 4);
    gtk_text_view_set_right_margin (textview, 4);
    gtk_text_view_set_wrap_mode (textview, GTK_WRAP_WORD);
    textbuffer = gtk_text_view_get_buffer (textview);

    GtkWidget * scrollview = gtk_scrolled_window_new (nullptr, nullptr);
    gtk_scrolled_window_set_shadow_type ((GtkScrolledWindow *) scrollview, GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy ((GtkScrolledWindow *) scrollview, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    GtkWidget * vbox = gtk_vbox_new (false, 6);

    gtk_container_add ((GtkContainer *) scrollview, (GtkWidget *) textview);
    gtk_box_pack_start ((GtkBox *) vbox, scrollview, true, true, 0);

    gtk_widget_show_all (vbox);

    gtk_text_buffer_create_tag (textbuffer, "weight_bold", "weight", PANGO_WEIGHT_BOLD, nullptr);
    gtk_text_buffer_create_tag (textbuffer, "size_x_large", "scale", PANGO_SCALE_X_LARGE, nullptr);
    gtk_text_buffer_create_tag (textbuffer, "style_italic", "style", PANGO_STYLE_ITALIC, nullptr);

    GtkWidget * hbox = gtk_hbox_new (false, 6);
    gtk_box_pack_start ((GtkBox *) vbox, hbox, false, false, 0);

    return vbox;
}

void * NPFilter::get_gtk_widget ()
{
    GtkWidget * vbox = build_widget ();

 //   g_signal_connect (vbox, "destroy", destroy_cb, nullptr);

    return vbox;
}