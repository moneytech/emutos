/*
 * text.c - uses text_blt to move data from a font table to screen
 *
 * Copyright 1982 by Digital Research Inc.  All rights reserved.
 * Copyright (c) 1999 Caldera, Inc. and Authors:
 *               2002 The EmuTOS development team
 *
 * This file is distributed under the GPL, version 2 or at your
 * option any later version.  See doc/license.txt for details.
 */



#include "portab.h"
#include "gsxdef.h"
#include "gsxextrn.h"
#include "jmptbl.h"

extern WORD clip_line();
extern void trnsfont();

extern WORD XACC_DDA;           /* accumulator for x DDA        */
extern WORD SOURCEX, SOURCEY;   /* upper left of character in font file */
extern WORD DESTX, DESTY;       /* upper left of destination on screen  */
extern WORD DELX, DELY;         /* width and height of character    */
extern WORD *FBASE;             /* pointer to font data         */
extern WORD FWIDTH;             /* offset,segment and form with of font */
extern WORD LITEMASK, SKEWMASK; /* special effects          */
extern WORD WEIGHT;             /* special effects          */
extern WORD R_OFF, L_OFF;       /* skew above and below baseline    */
extern WORD TEXT_FG;
extern WORD width, height;      /* extent of string set in dqt_extent   */
extern WORD wordx, wordy;       /* add this to each space for interword */
extern WORD rmword;             /* the number of pixels left over   */
extern WORD rmwordx, rmwordy;   /* add this to use up remainder     */
extern WORD charx, chary;       /* add this to each char for interchar  */
extern WORD rmchar;             /* number of pixels left over       */
extern WORD rmcharx, rmchary;   /* add this to use up remainder     */


extern struct font_head fon6x6;         /* See bios/fntxxx.c */
extern struct font_head fon8x8;         /* See bios/fntxxx.c */
extern struct font_head fon8x16;        /* See bios/fntxxx.c */



/* Prototypes for this module */

void make_header();
WORD clc_dda(WORD top, WORD height);

