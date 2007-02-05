/*
 * mad plugin for audacious
 * Copyright (C) 2005-2007 William Pitcock, Yoshiki Yazawa
 *
 * Portions derived from xmms-mad:
 * Copyright (C) 2001-2002 Sam Clegg - See COPYING
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; under version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#ifdef HAVE_ASSERT_H
#include <assert.h>
#endif                          /* HAVE_ASSERT_H */

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif                          /* HAVE_SYS_TYPES_H */

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif                          /* HAVE_SYS_SOCKET_H */

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif                          /* HAVE_NETINET_IN_H */

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif                          /* HAVE_ARPA_INET_H */

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif                          /* HAVE_NETDB_H */

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif                          /* HAVE_SYS_STAT_H */

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif                          /* HAVE_SYS_TIME_H */

#include <fcntl.h>
#include <errno.h>
#include <audacious/util.h>

#include "input.h"
#include "replaygain.h"

#define DIR_SEPARATOR '/'
#define HEADER_SIZE 256
#define LINE_LENGTH 256

extern gboolean scan_file(struct mad_info_t *info, gboolean fast);

/**
 * init the mad_info_t struct.
 */
gboolean input_init(struct mad_info_t * info, const char *url)
{
#ifdef DEBUG
    g_message("f: input_init");
#endif
    memset(info, 0, sizeof(struct mad_info_t));

    info->fmt = FMT_S16_LE;
    info->channels = -1;
    info->mpeg_layer = -1;
    info->size = -1;
    info->freq = -1;
    info->seek = -1;
    info->duration = mad_timer_zero;
    info->pos = mad_timer_zero;
    info->url = g_strdup(url);
    info->current_frame = 0;
    info->frames = 0;
    info->bitrate = 0;
    info->vbr = 0;
    info->mode = 0;
    info->title = 0;
    info->offset = 0;

    info->replaygain_album_str = 0;
    info->replaygain_track_str = 0;
    info->replaygain_album_peak_str = 0;
    info->replaygain_track_peak_str = 0;
    info->mp3gain_undo_str = 0;
    info->mp3gain_minmax_str = 0;

    info->tuple = NULL;
    info->filename = g_strdup(url);

    info->infile = vfs_fopen(info->filename, "rb");
    if (info->infile == NULL)
        return FALSE;

    // obtain file size
    vfs_fseek(info->infile, 0, SEEK_END);
    info->size = vfs_ftell(info->infile);
    vfs_fseek(info->infile, 0, SEEK_SET);
    info->remote = info->size == 0 ? TRUE : FALSE;

#ifdef DEBUG
    g_message("i: info->size == %lu", info->size);
    g_message("e: input_init");
#endif
    return TRUE;
}

/* return length in letters */
size_t mad_ucs4len(id3_ucs4_t *ucs)
{
    id3_ucs4_t *ptr = ucs;
    size_t len = 0;

    while(*ptr++ != 0)
        len++;

    return len;
}

/* duplicate id3_ucs4_t string. new string will be terminated with 0. */
id3_ucs4_t *mad_ucs4dup(id3_ucs4_t *org)
{
    id3_ucs4_t *new = NULL;
    size_t len = mad_ucs4len(org);

    new = g_malloc0((len + 1) * sizeof(id3_ucs4_t));
    memcpy(new, org, len * sizeof(id3_ucs4_t));
    *(new + len) = 0; //terminate

    return new;
}

#define BYTES(x) ((x) * sizeof(id3_ucs4_t))

