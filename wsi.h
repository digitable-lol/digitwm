/* SPDX-FileCopyrightText: 2026 Digitable <https://digitable.life> */
/* SPDX-License-Identifier: ISC */
/*
 * digitwm - the window system, as the ribbon asks for it
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
 * The seam between the arithmetic of the ribbon and the window system under
 * it.  ribbon.c decides where every window belongs; the eleven calls below
 * are the whole of what it needs somebody else to do about that decision.
 *
 * Nothing else of the window system reaches ribbon.c, and that is checked by
 * a compiler rather than by reading: "sh tools/no-x-build.sh" builds ribbon.c
 * against a stub of X11 that declares no X11 function at all, and fails if
 * ribbon.o comes out needing an X name, or needing a window-system call that
 * is not declared here.  CI runs it.
 *
 * So this header is a port's work order and its whole surface.  A macOS layer
 * over the Accessibility API implements these eleven and nothing more; the
 * four thousand-odd lines of xevents.c, screen.c and the EWMH half of
 * xutil.c are not ported to it, they are dropped.
 *
 * Each entry says what it has to guarantee and - which is the half that gets
 * forgotten - what it is not asked for.  The ribbon calls most of them in a
 * loop over every window of every column of every ribbon, so an implementation
 * that promises more than is written here pays for the promise on every
 * scroll.  Where the ribbon can avoid asking at all, it does: see
 * client_geom_current() below.
 *
 * One dependency here is not a call.  The ribbon reads the workable area of
 * an output out of struct region_ctx (field "work": the viewable area with
 * panel struts already taken off) and walks the list of them on struct
 * screen_ctx.  A port owes those two the same way it owes the calls: a list
 * of outputs, each with a stable name and a rectangle that a panel has
 * already been subtracted from.  The name matters more than it looks - a
 * ribbon is bound to an output by name, because the name is what survives a
 * cable being pulled.
 */

#ifndef _WSI_H_
#define _WSI_H_

struct client_ctx;
struct region_ctx;
struct screen_ctx;

/*
 * Geometry.
 *
 * The ribbon owns geometry outright: it writes cc->geom and then asks for it
 * to be made true.  Nothing may move a ribboned window behind the ribbon's
 * back, and the ribbon never asks the window system where a window is - it
 * already knows.
 */

/*
 * Has this window already been told to stand where cc->geom now says?
 *
 * Must guarantee: 0 whenever the window has not been told, or was told
 * something else.  A false 1 loses the move outright, so when in doubt the
 * answer is 0.
 *
 * Not asked for: any statement about where the window actually is.  A client
 * that ignored what it was told, or a user who dragged it, is not this
 * question - the ribbon asks what was sent, not what happened.
 *
 * This is the one call in the contract that exists purely to avoid another
 * one.  On X11 it saves a buffered request; on a window system where moving a
 * window is a synchronous round trip into another process, it is the
 * difference the call was written for.
 */
int			 client_geom_current(struct client_ctx *);

/*
 * Put the window where cc->geom says.
 *
 * Must guarantee: the move is on its way, and a later client_geom_current()
 * answers 1 for this geometry.  The second argument is 0 from the ribbon
 * always, meaning "do not clear the maximised state while you are at it";
 * a port with no such state can ignore it.
 *
 * Not asked for: that the window has arrived by the time the call returns,
 * that the client agreed to the size, or that anything was redrawn.  The
 * ribbon is happy to be lied to about timing and never about the fact.
 */
void			 client_resize(struct client_ctx *, int);

/*
 * Off the screen, and back.
 *
 * The ribbon parks what has scrolled out of the viewport when "ribbonhide" is
 * on, and it only ever brings back what it parked itself: a window hidden
 * because its group is hidden is not the ribbon's to show.
 */

/*
 * Take the window off the screen without unmanaging it.
 *
 * Must guarantee: the window stops being drawn, keeps its place in the
 * ribbon's model, and can be brought back by client_show() with the geometry
 * it had.  If it held the input focus, the focus goes away with it rather
 * than to some window of the window system's choosing.
 *
 * Not asked for: iconification as the platform understands it, an animation,
 * or telling the client anything it can act on.  A window the ribbon parks
 * has no idea it was parked, and that is deliberate - it will be back within
 * one scroll.
 */
void			 client_hide(struct client_ctx *);

/*
 * Put it back on the screen.
 *
 * Must guarantee: the window is drawn again and is above what it was above
 * before.
 *
 * Not asked for: focus.  Showing is not activating; the ribbon asks for that
 * separately when it means it.
 */
void			 client_show(struct client_ctx *);

/*
 * Stacking and focus.
 */