void d_gtext()
{
    WORD count;
    WORD i, j;
    WORD startx=0, starty=0;
    WORD xfact=0, yfact=0;
    WORD tx1, tx2, ty1, ty2;
    WORD delh=0, delv=0;
    WORD d1, d2;
    WORD extent[8];
    WORD *old_ptr;
    WORD justified;

    WORD temp;
    REG struct font_head *fnt_ptr = NULL;
    REG WORD *pointer = NULL;

    count = CONTRL[3];
    if (count > 0) {

        fnt_ptr = cur_font;     /* Get current font pointer in register */

        justified = (*CONTRL == 11);

        if (STYLE & F_THICKEN)
            WEIGHT = fnt_ptr->thicken;

        if (STYLE & F_LIGHT)
            LITEMASK = fnt_ptr->lighten;

        if (STYLE & F_SKEW) {
            L_OFF = fnt_ptr->left_offset;
            R_OFF = fnt_ptr->right_offset;
            SKEWMASK = fnt_ptr->skew;
        } else {
            L_OFF = 0;
            R_OFF = 0;
        }

        FBASE = fnt_ptr->dat_table;
        FWIDTH = fnt_ptr->form_width;

        switch (h_align) {
        case 0:
            delh = 0;
            break;
        case 1:
            if (!justified) {   /* width set if GDP */
                old_ptr = PTSOUT;
                PTSOUT = extent;
                dqt_extent();
                PTSOUT = old_ptr;
                *(CONTRL + 2) = 0;
            }
            delh = width / 2;
            break;
        case 2:
            if (!justified) {   /* width set if GDP */
                old_ptr = PTSOUT;
                PTSOUT = extent;
                dqt_extent();
                PTSOUT = old_ptr;
                *(CONTRL + 2) = 0;
            }
            delh = width;
            break;
        }

        if (STYLE & F_SKEW) {
            d1 = fnt_ptr->left_offset;
            d2 = fnt_ptr->right_offset;
        } else {
            d1 = 0;
            d2 = 0;
        }

        switch (v_align) {
        case 0:
            delv = fnt_ptr->top;
            delh += d1;
            break;
        case 1:
            delv = fnt_ptr->top - fnt_ptr->half;
            delh += (fnt_ptr->half * d2) / fnt_ptr->top;
            break;
        case 2:
            delv = fnt_ptr->top - fnt_ptr->ascent;
            delh += (fnt_ptr->ascent * d2) / fnt_ptr->top;
            break;
        case 3:
            delv = fnt_ptr->top + fnt_ptr->bottom;
            break;
        case 4:
            delv = fnt_ptr->top + fnt_ptr->descent;
            delh += (fnt_ptr->descent * d1) / fnt_ptr->bottom;
            break;
        case 5:
            delv = 0;
            delh += d1 + d2;
            break;
        }

        pointer = PTSIN;
        switch (CHUP) {
        case 0:
            DESTX = *(pointer) - delh;
            DESTY = *(pointer + 1) - delv;
            startx = DESTX;
            starty = DESTY + fnt_ptr->top + fnt_ptr->ul_size + 1;
            xfact = 0;
            yfact = 1;
            break;
        case 900:
            DESTX = *(pointer) - delv;
            DESTY = *(pointer + 1) + delh;
            startx = DESTX + fnt_ptr->top + fnt_ptr->ul_size + 1;
            starty = DESTY;
            xfact = 1;
            yfact = 0;
            break;
        case 1800:
            DESTX = *(pointer) + delh;
            DESTY = *(pointer + 1) - ((fnt_ptr->top + fnt_ptr->bottom) - delv);
            startx = DESTX;
            starty = (DESTY + fnt_ptr->bottom) - (fnt_ptr->ul_size + 1);
            xfact = 0;
            yfact = -1;
            break;
        case 2700:
            DESTX = *pointer - ((fnt_ptr->top + fnt_ptr->bottom) - delv);
            DESTY = *(pointer + 1) - delh;
            starty = DESTY;
            startx = (DESTX + fnt_ptr->bottom) - (fnt_ptr->ul_size + 1);
            xfact = -1;
            yfact = 0;
            break;
        }

        TEXT_FG = cur_work->text_color;

        DELY = fnt_ptr->form_height;

        XACC_DDA = 32767;   /* init the horizontal dda */

        for (j = 0; j < count; j++) {

            temp = INTIN[j];

            /* If the character is out of range for this font make it a ? */

            if ((temp < fnt_ptr->first_ade) || (temp > fnt_ptr->last_ade))
                temp = 63;
            temp -= fnt_ptr->first_ade;

            SOURCEX = fnt_ptr->off_table[temp];
            DELX = fnt_ptr->off_table[temp + 1] - SOURCEX;

            SOURCEY = 0;
            DELY = fnt_ptr->form_height;

            text_blt();

            fnt_ptr = cur_font;     /* restore reg var */

            if (justified) {
                DESTX += charx;
                DESTY += chary;
                if (rmchar) {
                    DESTX += rmcharx;
                    DESTY += rmchary;
                    rmchar--;
                }
                if (INTIN[j] == 32) {
                    DESTX += wordx;
                    DESTY += wordy;
                    if (rmword) {
                        DESTX += rmwordx;
                        DESTY += rmwordy;
                        rmword--;
                    }
                }
            }
            /* end if justified */
            if (fnt_ptr->flags & F_HORZ_OFF)
                DESTX += fnt_ptr->hor_table[temp];

        }                   /* for j */

        if (STYLE & F_UNDER) {
            X1 = startx;
            Y1 = starty;

            if (CHUP % 1800 == 0) {
                X2 = DESTX;
                Y2 = Y1;
            } else {
                X2 = X1;
                Y2 = DESTY;
            }
            if (STYLE & F_LIGHT)
                LN_MASK = cur_font->lighten;
            else
                LN_MASK = 0xffff;

            temp = TEXT_FG;
            FG_BP_1 = temp & 1;
            FG_BP_2 = temp & 2;
            FG_BP_3 = temp & 4;
            FG_BP_4 = temp & 8;

            count = cur_font->ul_size;
            for (i = 0; i < count; i++) {
                if (CLIP) {
                    tx1 = X1;
                    tx2 = X2;
                    ty1 = Y1;
                    ty2 = Y2;

                    if (clip_line())
                        ABLINE();

                    X1 = tx1;
                    X2 = tx2;
                    Y1 = ty1;
                    Y2 = ty2;
                } else
                    ABLINE();

                X1 += xfact;
                X2 += xfact;
                Y1 += yfact;
                Y2 += yfact;

                if (LN_MASK & 1)
                    LN_MASK = (LN_MASK >> 1) | 0x8000;
                else
                    LN_MASK = LN_MASK >> 1;
            } /* End for */
        } /* End if underline */
    } /* if count */
}