id3_ucs4_t *mad_parse_genre(const id3_ucs4_t *string)
{
    id3_ucs4_t *ret = NULL;
    id3_ucs4_t *tmp = NULL;
    id3_ucs4_t *genre = NULL;
    id3_ucs4_t *ptr, *end, *tail, *tp;
    size_t ret_len = 0; //num of ucs4 char!
    size_t tmp_len = 0;
    gboolean is_num = TRUE;

    tail = (id3_ucs4_t *)string + mad_ucs4len((id3_ucs4_t *)string);

    ret = g_malloc0(1024); // realloc() is too picky

    for(ptr = (id3_ucs4_t *)string; *ptr != 0 && ptr <= tail; ptr++) {
        if(*ptr == '(') {
            if(*(++ptr) == '(') { // escaped text like: ((something)
                for(end = ptr; *end != ')' && *end != 0;) { // copy "(something)"
                    end++;
                }
                end++; //include trailing ')'
//                ret = g_realloc(ret, BYTES(end - ptr + 1));
                memcpy(ret, ptr, BYTES(end - ptr));
                ret_len += (end - ptr);
                *(ret + ret_len) = 0; //terminate
                ptr = end + 1;
            }
            else {
                // reference to an id3v1 genre code
                for(end = ptr; *end != ')' && *end != 0;) {
                    end++;
                }

                tmp = g_malloc0(BYTES(end - ptr + 1));
                memcpy(tmp, ptr, BYTES(end - ptr));
                *(tmp + (end - ptr)) = 0; //terminate
                ptr += end - ptr;

                genre = (id3_ucs4_t *)id3_genre_name((const id3_ucs4_t *)tmp);

                g_free(tmp);
                tmp = NULL;

                tmp_len = mad_ucs4len(genre);

//                ret = g_realloc(ret, BYTES(ret_len + tmp_len + 1));
                memcpy(ret + BYTES(ret_len), genre, BYTES(tmp_len));

                ret_len += tmp_len;
                *(ret + ret_len) = 0; //terminate
            }
        }
        else {
            for(end = ptr; *end != '(' && *end != 0; ) {
                end++;
            }
            // scan string to determine whether a genre code number or not
            tp = ptr;
            is_num = TRUE;
            while(tp < end) {
                if(*tp < '0' || *tp > '9') { // anything else than number appears.
                    is_num = FALSE;
                    break;
                }
                tp++;
            }
            if(is_num) {
#ifdef DEBUG
                printf("is_num!\n");
#endif
                tmp = g_malloc0(BYTES(end - ptr + 1));
                memcpy(tmp, ptr, BYTES(end - ptr));
                *(tmp + (end - ptr)) = 0; //terminate
                ptr += end - ptr;

                genre = (id3_ucs4_t *)id3_genre_name((const id3_ucs4_t *)tmp);
#ifdef DEBUG
                printf("genre length = %d\n", mad_ucs4len(genre));
#endif
                g_free(tmp);
                tmp = NULL;

                tmp_len = mad_ucs4len(genre);

//                ret = g_realloc(ret, BYTES(ret_len + tmp_len + 1));
                memcpy(ret + BYTES(ret_len), genre, BYTES(tmp_len));

                ret_len += tmp_len;
                *(ret + ret_len) = 0; //terminate
            }
            else { // plain text
//                ret = g_realloc(ret, BYTES(end - ptr + 1));
#ifdef DEBUG
                printf("plain!\n");
                printf("ret_len = %d\n", ret_len);
#endif
                memcpy(ret + BYTES(ret_len), ptr, BYTES(end - ptr));
                ret_len = ret_len + (end - ptr);
                *(ret + ret_len) = 0; //terminate
                ptr += (end - ptr);
            }
        }
    }
    return ret;
}


gchar *input_id3_get_string(struct id3_tag * tag, char *frame_name)
{
    gchar *rtn;
    const id3_ucs4_t *string_const;
    id3_ucs4_t *string;
    struct id3_frame *frame;
    union id3_field *field;

    frame = id3_tag_findframe(tag, frame_name, 0);
    if (!frame)
        return NULL;

    if (frame_name == ID3_FRAME_COMMENT)
        field = id3_frame_field(frame, 3);
    else
        field = id3_frame_field(frame, 1);

    if (!field)
        return NULL;

    if (frame_name == ID3_FRAME_COMMENT)
        string_const = id3_field_getfullstring(field);
    else
        string_const = id3_field_getstrings(field, 0);

    if (!string_const)
        return NULL;

    string = mad_ucs4dup((id3_ucs4_t *)string_const);

    if (frame_name == ID3_FRAME_GENRE) {
        id3_ucs4_t *string2 = NULL;
        string2 = mad_parse_genre(string);
        g_free((void *)string);
        string = string2;
    }

    {
        id3_utf8_t *string2 = id3_ucs4_utf8duplicate(string);
        rtn = str_to_utf8(string2);
        g_free(string2);
    }

#ifdef DEBUG
    g_print("i: string = %s\n", rtn);
#endif
    return rtn;
}

/**
 * read the ID3 tag 
 */
static void input_read_tag(struct mad_info_t *info)
{
    gchar *string = NULL;
    TitleInput *title_input;

    if (info->tuple == NULL) {
        title_input = bmp_title_input_new();
        info->tuple = title_input;
    }
    else
        title_input = info->tuple;

    info->id3file = id3_file_open(info->filename, ID3_FILE_MODE_READONLY);
    if (!info->id3file) {
        return;
    }

    info->tag = id3_file_tag(info->id3file);
    if (!info->tag) {
        return;
    }

    title_input->performer =
        input_id3_get_string(info->tag, ID3_FRAME_ARTIST);
    title_input->track_name =
        input_id3_get_string(info->tag, ID3_FRAME_TITLE);
    title_input->album_name =
        input_id3_get_string(info->tag, ID3_FRAME_ALBUM);
    title_input->genre = input_id3_get_string(info->tag, ID3_FRAME_GENRE);
    title_input->comment =
        input_id3_get_string(info->tag, ID3_FRAME_COMMENT);
    string = input_id3_get_string(info->tag, ID3_FRAME_TRACK);
    if (string) {
        title_input->track_number = atoi(string);
        g_free(string);
        string = NULL;
    }
    // year
    string = NULL;
    string = input_id3_get_string(info->tag, ID3_FRAME_YEAR);   //TDRC
    if (!string)
        string = input_id3_get_string(info->tag, "TYER");

    if (string) {
        title_input->year = atoi(string);
        g_free(string);
        string = NULL;
    }

    title_input->file_name = g_strdup(g_basename(info->filename));
    title_input->file_path = g_path_get_dirname(info->filename);
    if ((string = strrchr(title_input->file_name, '.'))) {
        title_input->file_ext = string + 1;
        *string = '\0';         // make filename end at dot.
    }

    info->title = xmms_get_titlestring(audmad_config.title_override == TRUE ?
        audmad_config.id3_format : xmms_get_gentitle_format(), title_input);
}

