/*
 *  libcucul      Canvas for ultrafast compositing of Unicode letters
 *  Copyright (c) 2002-2006 Sam Hocevar <sam@zoy.org>
 *                All Rights Reserved
 *
 *  $Id$
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the Do What The Fuck You Want To
 *  Public License, Version 2, as published by Sam Hocevar. See
 *  http://sam.zoy.org/wtfpl/COPYING for more details.
 */

/*
 *  This file contains various import functions.
 */

#include "config.h"
#include "common.h"

#if !defined(__KERNEL__)
#   include <stdio.h>
#   include <stdlib.h>
#   include <string.h>
#endif

#include "cucul.h"
#include "cucul_internals.h"

static cucul_canvas_t *import_caca(void const *, unsigned int);
static cucul_canvas_t *import_text(void const *, unsigned int);
static cucul_canvas_t *import_ansi(void const *, unsigned int);

/** \brief Import a buffer into a canvas
 *
 *  This function imports a memory area into an internal libcucul canvas.
 *
 *  Valid values for \c format are:
 *
 *  \li \c "": attempt to autodetect the file format.
 *
 *  \li \c "ansi": import ANSI files.
 *
 *  \li \c "caca": import native libcaca files.
 *
 *  \param data The memory area to be loaded into a canvas.
 *  \param size The length of the memory area.
 *  \param format A string describing the input format.
 *  \return A libcucul canvas, or NULL in case of error.
 */
cucul_canvas_t * cucul_import_canvas(void const *data, unsigned int size,
                                     char const *format)
{
    char const *buf = (char const*) data;

    if(size==0 || data==NULL)
        return NULL;

    if(!strcasecmp("caca", format))
        return import_caca(data, size);
    if(!strcasecmp("text", format))
        return import_text(data, size);
    if(!strcasecmp("ansi", format))
        return import_ansi(data, size);

    /* Autodetection */
    if(!strcasecmp("", format))
    {
        unsigned int i=0;
        /* if 4 first letters are CACA */
        if(size >= 4 &&
            buf[0] == 'C' && buf[1] == 'A' && buf[2] == 'C' && buf[3] != 'A')
            return import_caca(data, size);

        /* If we find ESC[ argv, we guess it's an ANSI file */
        while(i<size-1)
        {
            if((buf[i] == 0x1b) && (buf[i+1] == '['))
                return import_ansi(data, size);
            i++;
        }

        /* Otherwise, import it as text */
        return import_text(data, size);
    }
    return NULL;
}

/** \brief Get available import formats
 *
 *  Return a list of available import formats. The list is a NULL-terminated
 *  array of strings, interleaving a string containing the internal value for
 *  the import format, to be used with cucul_import_canvas(), and a string
 *  containing the natural language description for that import format.
 *
 *  \return An array of strings.
 */
char const * const * cucul_get_import_list(void)
{
    static char const * const list[] =
    {
        "", "autodetect",
        "text", "plain text",
        "caca", "native libcaca format",
        "ansi", "ANSI coloured text",
        NULL, NULL
    };

    return list;
}

/*
 * XXX: the following functions are local.
 */

static cucul_canvas_t *import_caca(void const *data, unsigned int size)
{
    cucul_canvas_t *cv;
    uint8_t const *buf = (uint8_t const *)data;
    unsigned int width, height, n;

    if(size < 16)
        return NULL;

    if(buf[0] != 'C' || buf[1] != 'A' || buf[2] != 'C' || buf[3] != 'A')
        return NULL;

    if(buf[4] != 'C' || buf[5] != 'A' || buf[6] != 'N' || buf[7] != 'V')
        return NULL;

    width = ((uint32_t)buf[8] << 24) | ((uint32_t)buf[9] << 16)
           | ((uint32_t)buf[10] << 8) | (uint32_t)buf[11];
    height = ((uint32_t)buf[12] << 24) | ((uint32_t)buf[13] << 16)
            | ((uint32_t)buf[14] << 8) | (uint32_t)buf[15];

    if(!width || !height)
        return NULL;

    if(size != 16 + width * height * 8)
        return NULL;

    cv = cucul_create_canvas(width, height);

    if(!cv)
        return NULL;

    for(n = height * width; n--; )
    {
        cv->chars[n] = ((uint32_t)buf[16 + 0 + 8 * n] << 24)
                     | ((uint32_t)buf[16 + 1 + 8 * n] << 16)
                     | ((uint32_t)buf[16 + 2 + 8 * n] << 8)
                     | (uint32_t)buf[16 + 3 + 8 * n];
        cv->attr[n] = ((uint32_t)buf[16 + 4 + 8 * n] << 24)
                    | ((uint32_t)buf[16 + 5 + 8 * n] << 16)
                    | ((uint32_t)buf[16 + 6 + 8 * n] << 8)
                    | (uint32_t)buf[16 + 7 + 8 * n];
    }

    return cv;
}