void text_init()
{
    WORD i, j;
    WORD id_save;
    REG struct font_head *fnt_ptr, **chain_ptr;

    SIZ_TAB[0] = 32767;         // minimal char width
    SIZ_TAB[1] = 32767;         // minimal char height
    SIZ_TAB[2] = 0;             // maximal char width
    SIZ_TAB[3] = 0;             // maximal char heigh

    /* Initialize the font ring. */
    font_ring[0] = &fon6x6;
    font_ring[1] = &fon8x8;
    font_ring[2] = NULLPTR;
    font_ring[3] = NULLPTR;

    id_save = fon6x6.font_id;

    chain_ptr = font_ring;
    i = 0;
    j = 0;
    while ((fnt_ptr = *chain_ptr++)) {
        do {
            if (fnt_ptr->flags & F_DEFAULT)     /* If default save pointer */
                def_font = fnt_ptr;

            if (fnt_ptr->font_id != id_save) {  /* If new font count */
                j++;
                id_save = fnt_ptr->font_id;
            }

            if (fnt_ptr->font_id == 1) {        /* Update SIZ_TAB if system font */
                if (SIZ_TAB[0] > fnt_ptr->max_char_width)
                    SIZ_TAB[0] = fnt_ptr->max_char_width;

                if (SIZ_TAB[1] > fnt_ptr->top)
                    SIZ_TAB[1] = fnt_ptr->top;

                if (SIZ_TAB[2] < fnt_ptr->max_char_width)
                    SIZ_TAB[2] = fnt_ptr->max_char_width;

                if (SIZ_TAB[3] < fnt_ptr->top)
                    SIZ_TAB[3] = fnt_ptr->top;
                i++;            /* Increment count of heights */
            }
            /* end if system font */
            if (!(fnt_ptr->flags & F_STDFORM)) {
                FBASE = fnt_ptr->dat_table;
                FWIDTH = fnt_ptr->form_width;
                DELY = fnt_ptr->form_height;
                trnsfont();
            }

        } while ((fnt_ptr = fnt_ptr->next_font));
    }

    DEV_TAB[5] = i;                     /* number of sizes */
    font_count = DEV_TAB[10] = ++j;     /* number of faces */

    cur_font = def_font;
}

void dst_height()
{
    struct font_head **chain_ptr;
    REG struct font_head *test_font, *single_font;
    REG WORD *pointer, font_id, test_height;
    BYTE found;

    font_id = cur_font->font_id;
    cur_work->pts_mode = FALSE;

    /* Find the smallest font in the requested face */

    chain_ptr = font_ring;

    found = 0;
    while (!found && (test_font = *chain_ptr++)) {
        do {
            found = (test_font->font_id == font_id);
        } while (!found && (test_font = test_font->next_font));
    }

    single_font = test_font;
    test_height = PTSIN[1];
    if (cur_work->xfm_mode == 0)        /* If NDC transformation, swap y
                                           coordinate */
        test_height = DEV_TAB[1] + 1 - test_height;

    /* Traverse the chains and find the font closest to the size requested. */

    do {
        while ((test_font->top <= test_height)
               && (test_font->font_id == font_id)) {
            single_font = test_font;
            if (!(test_font = test_font->next_font))
                break;
        }
    } while ((test_font = *chain_ptr++));

    /* Set up environment for this font in the non-scaled case */

    cur_work->cur_font = cur_font = single_font;
    cur_work->scaled = FALSE;

    if (single_font->top != test_height) {
        DDA_INC = cur_work->dda_inc =
            clc_dda(single_font->top, test_height);
        cur_work->t_sclsts = T_SCLSTS;
        make_header();
        single_font = cur_font;
    }

    CONTRL[2] = 2;

    pointer = PTSOUT;
    *pointer++ = single_font->max_char_width;
    *pointer++ = test_height = single_font->top;
    *pointer++ = single_font->max_cell_width;
    *pointer++ = test_height + single_font->bottom + 1;
    FLIP_Y = 1;
}