/*
 * Raise the window above its siblings.
 *
 * Must guarantee: nothing overlaps it afterwards that did not overlap it from
 * above before.  Columns do not overlap by construction, so the ribbon uses
 * this only where they can: a window entering or leaving the floating layer.
 *
 * Not asked for: focus, and any particular order among the windows it was
 * raised over.
 */
void			 client_raise(struct client_ctx *);

/*
 * Give this window the keyboard.
 *
 * Must guarantee: afterwards client_current() on the same screen answers with
 * this window, and typing arrives at it.
 *
 * Not asked for: raising, showing, or moving the pointer - the ribbon does
 * those itself, in that order, when it wants them.  Implementations are
 * allowed to notice the change and tell the ribbon about it through
 * ribbon_client_focus(); the ribbon is written to survive being told about a
 * focus it asked for.
 */
void			 client_set_active(struct client_ctx *);

/*
 * Which window on this screen has the keyboard, NULL if none has.
 *
 * Must guarantee: the answer the window system would act on, not the one the
 * ribbon last asked for.  These differ exactly when something outside the
 * ribbon moved the focus, which is the case the ribbon is asking about.
 *
 * Not asked for: anything about windows that are not managed, and any answer
 * at all for a screen that has no windows.
 */
struct client_ctx	*client_current(struct screen_ctx *);

/*
 * The pointer.
 *
 * cwm gives focus to whatever the pointer is over.  That makes the pointer
 * part of the layout: a scroll that slides another window under a resting
 * pointer hands the focus away and the ribbon scrolls straight back.  So the
 * ribbon carries the pointer with the focus, and remembers where in a window
 * it stood.
 */

/*
 * Remember where the pointer stands inside this window.
 *
 * Must guarantee: a later client_ptr_warp() on the same window puts the
 * pointer back at the remembered spot.  A pointer that is not inside the
 * window at all is remembered as its centre - the point of the memory is that
 * the window keeps a sensible spot, not that the fact is recorded faithfully.
 *
 * Not asked for: that the spot survives the window being resized, or that
 * anything is remembered across a restart.
 */
void			 client_ptr_save(struct client_ctx *);

/*
 * Move the pointer into this window, to the remembered spot.
 *
 * Must guarantee: the pointer ends up inside the window.
 *
 * Not asked for: focus - though on a window system that follows the pointer
 * this call is how focus in fact arrives, and the ribbon relies on the two
 * agreeing.  A port where the pointer cannot be moved at all still satisfies
 * this contract by doing nothing here, provided that on that platform focus
 * does not follow the pointer either.  Those two are one decision, not two.
 */
void			 client_ptr_warp(struct client_ctx *);

/*
 * Which output the pointer is over, NULL if it is over none.
 *
 * Must guarantee: the output whose viewable rectangle contains the pointer
 * right now, by the same list of outputs the ribbon binds itself to.
 *
 * Not asked for: a cheap answer - the ribbon asks once per command, never in
 * a loop - and no answer at all is a legal one: the caller then falls back to
 * the first attached ribbon.
 *
 * This call exists to keep a window handle out of ribbon.c.  It used to be
 * two calls, "where is the pointer on the root window" and "which output is
 * that", and the first of them made the ribbon name a window of the window
 * system's own - a leak a symbol table cannot see, because a type is not a
 * symbol.
 */
struct region_ctx	*region_pointer(struct screen_ctx *);

/*
 * Let the moves just made settle, and swallow the focus change they caused.
 *
 * Must guarantee: when the call returns, no focus change caused by geometry
 * the ribbon itself has just pushed is still on its way in.  That, and
 * nothing else, is what the ribbon buys here: left undone, the layout
 * oscillates - the scroll moves a window under the pointer, the crossing
 * event moves the focus back, and the next scroll undoes the first.
 *
 * Not asked for: delivering anything, touching a focus change a human caused,
 * being cheap, or saying anything at all about windows the ribbon did not
 * move.
 *
 * This is the one entry in this header that a port cannot implement by
 * translating ours, and it is worth being plain about why.  On X11 the
 * guarantee is three lines (xutil.c): XSync() is a round trip, and the
 * protocol has it that the events a request generates are queued before the
 * reply to any request issued after it - so "already caused" and "already in
 * our queue" are the same set, and the set can simply be drained.  The
 * ribbon is therefore relying on a semantic, not on three function names:
 * synchronous delivery.  A window system that has no round trip and whose
 * notifications arrive after the fact - macOS, where AXObserverCallback is
 * declared void and the notification is not seen until the move has already
 * happened - cannot answer this question by waiting, at any timeout: the set
 * is not closed.  The same guarantee has to be built the other way round
 * there: the mover tags the moves it makes, and the notification handler
 * drops what carries a tag, instead of the ribbon draining a queue.  Same
 * contract, other mechanism, and it is the only place in this header where
 * the port is not a translation.
 */
void			 wsi_settle(void);

#endif /* _WSI_H_ */