static cucul_canvas_t *import_text(void const *data, unsigned int size)
{
    cucul_canvas_t *cv;
    char const *text = (char const *)data;
    unsigned int width = 1, height = 1, x = 0, y = 0, i;

    cv = cucul_create_canvas(width, height);
    cucul_set_color(cv, CUCUL_COLOR_DEFAULT, CUCUL_COLOR_TRANSPARENT);

    for(i = 0; i < size; i++)
    {
        unsigned char ch = *text++;

        if(ch == '\r')
            continue;

        if(ch == '\n')
        {
            x = 0;
            y++;
            continue;
        }

        if(x >= width || y >= height)
        {
            if(x >= width)
                width = x + 1;

            if(y >= height)
                height = y + 1;

            cucul_set_canvas_size(cv, width, height);
        }

        cucul_putchar(cv, x, y, ch);
        x++;
    }

    return cv;
}

#define IS_ALPHA(x) (x>='A' && x<='z')
#define END_ARG 0x1337
static int parse_tuple(unsigned int *, unsigned char const *, int);
static void manage_modifiers(int, uint8_t *, uint8_t *, uint8_t *, uint8_t *, uint8_t *, uint8_t *);

static cucul_canvas_t *import_ansi(void const *data, unsigned int size)
{
    cucul_canvas_t *cv;
    unsigned char const *buffer = (unsigned char const*)data;
    unsigned int i;
    int x = 0, y = 0;
    int width = 80, height = 25;
    int save_x = 0, save_y = 0;
    unsigned int skip;
    int j;
    uint8_t fg, bg, save_fg, save_bg, bold, reverse;

    fg = save_fg = CUCUL_COLOR_LIGHTGRAY;
    bg = save_bg = CUCUL_COLOR_BLACK;
    bold = reverse = 0;

    cv = cucul_create_canvas(width, height);
    cucul_set_color(cv, fg, bg);

    for(i = 0; i < size; i += skip)
    {
        skip = 1;

        if(buffer[i] == '\x1a' && size - i >= 8
           && !memcmp(buffer + i + 1, "SAUCE00", 7))
            break; /* End before SAUCE data */

        if(buffer[i] == '\r')
            continue; /* DOS sucks */

        if(buffer[i] == '\n')
        {
            x = 0;
            y++;
            continue;
        }

        if(buffer[i] == '\x1b' && buffer[i + 1] == '[')  /* ESC code */
        {
            unsigned int argv[1024]; /* Should be enough. Will it be? */
            unsigned int argc = 0;
            unsigned char c = '\0';

            i++; // ESC
            i++; // [

            for(j = i; j < (int)size; j++)
                if(IS_ALPHA(buffer[j]))
                {
                    c = buffer[j];
                    break;
                }

            skip += parse_tuple(argv, buffer + i, size - i);

            while(argv[argc] != END_ARG)
                argc++;  /* Gruik */

            switch(c)
            {
            case 'f':
            case 'H':
                switch(argc)
                {
                    case 0: x = y = 0; break;
                    case 1: y = argv[0] - 1; x = 0; break;
                    case 2: y = argv[0] - 1; x = argv[1] - 1; break;
                }
                break;
            case 'A':
                y -= argc ? argv[0] : 1;
                if(y < 0)
                    y = 0;
                break;
            case 'B':
                y += argc ? argv[0] : 1;
                break;
            case 'C':
                x += argc ? argv[0] : 1;
                break;
            case 'D':
                x -= argc ? argv[0] : 1;
                if(x < 0)
                    x = 0;
                break;
            case 's':
                save_x = x;
                save_y = y;
                break;
            case 'u':
                x = save_x;
                y = save_y;
                break;
            case 'J':
                if(argv[0] == 2)
                    x = y = 0;
                break;
            case 'K':
                // CLEAR END OF LINE
                for(j = x; j < width; j++)
                    _cucul_putchar32(cv, j, y, (uint32_t)' ');
                x = width;
                break;
            case 'm':
                for(j = 0; j < (int)argc; j++)
                    manage_modifiers(argv[j], &fg, &bg,
                                     &save_fg, &save_bg, &bold, &reverse);
                if(bold && fg < 8)
                    fg += 8;
                if(bold && bg < 8)
                    bg += 8;
                if(reverse)
                    cucul_set_color(cv, bg, fg);
                else
                    cucul_set_color(cv, fg, bg);
                break;
            default:
                break;
            }

            continue;
        }

        /* We're going to paste a character. First make sure the canvas
         * is big enough. */
        if(x >= width)
        {
            x = 0;
            y++;
        }

        if(y >= height)
        {
            height = y + 1;
            cucul_set_canvas_size(cv, width, height);
        }

        /* Now paste our character */
        _cucul_putchar32(cv, x, y,_cucul_cp437_to_utf32(buffer[i]));
        x++;
    }

    return cv;
}