void copy_name(BYTE * source, BYTE * dest)
{
    REG WORD i;
    REG BYTE *sptr, *dptr;

    sptr = source;
    dptr = dest;

    for (i = 0; i < 32; i++)
        *dptr++ = *sptr++;
}


void make_header()
{
    REG struct attribute *work_ptr;
    REG struct font_head *source_font, *dest_font;

    work_ptr = cur_work;
    source_font = work_ptr->cur_font;
    dest_font = &work_ptr->scratch_head;

    dest_font->font_id = source_font->font_id;
    dest_font->point = source_font->point * 2;

    copy_name(&source_font->name[0], &dest_font->name[0]);

    dest_font->first_ade = source_font->first_ade;
    dest_font->last_ade = source_font->last_ade;

    if (DDA_INC == 0xFFFF) {
        dest_font->top = source_font->top * 2 + 1;
        dest_font->ascent = source_font->ascent * 2 + 1;
        dest_font->half = source_font->half * 2 + 1;
        dest_font->descent = source_font->descent * 2;
        dest_font->bottom = source_font->bottom * 2;
        dest_font->max_char_width = source_font->max_char_width * 2;
        dest_font->max_cell_width = source_font->max_cell_width * 2;
        dest_font->left_offset = source_font->left_offset * 2;
        dest_font->right_offset = source_font->right_offset * 2;
        dest_font->thicken = source_font->thicken * 2;
        dest_font->ul_size = source_font->ul_size * 2;
    } else {
        dest_font->top = ACT_SIZ(source_font->top);
        dest_font->ascent = ACT_SIZ(source_font->ascent);
        dest_font->half = ACT_SIZ(source_font->half);
        dest_font->descent = ACT_SIZ(source_font->descent);
        dest_font->bottom = ACT_SIZ(source_font->bottom);
        dest_font->max_char_width = ACT_SIZ(source_font->max_char_width);
        dest_font->max_cell_width = ACT_SIZ(source_font->max_cell_width);
        dest_font->left_offset = ACT_SIZ(source_font->left_offset);
        dest_font->right_offset = ACT_SIZ(source_font->right_offset);
        dest_font->thicken = ACT_SIZ(source_font->thicken);
        dest_font->ul_size = ACT_SIZ(source_font->ul_size);
    }

    dest_font->lighten = source_font->lighten;
    dest_font->skew = source_font->skew;
    dest_font->flags = source_font->flags;

    dest_font->hor_table = source_font->hor_table;
    dest_font->off_table = source_font->off_table;
    dest_font->dat_table = source_font->dat_table;

    dest_font->form_width = source_font->form_width;
    dest_font->form_height = source_font->form_height;

    work_ptr->scaled = TRUE;
    work_ptr->cur_font = cur_font = dest_font;
}


