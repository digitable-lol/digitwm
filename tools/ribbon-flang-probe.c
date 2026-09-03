/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * digitwm - two implementations of the ribbon arithmetic, one grid of inputs
 *
 * Copyright (c) 2026 Digitable <https://digitable.life>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Left: ref_policy_*, the ten hand-written policies carved out of the ribbon.c
 * of a PINNED commit - the arithmetic digitwm shipped before it started asking
 * flang.  Right: ribbon_policy_*, out of the ribbon.o this tree builds, which
 * is the very object file the window manager links.
 *
 * Every input is printed as a line into each of two files, and the files are
 * then compared by cmp(1) - byte for byte, not "by meaning".  The counting
 * here and the comparing there are two different programs disagreeing or
 * agreeing, which is the point: a driver that both computes and judges can be
 * wrong about both at once.
 *
 * THE GRID IS NOT RANDOM and it is not ours either.  It is the grid of
 * flang-ribbon's own tools/compare.sh, value for value, so that the number of
 * inputs here is the number the library reports: 526871.  Sitting on the same
 * grid is deliberate - the library proved "emitted flang equals the reference
 * C" on it, and this proves "digitwm through the emitted flang equals the same
 * reference C" on it, so the two runs compose instead of merely rhyming.  The
 * values sit on boundaries and around them: zeros, ones, negatives, inverted
 * spans, presets past the end of the table, flags valued -1, 2 and 7, because
 * the reference treats any non-zero as true and that is checked rather than
 * assumed.
 *
 * Not covered here, and named rather than hidden: the library's «Пара вместе»,
 * which sums both halves of a facing pair.  digitwm has no such function to
 * compare it against - it is checked inside the library, on its own.
 *
 *   ribbon-flang-probe <file for the reference> <file for this tree>
 *
 * prints "<total> <mismatches>" and exits 1 if anything differs.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ribbon-flang-shim.h"

static FILE		*fa, *fb;
static long long	 total, bad;

static void
one(const char *label, int ref, int got)
{
	fprintf(fa, "%s = %d\n", label, ref);
	fprintf(fb, "%s = %d\n", label, got);

	total++;
	if (ref != got) {
		bad++;
		if (bad <= 20)
			fprintf(stderr, "MISMATCH  %s: reference %d, flang %d\n",
			    label, ref, got);
	}
}

#define N(a) ((int)(sizeof(a) / sizeof((a)[0])))