/* XXX : ANSI loader helpers */

static int parse_tuple(unsigned int *ret, unsigned char const *buffer, int size)
{
    int i = 0;
    int j = 0;
    int t = 0;
    unsigned char nbr[1024];

    ret[0] = END_ARG;

    for(i = 0; i < size; i++)
    {
        if(IS_ALPHA(buffer[i]))
        {
            if(j != 0)
            {
                ret[t] = atoi((char*)nbr);
                t++;
            }
            ret[t] = END_ARG;
            j = 0;
            return i;
        }

        if(buffer[i] != ';')
        {
            nbr[j] = buffer[i];
            nbr[j + 1] = 0;
            j++;
        }
        else
        {
            ret[t] = atoi((char*)nbr);
            t++;
            ret[t] = END_ARG;
            j = 0;
        }
    }
    return size;
}

static void manage_modifiers(int i, uint8_t *fg, uint8_t *bg, uint8_t *save_fg,
                             uint8_t *save_bg, uint8_t *bold, uint8_t *reverse)
{
    static uint8_t const ansi2cucul[] =
    {
        CUCUL_COLOR_BLACK,
        CUCUL_COLOR_RED,
        CUCUL_COLOR_GREEN,
        CUCUL_COLOR_BROWN,
        CUCUL_COLOR_BLUE,
        CUCUL_COLOR_MAGENTA,
        CUCUL_COLOR_CYAN,
        CUCUL_COLOR_LIGHTGRAY
    };

    if(i >= 30 && i <= 37)
        *fg = ansi2cucul[i - 30];
    else if(i >= 40 && i <= 47)
        *bg = ansi2cucul[i - 40];
    else if(i >= 90 && i <= 97)
        *fg = ansi2cucul[i - 90] + 8;
    else if(i >= 100 && i <= 107)
        *bg = ansi2cucul[i - 100] + 8;
    else switch(i)
    {
    case 0:
        *fg = CUCUL_COLOR_DEFAULT;
        *bg = CUCUL_COLOR_DEFAULT;
        *bold = 0;
        *reverse = 0;
        break;
    case 1: /* BOLD */
        *bold = 1;
        break;
    case 4: // Underline
        break;
    case 5: // blink
        break;
    case 7: // reverse
        *reverse = 1;
        break;
    case 8: // invisible
        *save_fg = *fg;
        *save_bg = *bg;
        *fg = CUCUL_COLOR_TRANSPARENT;
        *bg = CUCUL_COLOR_TRANSPARENT;
        break;
    case 28: // not invisible
        *fg = *save_fg;
        *bg = *save_bg;
        break;
    case 39:
        *fg = CUCUL_COLOR_DEFAULT;
        break;
    case 49:
        *bg = CUCUL_COLOR_DEFAULT;
        break;
    default:
        break;
    }
}