void dst_point()
{
    WORD font_id;
    struct font_head **chain_ptr, *double_font;
    REG struct font_head *test_font, *single_font;
    REG WORD *pointer, test_height, height;
    BYTE found;

    font_id = cur_font->font_id;
    cur_work->pts_mode = TRUE;

    /* Find the smallest font in the requested face */

    chain_ptr = font_ring;
    found = 0;
    while (!found && (test_font = *chain_ptr++)) {
        do {
            found = (test_font->font_id == font_id);
        } while (!found && (test_font = test_font->next_font));
    }

    double_font = single_font = test_font;
    test_height = INTIN[0];

    /* Traverse the chains and find the font closest to the size requested */
    /* and closest to half the size requested.                 */

    do {
        while (((height = test_font->point) <= test_height)
               && (test_font->font_id == font_id)) {
            single_font = test_font;
            if (height * 2 <= test_height)
                double_font = test_font;

            if (!(test_font = test_font->next_font))
                break;
        }
    } while ((test_font = *chain_ptr++));

    /* Set up environment for this font in the non-scaled case */

    cur_font = cur_work->cur_font = single_font;
    cur_work->scaled = FALSE;

    if (single_font->point != test_height) {
        height = double_font->point * 2;

        if ((height > single_font->point) && (height <= test_height)) {
            DDA_INC = cur_work->dda_inc = 0xFFFF;
            cur_work->t_sclsts = 1;
            cur_work->cur_font = double_font;
            make_header();
            single_font = cur_font;
        }
    }

    pointer = CONTRL;
    *(pointer + 4) = 1;
    *(pointer + 2) = 2;

    INTOUT[0] = single_font->point;

    pointer = PTSOUT;
    *pointer++ = single_font->max_char_width;
    *pointer++ = test_height = single_font->top;
    *pointer++ = single_font->max_cell_width;
    *pointer++ = test_height + single_font->bottom + 1;
    FLIP_Y = 1;
}


void dst_style()
{
    INTOUT[0] = cur_work->style = INTIN[0] & INQ_TAB[2];
    CONTRL[4] = 1;
}


void dst_alignment()
{
    REG WORD a, *int_out, *int_in;
    REG struct attribute *work_ptr;

    work_ptr = cur_work;
    int_in = INTIN;
    int_out = INTOUT;
    a = *int_in++;
    if (a < 0 || a > 2)
        a = 0;
    work_ptr->h_align = *int_out++ = a;

    a = *int_in;
    if (a < 0 || a > 5)
        a = 0;
    work_ptr->v_align = *int_out = a;

    CONTRL[4] = 2;
}


void dst_rotation()
{
    INTOUT[0] = cur_work->chup = ((INTIN[0] + 450) / 900) * 900;
    CONTRL[4] = 1;
}


void dst_font()
{
    WORD *old_intin, point, *old_ptsout, dummy[4], *old_ptsin;
    REG WORD face;
    REG struct font_head *test_font, **chain_ptr;
    BYTE found;

    test_font = cur_font;
    point = test_font->point;
    dummy[1] = test_font->top;
    face = INTIN[0];

    chain_ptr = font_ring;

    found = 0;
    while (!found && (test_font = *chain_ptr++)) {
        do {
            found = (test_font->font_id == face);
        } while (!found && (test_font = test_font->next_font));
    }

    /* If we fell through the loop, we could not find the face. */
    /* Default to the system font.                  */
    if (!found)
        test_font = &fon6x6;

    /* Call down to the set text height routine to get the proper size */

    cur_work->cur_font = cur_font = test_font;

    old_intin = INTIN;
    old_ptsin = PTSIN;
    old_ptsout = PTSOUT;
    INTIN = &point;
    PTSIN = PTSOUT = dummy;

    if (cur_work->pts_mode)
        dst_point();
    else
        dst_height();

    INTIN = old_intin;
    PTSIN = old_ptsin;
    PTSOUT = old_ptsout;

    CONTRL[2] = 0;
    CONTRL[4] = 1;
    INTOUT[0] = cur_font->font_id;
}


void dst_color()
{
    REG WORD r;

    r = INTIN[0];
    if ((r >= DEV_TAB[13]) || (r < 0))
        r = 1;
    CONTRL[4] = 1;
    INTOUT[0] = r;
    cur_work->text_color = MAP_COL[r];
}


void dqt_attributes()
{
    REG WORD *pointer, temp;
    REG struct font_head *fnt_ptr;
    REG struct attribute *work_ptr;

    pointer = INTOUT;
    work_ptr = cur_work;
    fnt_ptr = cur_font;

    *pointer++ = fnt_ptr->font_id;      /* INTOUT[0] */
    *pointer++ = REV_MAP_COL[work_ptr->text_color];     /* INTOUT[1] */
    *pointer++ = work_ptr->chup;        /* INTOUT[2] */
    *pointer++ = work_ptr->h_align;     /* INTOUT[3] */
    *pointer++ = work_ptr->v_align;     /* INTOUT[4] */
    *pointer = work_ptr->wrt_mode;      /* INTOUT[5] */

    pointer = PTSOUT;
    *pointer++ = fnt_ptr->max_char_width;
    *pointer++ = temp = fnt_ptr->top;
    *pointer++ = fnt_ptr->max_cell_width;
    *pointer = temp + fnt_ptr->bottom + 1;

    pointer = CONTRL;
    *(pointer + 2) = 2;
    *(pointer + 4) = 6;
    FLIP_Y = 1;
}