int
main(int argc, char **argv)
{
	char	lab[192];
	int	i0, i1, i2, i3, i4, i5;

	if (argc != 3) {
		fprintf(stderr, "usage: ribbon-flang-probe <ref file> <tree file>\n");
		return 2;
	}
	if (((fa = fopen(argv[1], "w")) == NULL) ||
	    ((fb = fopen(argv[2], "w")) == NULL)) {
		perror("fopen");
		return 2;
	}

	/* 1. Offset across the ribbon. */
	{
	static const int vw[] = { 0, 1, 100, 800, 1280, 1281 };
	static const int cl[] = { -3000, -1000, -8, -1, 0, 1, 7, 640, 1300, 2560, 4000, 5000 };
	static const int cw[] = { 0, 1, 8, 120, 640, 1280, 2000 };
	static const int of[] = { -3000, -1000, -40, 0, 7, 640, 1280, 3000 };
	static const int gp[] = { 0, 1, 8, 1000 };
	static const int ln[] = { 0, 900, 2560, 5000 };
	for (i0 = 0; i0 < N(vw); i0++)
	for (i1 = 0; i1 < N(cl); i1++)
	for (i2 = 0; i2 < N(cw); i2++)
	for (i3 = 0; i3 < N(of); i3++)
	for (i4 = 0; i4 < N(gp); i4++)
	for (i5 = 0; i5 < N(ln); i5++) {
		snprintf(lab, sizeof lab, "offset %d %d %d %d %d %d",
		    vw[i0], cl[i1], cw[i2], of[i3], gp[i4], ln[i5]);
		one(lab,
		    ref_policy_offset(vw[i0], cl[i1], cw[i2], of[i3], gp[i4], ln[i5]),
		    ribbon_policy_offset(vw[i0], cl[i1], cw[i2], of[i3], gp[i4], ln[i5]));
	}
	}

	/* 2. Offset down the stack. */
	{
	static const int vh[] = { 0, 1, 60, 400, 800, 1200 };
	static const int wy[] = { -2000, -60, -1, 0, 1, 300, 900, 4000 };
	static const int wh[] = { 0, 1, 60, 260, 800, 1500, 4000 };
	static const int of[] = { -500, -1, 0, 11, 552, 2000 };
	static const int gp[] = { 0, 8, 64, 4000 };
	static const int cv[] = { 0, 811, 1352, 4000 };
	for (i0 = 0; i0 < N(vh); i0++)
	for (i1 = 0; i1 < N(wy); i1++)
	for (i2 = 0; i2 < N(wh); i2++)
	for (i3 = 0; i3 < N(of); i3++)
	for (i4 = 0; i4 < N(gp); i4++)
	for (i5 = 0; i5 < N(cv); i5++) {
		snprintf(lab, sizeof lab, "voffset %d %d %d %d %d %d",
		    vh[i0], wy[i1], wh[i2], of[i3], gp[i4], cv[i5]);
		one(lab,
		    ref_policy_voffset(vh[i0], wy[i1], wh[i2], of[i3], gp[i4], cv[i5]),
		    ribbon_policy_voffset(vh[i0], wy[i1], wh[i2], of[i3], gp[i4], cv[i5]));
	}
	}

	/* 3. Offset after the viewport changed size. */
	{
	static const int vs[] = { -1000, -8, -1, 0, 1, 2, 7, 8, 60, 100, 119, 120, 121,
	    400, 640, 800, 801, 1280, 1281, 1600, 2000, 2560, 2561, 4000, 9000 };
	for (i0 = 0; i0 < N(vs); i0++)
	for (i1 = 0; i1 < N(vs); i1++)
	for (i2 = 0; i2 < N(vs); i2++) {
		snprintf(lab, sizeof lab, "output %d %d %d", vs[i0], vs[i1], vs[i2]);
		one(lab,
		    ref_policy_output(vs[i0], vs[i1], vs[i2]),
		    ribbon_policy_output(vs[i0], vs[i1], vs[i2]));
	}
	}

	/*
	 * 4. Column width by preset number, together with the table of shares.
	 * The table is the one thing a policy reads from configuration, so the
	 * grid moves Conf.ribbonwidth under it rather than trusting the default.
	 */
	{
	static const int vw[] = { 0, 1, 8, 100, 119, 120, 121, 800, 1280, 2560 };
	static const int ps[] = { -5, -1, 0, 1, 2, 3, 4, 10 };
	static const int gp[] = { 0, 1, 8, 40, 2000 };
	static const int mw[] = { 0, 1, 120, 400, 3000 };
	static const int tb[][4] = {
		{ 33, 50, 67, 100 },
		{ 100, 67, 50, 33 },
		{ 0, 1, 99, 101 },
		{ -20, 200, 50, 50 },
		{ 25, 25, 25, 25 }
	};
	for (i0 = 0; i0 < N(vw); i0++)
	for (i1 = 0; i1 < N(ps); i1++)
	for (i2 = 0; i2 < N(gp); i2++)
	for (i3 = 0; i3 < N(mw); i3++)
	for (i4 = 0; i4 < N(tb); i4++) {
		Conf.ribbonwidth[0] = tb[i4][0];
		Conf.ribbonwidth[1] = tb[i4][1];
		Conf.ribbonwidth[2] = tb[i4][2];
		Conf.ribbonwidth[3] = tb[i4][3];
		snprintf(lab, sizeof lab, "width %d %d %d %d [%d %d %d %d]",
		    vw[i0], ps[i1], gp[i2], mw[i3],
		    tb[i4][0], tb[i4][1], tb[i4][2], tb[i4][3]);
		one(lab,
		    ref_policy_width(vw[i0], ps[i1], gp[i2], mw[i3]),
		    ribbon_policy_width(vw[i0], ps[i1], gp[i2], mw[i3]));
	}
	}

	/*
	 * 5. Width by the share itself: all four cells of the table hold the
	 * same number, so the preset chooses nothing and one arithmetic is
	 * measured.
	 */
	{
	static const int vw[] = { -1000, -8, 0, 1, 8, 40, 100, 119, 120, 121, 200,
	    300, 400, 640, 800, 1000, 1280, 1600, 2560, 4000 };
	static const int pc[] = { -100, -50, -1, 0, 1, 2, 10, 25, 33, 34, 49, 50,
	    51, 66, 67, 75, 99, 100, 101, 150, 200, 999, 3, 7 };
	static const int gp[] = { -8, 0, 1, 8, 40, 2000 };
	static const int mw[] = { -10, 0, 1, 60, 120, 400, 3000 };
	for (i0 = 0; i0 < N(vw); i0++)
	for (i1 = 0; i1 < N(pc); i1++)
	for (i2 = 0; i2 < N(gp); i2++)
	for (i3 = 0; i3 < N(mw); i3++) {
		Conf.ribbonwidth[0] = pc[i1];
		Conf.ribbonwidth[1] = pc[i1];
		Conf.ribbonwidth[2] = pc[i1];
		Conf.ribbonwidth[3] = pc[i1];
		snprintf(lab, sizeof lab, "widthpct %d %d %d %d",
		    vw[i0], pc[i1], gp[i2], mw[i3]);
		one(lab,
		    ref_policy_width(vw[i0], 0, gp[i2], mw[i3]),
		    ribbon_policy_width(vw[i0], 0, gp[i2], mw[i3]));
	}
	}

	/* 6. Height of a window in a column. */
	{
	static const int vh[] = { -100, 0, 1, 40, 59, 60, 61, 100, 400, 800, 801, 1200 };
	static const int nw[] = { -3, -1, 0, 1, 2, 3, 4, 5, 6, 10, 11, 12, 13, 20, 33, 50 };
	static const int ix[] = { -2, -1, 0, 1, 2, 3, 4, 5, 9, 10, 11, 12, 19, 32, 49, 60 };
	static const int gp[] = { 0, 1, 8, 40, 500 };
	static const int mh[] = { 0, 1, 60, 100, 900 };
	for (i0 = 0; i0 < N(vh); i0++)
	for (i1 = 0; i1 < N(nw); i1++)
	for (i2 = 0; i2 < N(ix); i2++)
	for (i3 = 0; i3 < N(gp); i3++)
	for (i4 = 0; i4 < N(mh); i4++) {
		snprintf(lab, sizeof lab, "height %d %d %d %d %d",
		    vh[i0], nw[i1], ix[i2], gp[i3], mh[i4]);
		one(lab,
		    ref_policy_height(vh[i0], nw[i1], ix[i2], gp[i3], mh[i4]),
		    ribbon_policy_height(vh[i0], nw[i1], ix[i2], gp[i3], mh[i4]));
	}
	}

	/*
	 * 7. Where a window goes.  The flags run over more than 0 and 1: the
	 * reference calls any non-zero true, and that is checked here.
	 */
	{
	static const int fl[] = { -1, 0, 1, 2, 7 };
	static const int ru[] = { -1, 0, 1, 2, 3, 7 };
	int a, b, c, d, f;
	for (a = 0; a < N(fl); a++)
	for (b = 0; b < N(fl); b++)
	for (c = 0; c < N(fl); c++)
	for (d = 0; d < N(fl); d++)
	for (f = 0; f < N(fl); f++)
	for (i0 = 0; i0 < N(ru); i0++) {
		snprintf(lab, sizeof lab, "insert %d %d %d %d %d %d",
		    fl[a], fl[b], fl[c], fl[d], fl[f], ru[i0]);
		one(lab,
		    ref_policy_insert(fl[a], fl[b], fl[c], fl[d], fl[f], ru[i0]),
		    ribbon_policy_insert(fl[a], fl[b], fl[c], fl[d], fl[f], ru[i0]));
	}
	}

	/* 8. Focus after a close. */
	{
	static const int ix[] = { -5, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
	    12, 15, 20, 33, 50, 100 };
	static const int nc[] = { -3, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 20, 50, 101 };
	static const int fg[] = { -1, 0, 1, 2 };
	for (i0 = 0; i0 < N(ix); i0++)
	for (i1 = 0; i1 < N(nc); i1++)
	for (i2 = 0; i2 < N(fg); i2++)
	for (i3 = 0; i3 < N(fg); i3++) {
		snprintf(lab, sizeof lab, "close %d %d %d %d",
		    ix[i0], nc[i1], fg[i2], fg[i3]);
		one(lab,
		    ref_policy_close(ix[i0], nc[i1], fg[i2], fg[i3]),
		    ribbon_policy_close(ix[i0], nc[i1], fg[i2], fg[i3]));
	}
	}

	/* 9. Do a panel's strut and a region meet at all. */
	{
	static const int sp[] = { -100, -1, 0, 1, 2, 40, 49, 50, 51, 99, 100, 101,
	    150, 199, 200, 300, 800, 1600, 2000, 4000 };
	static const int po[] = { -100, -1, 0, 1, 2, 40, 49, 50, 51, 99, 100, 101,
	    150, 199, 200, 300, 800, 1600, 2000, 4000 };
	static const int ln[] = { -3, -1, 0, 1, 2, 3, 40, 50, 100, 101, 200, 800,
	    1600, 4000, 5, 7 };
	for (i0 = 0; i0 < N(sp); i0++)
	for (i1 = 0; i1 < N(sp); i1++)
	for (i2 = 0; i2 < N(po); i2++)
	for (i3 = 0; i3 < N(ln); i3++) {
		snprintf(lab, sizeof lab, "span %d %d %d %d",
		    sp[i0], sp[i1], po[i2], ln[i3]);
		one(lab,
		    ref_policy_span(sp[i0], sp[i1], po[i2], ln[i3]),
		    ribbon_policy_span(sp[i0], sp[i1], po[i2], ln[i3]));
	}
	}

	/* 10. How much a panel takes off one edge. */
	{
	static const int sr[] = { -100, -1, 0, 1, 2, 28, 40, 60, 100, 400, 800, 801,
	    900, 1000, 2000, 4000 };
	static const int sc[] = { -10, 0, 1, 40, 100, 400, 800, 801, 1200, 1600, 4000, 7 };
	static const int po[] = { -100, -1, 0, 1, 8, 28, 40, 100, 400, 800, 1200, 2000, 3 };
	static const int ln[] = { -5, -1, 0, 1, 8, 28, 40, 100, 400, 720, 800, 1600, 4000 };
	static const int fr[] = { -1, 0, 1, 2 };
	for (i0 = 0; i0 < N(sr); i0++)
	for (i1 = 0; i1 < N(sc); i1++)
	for (i2 = 0; i2 < N(po); i2++)
	for (i3 = 0; i3 < N(ln); i3++)
	for (i4 = 0; i4 < N(fr); i4++) {
		snprintf(lab, sizeof lab, "reserve %d %d %d %d %d",
		    sr[i0], sc[i1], po[i2], ln[i3], fr[i4]);
		one(lab,
		    ref_policy_reserve(sr[i0], sc[i1], po[i2], ln[i3], fr[i4]),
		    ribbon_policy_reserve(sr[i0], sc[i1], po[i2], ln[i3], fr[i4]));
	}
	}

	/* 11. What two panels facing each other are left with. */
	{
	static const int nf[] = { -100, -5, -1, 0, 1, 2, 8, 20, 28, 39, 40, 41, 50,
	    60, 100, 200, 400, 800, 1600, 4000 };
	static const int ln[] = { -100, -5, -1, 0, 1, 2, 20, 39, 40, 41, 56, 100,
	    400, 800, 1600, 4000 };
	static const int wf[] = { -1, 0, 1, 2 };
	for (i0 = 0; i0 < N(nf); i0++)
	for (i1 = 0; i1 < N(nf); i1++)
	for (i2 = 0; i2 < N(ln); i2++)
	for (i3 = 0; i3 < N(wf); i3++) {
		snprintf(lab, sizeof lab, "pair %d %d %d %d",
		    nf[i0], nf[i1], ln[i2], wf[i3]);
		one(lab,
		    ref_policy_pair(nf[i0], nf[i1], ln[i2], wf[i3]),
		    ribbon_policy_pair(nf[i0], nf[i1], ln[i2], wf[i3]));
	}
	}

	if ((fclose(fa) != 0) || (fclose(fb) != 0)) {
		perror("fclose");
		return 2;
	}
	printf("%lld %lld\n", total, bad);

	return (bad == 0) ? 0 : 1;
}