/**
 * Retrieve meta-information about URL.
 * For local files this means ID3 tag etc.
 */
gboolean input_get_info(struct mad_info_t *info, gboolean fast_scan)
{
#ifdef DEBUG
    g_message("f: input_get_info: %s", info->title);
#endif                          /* DEBUG */
    input_read_tag(info);
    input_read_replaygain(info);

    /* scan mp3 file, decoding headers unless fast_scan is set */
    if (scan_file(info, fast_scan) == FALSE)
        return FALSE;

    /* reset the input file to the start */
    vfs_rewind(info->infile);
    info->offset = 0;

    if (info->remote)
    {
        gchar *stream_name = vfs_get_metadata(info->infile, "stream-name");
        gchar *track_name = vfs_get_metadata(info->infile, "track-name");
        gchar *tmp = NULL;

        g_free(info->title);
        g_free(info->tuple->track_name);
        g_free(info->tuple->album_name);

        info->title = g_strdup(track_name);
        info->tuple->track_name = g_strdup(track_name);
        info->tuple->album_name = g_strdup(stream_name);
        tmp = g_strdup_printf("%s (%s)", track_name, stream_name);
        mad_plugin->set_info(tmp,
                             -1, // indicates the stream is unseekable
                             info->bitrate, info->freq, info->channels);
        g_free(tmp); g_free(stream_name); g_free(track_name);
    }

    /* use the filename for the title as a last resort */
    if (!info->title)
    {
        char *pos = strrchr(info->filename, DIR_SEPARATOR);
        if (pos)
            info->title = g_strdup(pos + 1);
        else
            info->title = g_strdup(info->filename);
    }

#ifdef DEBUG
    g_message("e: input_get_info");
#endif                          /* DEBUG */
    return TRUE;
}


/**
 * Read data from the source given my madinfo into the buffer
 * provided.  Return the number of bytes read.
 * @return 0 on EOF
 * @return -1 on error
 */
int
input_get_data(struct mad_info_t *madinfo, guchar * buffer,
               int buffer_size)
{
    int len = 0;
#ifdef DEBUG
//  g_message ("f: input_get_data: %d", buffer_size);
#endif

    /* simply read to data from the file */
    len = vfs_fread(buffer, 1, buffer_size, madinfo->infile);

    if (len == 0 && madinfo->playback)
        madinfo->playback->eof = TRUE;

    if (madinfo->remote)
    {
        gchar *stream_name = vfs_get_metadata(madinfo->infile, "stream-name");
        gchar *track_name = vfs_get_metadata(madinfo->infile, "track-name");
        gchar *tmp = NULL;

        g_free(madinfo->title);
        g_free(madinfo->tuple->track_name);
        g_free(madinfo->tuple->album_name);

        madinfo->title = g_strdup(track_name);
        madinfo->tuple->track_name = g_strdup(track_name);
        madinfo->tuple->album_name = g_strdup(stream_name);
        tmp = g_strdup_printf("%s (%s)", track_name, stream_name);
        mad_plugin->set_info(tmp,
                             -1, // indicates the stream is unseekable
                             madinfo->bitrate, madinfo->freq, madinfo->channels);
        g_free(tmp); g_free(stream_name); g_free(track_name);
    }

#ifdef DEBUG
//  g_message ("e: input_get_data: size=%d offset=%d", len, madinfo->offset);
#endif
    madinfo->offset += len;
    return len;
}

/**
 * Free up all mad_info_t related resourses.
 */
gboolean input_term(struct mad_info_t * info)
{
#ifdef DEBUG
    g_message("f: input_term");
#endif

    if (info->title)
        g_free(info->title);
    if (info->url)
        g_free(info->url);
    if (info->filename)
        g_free(info->filename);
    if (info->infile)
        vfs_fclose(info->infile);
    if (info->id3file)
        id3_file_close(info->id3file);

    if (info->replaygain_album_str)
        g_free(info->replaygain_album_str);
    if (info->replaygain_track_str)
        g_free(info->replaygain_track_str);
    if (info->replaygain_album_peak_str)
        g_free(info->replaygain_album_peak_str);
    if (info->replaygain_track_peak_str)
        g_free(info->replaygain_track_peak_str);
    if (info->mp3gain_undo_str)
        g_free(info->mp3gain_undo_str);
    if (info->mp3gain_minmax_str)
        g_free(info->mp3gain_minmax_str);

    if (info->tuple) {
        bmp_title_input_free(info->tuple);
        info->tuple = NULL;
    }

    /* set everything to zero in case it gets used again. */
    memset(info, 0, sizeof(struct mad_info_t));
#ifdef DEBUG
    g_message("e: input_term");
#endif
    return TRUE;
}