void dqt_extent()
{
    REG WORD i, chr, table_start;
    REG WORD *pointer;
    REG struct font_head *fnt_ptr;

    WORD cnt;

    fnt_ptr = cur_font;
    pointer = INTIN;

    width = 0;
    table_start = fnt_ptr->first_ade;
    cnt = CONTRL[3];

    for (i = 0; i < cnt; i++) {
        chr = *pointer++ - table_start;
        width += fnt_ptr->off_table[chr + 1] - fnt_ptr->off_table[chr];
    }

    if (DOUBLE) {
        if (DDA_INC == 0xFFFF)
            width *= 2;
        else
            width = ACT_SIZ(width);
    }

    if ((STYLE & F_THICKEN) && !(fnt_ptr->flags & F_MONOSPACE))
        width += cnt * fnt_ptr->thicken;

    if (STYLE & F_SKEW)
        width += fnt_ptr->left_offset + fnt_ptr->right_offset;

    height = fnt_ptr->top + fnt_ptr->bottom + 1;

    CONTRL[2] = 4;

    pointer = PTSOUT;
    switch (CHUP) {
    case 0:
        *pointer++ = 0;
        *pointer++ = 0;
        *pointer++ = width;
        *pointer++ = 0;
        *pointer++ = width;
        *pointer++ = height;
        *pointer++ = 0;
        *pointer = height;
        break;
    case 900:
        *pointer++ = height;
        *pointer++ = 0;
        *pointer++ = height;
        *pointer++ = width;
        *pointer++ = 0;
        *pointer++ = width;
        *pointer++ = 0;
        *pointer = 0;
        break;
    case 1800:
        *pointer++ = width;
        *pointer++ = height;
        *pointer++ = 0;
        *pointer++ = height;
        *pointer++ = 0;
        *pointer++ = 0;
        *pointer++ = width;
        *pointer = 0;
        break;
    case 2700:
        *pointer++ = 0;
        *pointer++ = height;
        *pointer++ = 0;
        *pointer++ = 0;
        *pointer++ = height;
        *pointer++ = 0;
        *pointer++ = width;
        *pointer = height;
        break;
    }
    FLIP_Y = 1;
}


void dqt_width()
{
    REG WORD k;
    REG WORD *pointer;
    REG struct font_head *fnt_ptr;

    fnt_ptr = cur_font;
    pointer = PTSOUT;

    /* Set that there is no horizontal offset */

    *(pointer + 2) = 0;
    *(pointer + 4) = 0;

    k = INTIN[0];
    if ((k < fnt_ptr->first_ade) || (k > fnt_ptr->last_ade))
        INTOUT[0] = -1;
    else {
        INTOUT[0] = k;
        k -= fnt_ptr->first_ade;
        *(pointer) = fnt_ptr->off_table[k + 1] - fnt_ptr->off_table[k];
        if (DOUBLE) {
            if (DDA_INC == 0xFFFF)
                *pointer *= 2;
            else
                *pointer = ACT_SIZ(*pointer);
        }

        if (fnt_ptr->flags & F_HORZ_OFF) {
            *(pointer + 2) = fnt_ptr->hor_table[k * 2];
            *(pointer + 4) = fnt_ptr->hor_table[(k * 2) + 1];
        }
    }

    pointer = CONTRL;
    *(pointer + 2) = 3;
    *(pointer + 4) = 1;
    FLIP_Y = 1;
}


void dqt_name()
{
    REG WORD i, element;
    REG BYTE *name;
    REG WORD *int_out;
    REG struct font_head *tmp_font;
    BYTE found;

    WORD font_id;
    struct font_head **chain_ptr;

    element = INTIN[0];
    chain_ptr = font_ring;
    i = 0;
    font_id = -1;

    found = 0;
    while (!found && (tmp_font = *chain_ptr++)) {
        do {
            font_id = tmp_font->font_id;
            if ((++i) == element)
                found = 1;
        } while (!found && (tmp_font = tmp_font->next_font));
    }

    /* The element is out of bounds - default to the system font */
    if (!found)
        tmp_font = &fon6x6;

    int_out = INTOUT;
    *int_out++ = tmp_font->font_id;
    for (i = 1, name = tmp_font->name; (*int_out++ = *name++); i++);
    while (i < 33) {
        *int_out++ = 0;
        i++;
    }
    CONTRL[4] = 33;

}


void dqt_fontinfo()
{
    REG WORD *pointer;
    REG struct font_head *fnt_ptr;

    fnt_ptr = cur_font;

    pointer = INTOUT;
    *pointer++ = fnt_ptr->first_ade;
    *pointer = fnt_ptr->last_ade;

    pointer = PTSOUT;
    *pointer++ = fnt_ptr->max_cell_width;
    *pointer++ = fnt_ptr->bottom;

    if (STYLE & F_THICKEN)
        *pointer++ = fnt_ptr->thicken;
    else
        *pointer++ = 0;

    *pointer++ = fnt_ptr->descent;

    if (STYLE & F_SKEW) {
        *pointer++ = fnt_ptr->left_offset;
        *pointer++ = fnt_ptr->half;
        *pointer++ = fnt_ptr->right_offset;
    } else {
        *pointer++ = 0;
        *pointer++ = fnt_ptr->half;
        *pointer++ = 0;
    }

    *pointer++ = fnt_ptr->ascent;
    *pointer++ = 0;
    *pointer = fnt_ptr->top;

    pointer = CONTRL;
    *(pointer + 2) = 5;
    *(pointer + 4) = 2;
    FLIP_Y = 1;
}


void d_justified()
{
    WORD spaces;
    WORD expand, sav_cnt;
    WORD interword, interchar;
    WORD cnt, *old_intin, *old_ptsout, extent[8], max_x;
    REG WORD i, direction, delword, delchar;
    REG WORD *pointer;

    pointer = (CONTRL + 3);
    cnt = *pointer = (sav_cnt = *pointer) - 2;

    pointer = INTIN;
    interword = *pointer++;
    interchar = *pointer++;

    old_intin = INTIN;
    INTIN = pointer;
    old_ptsout = PTSOUT;
    PTSOUT = extent;

    for (i = 0, spaces = 0; i < cnt; i++)
        if (*(pointer++) == 32)
            spaces++;

    dqt_extent();
    CONTRL[2] = 0;

    max_x = PTSIN[2];

    if (interword && spaces) {
        delword = (max_x - width) / spaces;
        rmword = (max_x - width) % spaces;

        if (rmword < 0) {
            direction = -1;
            rmword = 0 - rmword;
        } else
            direction = 1;

        if (interchar) {
            expand = cur_font->max_cell_width / 2;
            if (delword > expand) {
                delword = expand;
                rmword = 0;
            }
            if (delword < (0 - expand)) {
                delword = 0 - expand;
                rmword = 0;
            }
            width += (delword * spaces) + (rmword * direction);
        }

        switch (CHUP) {
        case 0:
            wordx = delword;
            wordy = 0;
            rmwordx = direction;
            rmwordy = 0;
            break;
        case 900:
            wordx = 0;
            wordy = 0 - delword;
            rmwordx = 0;
            rmwordy = 0 - direction;
            break;
        case 1800:
            wordx = 0 - delword;
            wordy = 0;
            rmwordx = 0 - direction;
            rmwordy = 0;
            break;
        case 2700:
            wordx = 0;
            wordy = delword;
            rmwordx = 0;
            rmwordy = direction;
            break;
        }
    } else {
        wordx = 0;
        wordy = 0;
        rmword = 0;
    }

    if (interchar && cnt > 1) {
        delchar = (max_x - width) / (cnt - 1);
        rmchar = (max_x - width) % (cnt - 1);

        if (rmchar < 0) {
            direction = -1;
            rmchar = 0 - rmchar;
        } else
            direction = 1;

        switch (CHUP) {
        case 0:
            charx = delchar;
            chary = 0;
            rmcharx = direction;
            rmchary = 0;
            break;
        case 900:
            charx = 0;
            chary = 0 - delchar;
            rmcharx = 0;
            rmchary = 0 - direction;
            break;
        case 1800:
            charx = 0 - delchar;
            chary = 0;
            rmcharx = 0 - direction;
            rmchary = 0;
            break;
        case 2700:
            charx = 0;
            chary = delchar;
            rmcharx = 0;
            rmchary = direction;
            break;
        }
    } else {
        charx = 0;
        chary = 0;
        rmchar = 0;
    }

    width = max_x;

    d_gtext();

    CONTRL[2] = sav_cnt;
    PTSOUT = old_ptsout;
    INTIN = old_intin;
}


void dt_loadfont()
{
    REG WORD id, count, *control;

    REG struct font_head *first_font;
    REG struct attribute *work_ptr;

    /* Init some common variables */

    work_ptr = cur_work;
    control = CONTRL;
    *(control + 4) = 1;

    /* You only get one chance to load fonts.  If fonts are linked in, exit  */

    if (work_ptr->loaded_fonts) {
        INTOUT[0] = 0;
        return;
    }

    /* The inputs to this routine are :         */
    /* CONTRL[7-8]   = Pointer to scratch buffer    */
    /* CONTRL[9]     = Offset to buffer 2       */
    /* CONTRL[10-11] = Pointer to first font    */

    /* Init the global structures           */

    work_ptr->scrpt2 = *(control + 9);
    work_ptr->scrtchp = (WORD *) *((LONG *) (control + 7));

    first_font = (struct font_head *) *((LONG *) (control + 10));
    work_ptr->loaded_fonts = first_font;

    /* Find out how many distinct font id numbers have just been linked in. */

    id = -1;
    count = 0;

    do {

        /* Update the count of font id numbers, if necessary. */

        if (first_font->font_id != id) {
            id = first_font->font_id;
            count++;
        }

        /* Make sure the font is in device specific format. */

        if (!(first_font->flags & F_STDFORM)) {
            FBASE = first_font->dat_table;
            FWIDTH = first_font->form_width;
            DELY = first_font->form_height;
            trnsfont();
            first_font->flags ^= F_STDFORM;
        }
        first_font = first_font->next_font;
    } while (first_font);

    /* Update the device table count of faces. */

    work_ptr->num_fonts += count;
    INTOUT[0] = count;
}


void dt_unloadfont()
{
    REG struct attribute *work_ptr;

    /* Since we always unload all fonts, this is easy. */

    work_ptr = cur_work;
    work_ptr->loaded_fonts = NULLPTR;   /* No fonts installed */

    work_ptr->scrpt2 = scrtsiz; /* Reset pointers to default buffers */
    work_ptr->scrtchp = deftxbuf;

    work_ptr->num_fonts = font_count;   /* Reset font count to
                                           default */
}



/*
 * clc_dda - calculate DDA
 *
 * returns the quotient requested/actual
 */

WORD clc_dda(WORD actual, WORD requested)
{
    ULONG retval;                       /* unsigned return value */

    if (actual <= requested) {
        /* scale down */
        T_SCLSTS = 0;                   /* clear enlarge indicator */
        if (!requested)                 /* if requested 0 ... */
            requested = 1;              /* then make it 1 (minimum value) */
    } else {                            /* small DDA */
        /* scale up */
        T_SCLSTS = 1;                   /* set enlarge indicator */

        requested -= actual;            /* larger than 2x? FIXME: Keep half value ??? */
        if (requested >= actual)
            return -1;                  /* error, (max value, 2x) */
    }

    retval = requested;                 /* expand to LONG */
    retval = retval << 16;              /* request size to high word(bits 31-16) */
    return ((WORD)(retval / actual));   /* return the quotient as WORD */
}

