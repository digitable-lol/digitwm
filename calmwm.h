/*
 * calmwm - the calm window manager
 *
 * Copyright (c) 2004 Marius Aamodt Eriksen <marius@monkey.org>
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
 *
 * $OpenBSD$
 */

#ifndef _CALMWM_H_
#define _CALMWM_H_

#include <sys/param.h>
#include <stdio.h>
#include "queue.h"

/* prototypes for portable-included functions */
char *fgetln(FILE *, size_t *);
long long strtonum(const char *, long long, long long, const char **);
void *reallocarray(void *, size_t, size_t);


#ifdef strlcat
#define HAVE_STRLCAT
#else
size_t strlcat(char *, const char *, size_t);
#endif
#ifdef strlcpy
#define HAVE_STRLCPY
#else
size_t strlcpy(char *, const char *, size_t);
#endif

#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <X11/Xlib.h>
#include <X11/Xproto.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xrandr.h>
#include <X11/keysym.h>

#define LOG_DEBUG0(...)	log_debug(0, __func__, __VA_ARGS__)
#define LOG_DEBUG1(...)	log_debug(1, __func__, __VA_ARGS__)
#define LOG_DEBUG2(...)	log_debug(2, __func__, __VA_ARGS__)
#define LOG_DEBUG3(...)	log_debug(3, __func__, __VA_ARGS__)

#undef MIN
#undef MAX
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#ifndef nitems
#define nitems(_a) (sizeof((_a)) / sizeof((_a)[0]))
#endif

#define BUTTONMASK	(ButtonPressMask | ButtonReleaseMask)
#define MOUSEMASK	(BUTTONMASK | PointerMotionMask)
#define IGNOREMODMASK	(LockMask | Mod2Mask | 0x2000)

/* direction/amount */
#define CWM_UP			0x0001
#define CWM_DOWN		0x0002
#define CWM_LEFT		0x0004
#define CWM_RIGHT		0x0008
#define CWM_BIGAMOUNT		0x0010
#define CWM_CENTER		0x0020
#define CWM_UP_BIG		(CWM_UP | CWM_BIGAMOUNT)
#define CWM_DOWN_BIG		(CWM_DOWN | CWM_BIGAMOUNT)
#define CWM_LEFT_BIG		(CWM_LEFT | CWM_BIGAMOUNT)
#define CWM_RIGHT_BIG		(CWM_RIGHT | CWM_BIGAMOUNT)
#define CWM_UP_RIGHT		(CWM_UP | CWM_RIGHT)
#define CWM_UP_LEFT		(CWM_UP | CWM_LEFT)
#define CWM_DOWN_RIGHT		(CWM_DOWN | CWM_RIGHT)
#define CWM_DOWN_LEFT		(CWM_DOWN | CWM_LEFT)

#define CWM_CYCLE_FORWARD	0x0001
#define CWM_CYCLE_REVERSE	0x0002
#define CWM_CYCLE_INGROUP	0x0004
#define CWM_CYCLE_INCLASS	0x0008

enum cwm_status {
	CWM_QUIT,
	CWM_RUNNING,
	CWM_EXEC_WM
};
enum cursor_font {
	CF_NORMAL,
	CF_MOVE,
	CF_RESIZE,
	CF_QUESTION,
	CF_NITEMS
};
enum color {
	CWM_COLOR_BORDER_ACTIVE,
	CWM_COLOR_BORDER_INACTIVE,
	CWM_COLOR_BORDER_URGENCY,
	CWM_COLOR_BORDER_GROUP,
	CWM_COLOR_BORDER_UNGROUP,
	CWM_COLOR_MENU_FG,
	CWM_COLOR_MENU_BG,
	CWM_COLOR_MENU_FONT,
	CWM_COLOR_MENU_FONT_SEL,
	CWM_COLOR_NITEMS
};

struct ribbon_col;

struct geom {
	int		 x;
	int		 y;
	int		 w;
	int		 h;
};
struct gap {
	int		 top;
	int		 bottom;
	int		 left;
	int		 right;
};

/*
 * What a panel takes off the screen: _NET_WM_STRUT_PARTIAL, in the order the
 * specification writes it.  A window says how deep it eats into each edge and
 * over which stretch of that edge, so that a panel spanning one monitor of
 * three does not shrink the other two.  Plain _NET_WM_STRUT is read into the
 * same fields with the spans opened to the whole screen, because that is what
 * the four-number form means.
 */
struct strut {
	int		 left;
	int		 right;
	int		 top;
	int		 bottom;
	int		 left_start_y;
	int		 left_end_y;
	int		 right_start_y;
	int		 right_end_y;
	int		 top_start_x;
	int		 top_end_x;
	int		 bottom_start_x;
	int		 bottom_end_x;
};

struct winname {
	TAILQ_ENTRY(winname)	 entry;
	char			*name;
};
TAILQ_HEAD(name_q, winname);
TAILQ_HEAD(ignore_q, winname);

struct client_ctx {
	TAILQ_ENTRY(client_ctx)	 entry;
	TAILQ_ENTRY(client_ctx)	 rbentry; /* stack of the holding column */
	struct screen_ctx	*sc;
	struct group_ctx	*gc;
	struct ribbon_col	*rbcol; /* column holding it, NULL if floating */
	struct geom		 rbgeom; /* geometry in ribbon coordinates */
	struct strut		 strut; /* screen edges this window reserves */
	Window			 win;
	Colormap		 colormap;
	int			 bwidth; /* border width */
	int			 obwidth; /* original border width */
	struct geom		 geom, savegeom, fullgeom;
	/*
	 * The geometry this window was last actually given, as opposed to the
	 * one it should have.  They differ only while a change is pending, and
	 * the point of keeping the pair is to notice when they do not differ
	 * at all: then there is nothing to send.  See client_resize().
	 */
	struct geom		 sentgeom;
	struct {
		long		 flags;	/* defined hints */
		int		 basew;	/* desired width */
		int		 baseh;	/* desired height */
		int		 minw;	/* minimum width */
		int		 minh;	/* minimum height */
		int		 maxw;	/* maximum width */
		int		 maxh;	/* maximum height */
		int		 incw;	/* width increment progression */
		int		 inch;	/* height increment progression */
		float		 mina;	/* minimum aspect ratio */
		float		 maxa;	/* maximum aspect ratio */
	} hint;
	struct {
		int		 x;	/* x position */
		int		 y;	/* y position */
	} ptr;
	struct {
		int		 h;	/* height */
		int		 w;	/* width */
	} dim;
#define CLIENT_HIDDEN			0x0001
#define CLIENT_IGNORE			0x0002
#define CLIENT_VMAXIMIZED		0x0004
#define CLIENT_HMAXIMIZED		0x0008
#define CLIENT_FREEZE			0x0010
#define CLIENT_GROUP			0x0020
#define CLIENT_UNGROUP			0x0040
#define CLIENT_INPUT			0x0080
#define CLIENT_WM_DELETE_WINDOW		0x0100
#define CLIENT_WM_TAKE_FOCUS		0x0200
#define CLIENT_URGENCY			0x0400
#define CLIENT_FULLSCREEN		0x0800
#define CLIENT_STICKY			0x1000
#define CLIENT_ACTIVE			0x2000
#define CLIENT_SKIP_PAGER		0x4000
#define CLIENT_SKIP_TASKBAR		0x8000
#define CLIENT_TRANSIENT		0x10000
#define CLIENT_TYPE_DIALOG		0x20000
#define CLIENT_TYPE_DOCK		0x40000
#define CLIENT_RIBBON			0x80000
#define CLIENT_RIBBON_PARKED		0x100000
#define CLIENT_GEOM_SENT		0x200000

#define CLIENT_SKIP_CYCLE		(CLIENT_HIDDEN | CLIENT_IGNORE | \
					 CLIENT_SKIP_TASKBAR | CLIENT_SKIP_PAGER)
#define CLIENT_HIGHLIGHT		(CLIENT_GROUP | CLIENT_UNGROUP)
#define CLIENT_MAXFLAGS			(CLIENT_VMAXIMIZED | CLIENT_HMAXIMIZED)
#define CLIENT_MAXIMIZED		(CLIENT_VMAXIMIZED | CLIENT_HMAXIMIZED)
	int			 flags;
	int			 stackingorder;
	struct name_q		 nameq;
	char			*name;
	char			*label;
	char			*res_class; /* class hint */
	char			*res_name; /* class hint */
	int			 initial_state; /* wm hint */
};
TAILQ_HEAD(client_q, client_ctx);

struct group_ctx {
	TAILQ_ENTRY(group_ctx)	 entry;
	struct screen_ctx	*sc;
	char			*name;
	int			 num;
};
TAILQ_HEAD(group_q, group_ctx);

struct autogroup {
	TAILQ_ENTRY(autogroup)	 entry;
	char			*class;
	char			*name;
	int 			 num;
};
TAILQ_HEAD(autogroup_q, autogroup);

struct region_ctx {
	TAILQ_ENTRY(region_ctx)	 entry;
	int			 num;
	char			*name; /* RandR output name, stable over hotplug */
	struct geom		 view; /* viewable area */
	struct geom		 work; /* workable area, gap-applied */
};
TAILQ_HEAD(region_q, region_ctx);

/*
 * The ribbon: an endless row of columns, of which the viewport shows a
 * stretch.  A column keeps its own width and an ordered stack of windows;
 * the ribbon keeps the order of columns and two numbers, the offsets of the
 * viewport across it.  Opening a window never changes the ribbon geometry of
 * any window already there - it only ever appends to the row.
 *
 * Two offsets, because the row and the stacks together make a canvas and the
 * viewport slides over both axes of it.  The canvas is as wide as the row of
 * columns (len) and as tall as the tallest stack on it (canvas): a stack that
 * outgrows the viewport used to end up below the edge with nothing able to
 * reach it, and the vertical offset is what reaches it.
 */
#define RIBBON_NPRESET		4

/* Return codes of the insertion policy; the order is part of the model. */
#define RIBBON_PLACE_COLUMN	0	/* a fresh column, right of focus */
#define RIBBON_PLACE_STACK	1	/* down the focused column */
#define RIBBON_PLACE_FLOAT	2	/* not the ribbon's business */
#define RIBBON_PLACE_FULL	3	/* fullscreen */

/* Configuration rule fed to the insertion policy. */
#define RIBBON_RULE_NONE	0
#define RIBBON_RULE_STACK	1
#define RIBBON_RULE_FLOAT	2

/*
 * One "ribbonrule" line of cwmrc: the place a window whose class - or whose
 * name and class - matches is opened in.  rule is one of the RIBBON_RULE_*
 * above, so the queue carries no number of its own: it is the model's field
 * "правило конфигурации" with a user in front of it.
 */
struct ribbonrule {
	TAILQ_ENTRY(ribbonrule)	 entry;
	char			*class;
	char			*name;
	int			 rule;
};
TAILQ_HEAD(ribbonrule_q, ribbonrule);

TAILQ_HEAD(rb_client_q, client_ctx);

struct ribbon_col {
	TAILQ_ENTRY(ribbon_col)	 entry;
	struct ribbon		*rb;
	struct rb_client_q	 winq;
	struct client_ctx	*focus; /* last focused window of the stack */
	int			 nwin;
	int			 preset; /* index into Conf.ribbonwidth */
	int			 x;	/* left edge along the ribbon */
	int			 w;	/* width in pixels */
	int			 h;	/* height of the stack, gaps included */
};
TAILQ_HEAD(ribbon_col_q, ribbon_col);

struct ribbon {
	TAILQ_ENTRY(ribbon)	 entry;
	struct screen_ctx	*sc;
	struct ribbon_col_q	 colq;
	struct ribbon_col	*focus;
	char			*output; /* RandR output this ribbon belongs to */
	struct geom		 view;	/* viewport, gap applied */
	int			 offset; /* viewport offset along the ribbon */
	int			 voffset; /* viewport offset down the canvas */
	int			 len;	/* total ribbon length in pixels */
	int			 canvas; /* canvas height: the tallest stack */
	int			 active; /* the output is currently attached */
};
TAILQ_HEAD(ribbon_q, ribbon);

struct screen_ctx {
	TAILQ_ENTRY(screen_ctx)	 entry;
	int			 which;
	Window			 rootwin;
	int			 cycling;
	int			 hideall;
	int			 snapdist;
	struct geom		 view; /* viewable area */
	struct geom		 work; /* workable area, gap-applied */
	struct gap		 gap;
	struct client_q		 clientq;
	struct region_q		 regionq;
	struct ribbon_q		 ribbonq;
	struct group_q		 groupq;
	struct group_ctx	*group_active;
	struct group_ctx	*group_last;
	Colormap		 colormap;
	Visual			*visual;
	struct {
		Window		 win;
		XftDraw		*xftdraw;
	} prop;
	XftColor		 xftcolor[CWM_COLOR_NITEMS];
	XftFont			*xftfont;
};
TAILQ_HEAD(screen_q, screen_ctx);

struct cargs {
	char		*cmd;
	int		 flag;
	enum {
		CWM_XEV_KEY,
		CWM_XEV_BTN
	} xev;
};
enum context {
	CWM_CONTEXT_NONE = 0,
	CWM_CONTEXT_CC,
	CWM_CONTEXT_SC
};
struct bind_ctx {
	TAILQ_ENTRY(bind_ctx)	 entry;
	void			(*callback)(void *, struct cargs *);
	struct cargs		*cargs;
	enum context		 context;
	unsigned int		 modmask;
	union {
		KeySym		 keysym;
		unsigned int	 button;
	} press;
};
TAILQ_HEAD(keybind_q, bind_ctx);
TAILQ_HEAD(mousebind_q, bind_ctx);

struct cmd_ctx {
	TAILQ_ENTRY(cmd_ctx)	 entry;
	char			*name;
	char			*path;
};
TAILQ_HEAD(cmd_q, cmd_ctx);
TAILQ_HEAD(wm_q, cmd_ctx);

#define CWM_MENU_DUMMY		0x0001
#define CWM_MENU_FILE		0x0002
#define CWM_MENU_LIST		0x0004
#define CWM_MENU_WINDOW_ALL	0x0008
#define CWM_MENU_WINDOW_HIDDEN	0x0010

struct menu {
	TAILQ_ENTRY(menu)	 entry;
	TAILQ_ENTRY(menu)	 resultentry;
#define MENU_MAXENTRY		 200
	char			 text[MENU_MAXENTRY + 1];
	char			 print[MENU_MAXENTRY + 1];
	void			*ctx;
	short			 dummy;
	short			 abort;
};
TAILQ_HEAD(menu_q, menu);

struct conf {
	struct keybind_q	 keybindq;
	struct mousebind_q	 mousebindq;
	struct autogroup_q	 autogroupq;
	struct ignore_q		 ignoreq;
	struct ribbonrule_q	 ribbonruleq;
	struct cmd_q		 cmdq;
	struct wm_q		 wmq;
	int			 ngroups;
	int			 stickygroups;
	int			 nameqlen;
	int			 bwidth;
	int			 mamount;
	int			 snapdist;
	int			 htile;
	int			 vtile;
	int			 ribbon;	/* ribbon layout in charge */
	int			 ribbonhide;	/* unmap what the viewport hides */
	int			 ribbongap;	/* between columns and windows */
	int			 ribbonminw;	/* narrowest a column may get */
	int			 ribbonminh;	/* shortest a window may get */
	int			 ribbonwidth[RIBBON_NPRESET]; /* percent presets */
	struct gap		 gap;
	char			*color[CWM_COLOR_NITEMS];
	char			*font;
	char			*wmname;
	Cursor			 cursor[CF_NITEMS];
	int			 xrandr;
	int			 xrandr_event_base;
	char			*conf_file;
	char			*known_hosts;
	char			*wm_argv;
	int			 debug;
};

/* MWM hints */
struct mwm_hints {
#define MWM_HINTS_ELEMENTS	5L

#define MWM_HINTS_FUNCTIONS	(1L << 0)
#define MWM_HINTS_DECORATIONS	(1L << 1)
#define MWM_HINTS_INPUT_MODE	(1L << 2)
#define MWM_HINTS_STATUS	(1L << 3)
	unsigned long	flags;

#define MWM_FUNC_ALL		(1L << 0)
#define MWM_FUNC_RESIZE		(1L << 1)
#define MWM_FUNC_MOVE		(1L << 2)
#define MWM_FUNC_MINIMIZE	(1L << 3)
#define MWM_FUNC_MAXIMIZE	(1L << 4)
#define MWM_FUNC_CLOSE		(1L << 5)
	unsigned long	functions;

#define	MWM_DECOR_ALL		(1L << 0)
#define	MWM_DECOR_BORDER	(1L << 1)
#define MWM_DECOR_RESIZEH	(1L << 2)
#define MWM_DECOR_TITLE		(1L << 3)
#define MWM_DECOR_MENU		(1L << 4)
#define MWM_DECOR_MINIMIZE	(1L << 5)
#define MWM_DECOR_MAXIMIZE	(1L << 6)
	unsigned long	decorations;

#define MWM_INPUT_MODELESS			0
#define MWM_INPUT_PRIMARY_APPLICATION_MODAL	1
#define MWM_INPUT_SYSTEM_MODAL			2
#define MWM_INPUT_FULL_APPLICATION_MODAL	3
	long		inputMode;

#define MWM_TEAROFF_WINDOW	(1L << 0)
	unsigned long	status;
};

enum cwmh {
	WM_STATE,
	WM_DELETE_WINDOW,
	WM_TAKE_FOCUS,
	WM_PROTOCOLS,
	_MOTIF_WM_HINTS,
	UTF8_STRING,
	WM_CHANGE_STATE,
	CWMH_NITEMS
};
enum ewmh {
	_NET_SUPPORTED,
	_NET_SUPPORTING_WM_CHECK,
	_NET_ACTIVE_WINDOW,
	_NET_CLIENT_LIST,
	_NET_CLIENT_LIST_STACKING,
	_NET_NUMBER_OF_DESKTOPS,
	_NET_CURRENT_DESKTOP,
	_NET_DESKTOP_VIEWPORT,
	_NET_DESKTOP_GEOMETRY,
	_NET_VIRTUAL_ROOTS,
	_NET_SHOWING_DESKTOP,
	_NET_DESKTOP_NAMES,
	_NET_WORKAREA,
	_NET_WM_NAME,
	_NET_WM_DESKTOP,
	_NET_CLOSE_WINDOW,
	_NET_WM_STATE,
#define	_NET_WM_STATES_NITEMS	9
	_NET_WM_STATE_STICKY,
	_NET_WM_STATE_MAXIMIZED_VERT,
	_NET_WM_STATE_MAXIMIZED_HORZ,
	_NET_WM_STATE_HIDDEN,
	_NET_WM_STATE_FULLSCREEN,
	_NET_WM_STATE_DEMANDS_ATTENTION,
	_NET_WM_STATE_SKIP_PAGER,
	_NET_WM_STATE_SKIP_TASKBAR,
	_CWM_WM_STATE_FREEZE,
	_NET_WM_WINDOW_TYPE,
	_NET_WM_WINDOW_TYPE_DESKTOP,
	_NET_WM_WINDOW_TYPE_DOCK,
	_NET_WM_WINDOW_TYPE_TOOLBAR,
	_NET_WM_WINDOW_TYPE_MENU,
	_NET_WM_WINDOW_TYPE_UTILITY,
	_NET_WM_WINDOW_TYPE_SPLASH,
	_NET_WM_WINDOW_TYPE_DIALOG,
	_NET_WM_WINDOW_TYPE_NORMAL,
	_NET_WM_STRUT,
	_NET_WM_STRUT_PARTIAL,
	EWMH_NITEMS
};
enum net_wm_state {
	_NET_WM_STATE_REMOVE,
	_NET_WM_STATE_ADD,
	_NET_WM_STATE_TOGGLE
};

extern Display				*X_Dpy;
extern Time				 Last_Event_Time;
extern Atom				 cwmh[CWMH_NITEMS];
extern Atom				 ewmh[EWMH_NITEMS];
extern struct screen_q			 Screenq;
extern struct conf			 Conf;

void			 usage(void);

/*
 * Everything the ribbon asks of the window system is declared in wsi.h and
 * nowhere else - nine of the client_* prototypes that would otherwise stand
 * in the list below, region_pointer() and wsi_settle().  Split out because
 * that list is the contract a port to another window system implements, and a
 * contract nobody can point at is not one.  The names and signatures did not
 * change; only the header they live in did.
 */
#include "wsi.h"

void			 client_apply_sizehints(struct client_ctx *);
void			 client_close(struct client_ctx *);
void			 client_config(struct client_ctx *);
void			 client_draw_border(struct client_ctx *);
struct client_ctx	*client_find(Window);
void			 client_get_sizehints(struct client_ctx *);
void 			 client_htile(struct client_ctx *);
int			 client_inbound(struct client_ctx *, int, int);
struct client_ctx	*client_init(Window, struct screen_ctx *);
void			 client_lower(struct client_ctx *);
void			 client_geom_sent(struct client_ctx *);
void			 client_move(struct client_ctx *);
void			 client_mtf(struct client_ctx *);
struct client_ctx	*client_next(struct client_ctx *);
struct client_ctx	*client_prev(struct client_ctx *);
void			 client_ptr_inbound(struct client_ctx *, int);
void			 client_remove(struct client_ctx *);
void			 client_set_name(struct client_ctx *);
int			 client_snapcalc(int, int, int, int, int);
void			 client_toggle_hidden(struct client_ctx *);
void			 client_toggle_hmaximize(struct client_ctx *);
void			 client_toggle_fullscreen(struct client_ctx *);
void			 client_toggle_freeze(struct client_ctx *);
void			 client_toggle_maximize(struct client_ctx *);
void			 client_toggle_skip_pager(struct client_ctx *);
void			 client_toggle_skip_taskbar(struct client_ctx *);
void			 client_toggle_sticky(struct client_ctx *);
void			 client_toggle_vmaximize(struct client_ctx *);
void			 client_transient(struct client_ctx *);
void			 client_urgency(struct client_ctx *);
void			 client_wm_type(struct client_ctx *);
void			 client_wm_strut(struct client_ctx *);
int			 client_has_strut(struct client_ctx *);
void 			 client_vtile(struct client_ctx *);
void			 client_wm_hints(struct client_ctx *);

void			 group_assign(struct group_ctx *, struct client_ctx *);
int			 group_autogroup(struct client_ctx *);
void			 group_cycle(struct screen_ctx *, int);
void			 group_hide(struct group_ctx *);
int			 group_holds_only_hidden(struct group_ctx *);
int			 group_holds_only_sticky(struct group_ctx *);
void			 group_init(struct screen_ctx *, int, const char *);
void			 group_movetogroup(struct client_ctx *, int);
void			 group_only(struct screen_ctx *, int);
void			 group_close(struct screen_ctx *, int);
int			 group_restore(struct client_ctx *);
void			 group_show(struct group_ctx *);
void			 group_toggle(struct screen_ctx *, int);
void			 group_toggle_all(struct screen_ctx *);
void			 group_toggle_membership(struct client_ctx *);
void			 group_update_names(struct screen_ctx *);

void			 search_match_client(struct menu_q *, struct menu_q *,
			     char *);
void			 search_match_cmd(struct menu_q *, struct menu_q *,
			     char *);
void			 search_match_exec(struct menu_q *, struct menu_q *,
			     char *);
void			 search_match_group(struct menu_q *, struct menu_q *,
			     char *);
void			 search_match_path(struct menu_q *, struct menu_q *,
			     char *);
void			 search_match_text(struct menu_q *, struct menu_q *,
			     char *);
void			 search_match_wm(struct menu_q *, struct menu_q *,
			     char *);
void			 search_print_client(struct menu *, int);
void			 search_print_cmd(struct menu *, int);
void			 search_print_group(struct menu *, int);
void			 search_print_text(struct menu *, int);
void			 search_print_wm(struct menu *, int);

int			 ribbon_policy_offset(int, int, int, int, int, int);
int			 ribbon_policy_voffset(int, int, int, int, int, int);
int			 ribbon_policy_width(int, int, int, int);
int			 ribbon_policy_height(int, int, int, int, int);
int			 ribbon_policy_insert(int, int, int, int, int, int);
int			 ribbon_policy_close(int, int, int, int);
int			 ribbon_policy_output(int, int, int);
int			 ribbon_policy_span(int, int, int, int);
int			 ribbon_policy_reserve(int, int, int, int, int);
int			 ribbon_policy_pair(int, int, int, int);

struct ribbon		*ribbon_new(struct screen_ctx *, const char *);
void			 ribbon_free(struct ribbon *);
struct ribbon		*ribbon_find(struct screen_ctx *, const char *);
struct ribbon		*ribbon_current(struct screen_ctx *);
struct ribbon_col	*ribbon_col_new(struct ribbon *, struct ribbon_col *);
struct ribbon_col	*ribbon_col_at(struct ribbon *, int);
int			 ribbon_col_index(struct ribbon *, struct ribbon_col *);
int			 ribbon_col_count(struct ribbon *);
void			 ribbon_col_add(struct ribbon_col *,
			     struct client_ctx *);
void			 ribbon_measure(struct ribbon *);
void			 ribbon_place(struct ribbon *);
void			 ribbon_scroll(struct ribbon *);
void			 ribbon_sync(struct screen_ctx *);

void			 ribbon_screen_init(struct screen_ctx *);
void			 ribbon_screen_relayout(struct screen_ctx *);
void			 ribbon_screen_update(struct screen_ctx *);
struct ribbon_col	*ribbon_insert(struct ribbon *, int,
			     struct client_ctx *);
int			 ribbon_stack_reorder(struct ribbon_col *,
			     struct client_ctx *, int);
int			 ribbon_col_reorder(struct ribbon *,
			     struct ribbon_col *, int);
int			 ribbon_client_insert(struct client_ctx *);
void			 ribbon_client_remove(struct client_ctx *);
void			 ribbon_client_focus(struct client_ctx *);

void			 ribbon_focus_col(struct screen_ctx *, int);
void			 ribbon_focus_win(struct screen_ctx *, int);
void			 ribbon_move_client(struct client_ctx *, int);
void			 ribbon_move_win(struct client_ctx *, int);
void			 ribbon_swap_col(struct screen_ctx *, int);
void			 ribbon_width(struct client_ctx *, int);
void			 ribbon_center(struct screen_ctx *);
void			 ribbon_float_toggle(struct client_ctx *);

struct region_ctx	*region_find(struct screen_ctx *, int, int);
void			 screen_assert_clients_within(struct screen_ctx *);
struct geom		 screen_area(struct screen_ctx *, int, int, int);
struct screen_ctx	*screen_find(Window);
void			 screen_init(int);
void			 screen_prop_win_create(struct screen_ctx *, Window);
void			 screen_prop_win_destroy(struct screen_ctx *);
void			 screen_prop_win_draw(struct screen_ctx *,
			     const char *, ...)
			    __attribute__((__format__ (printf, 2, 3)))
			    __attribute__((__nonnull__ (2)));
void			 screen_update_geometry(struct screen_ctx *);
void			 screen_update_struts(struct screen_ctx *);
void			 screen_updatestackingorder(struct screen_ctx *);

void			 kbfunc_cwm_status(void *, struct cargs *);
void			 kbfunc_ptrmove(void *, struct cargs *);
void			 kbfunc_client_snap(void *, struct cargs *);
void			 kbfunc_client_move(void *, struct cargs *);
void			 kbfunc_client_resize(void *, struct cargs *);
void			 kbfunc_client_close(void *, struct cargs *);
void			 kbfunc_client_lower(void *, struct cargs *);
void			 kbfunc_client_raise(void *, struct cargs *);
void			 kbfunc_client_hide(void *, struct cargs *);
void			 kbfunc_client_toggle_freeze(void *, struct cargs *);
void			 kbfunc_client_toggle_sticky(void *, struct cargs *);
void			 kbfunc_client_toggle_fullscreen(void *,
			      struct cargs *);
void			 kbfunc_client_toggle_maximize(void *, struct cargs *);
void			 kbfunc_client_toggle_hmaximize(void *, struct cargs *);
void			 kbfunc_client_toggle_vmaximize(void *, struct cargs *);
void 			 kbfunc_client_htile(void *, struct cargs *);
void 			 kbfunc_client_vtile(void *, struct cargs *);
void			 kbfunc_ribbon_focus(void *, struct cargs *);
void			 kbfunc_ribbon_focus_win(void *, struct cargs *);
void			 kbfunc_ribbon_move(void *, struct cargs *);
void			 kbfunc_ribbon_move_win(void *, struct cargs *);
void			 kbfunc_ribbon_swap(void *, struct cargs *);
void			 kbfunc_ribbon_width(void *, struct cargs *);
void			 kbfunc_ribbon_center(void *, struct cargs *);
void			 kbfunc_ribbon_float(void *, struct cargs *);
void			 kbfunc_client_cycle(void *, struct cargs *);
void			 kbfunc_client_toggle_group(void *, struct cargs *);
void			 kbfunc_client_movetogroup(void *, struct cargs *);
void			 kbfunc_group_toggle(void *, struct cargs *);
void			 kbfunc_group_only(void *, struct cargs *);
void			 kbfunc_group_last(void *, struct cargs *);
void			 kbfunc_group_close(void *, struct cargs *);
void			 kbfunc_group_cycle(void *, struct cargs *);
void			 kbfunc_group_toggle_all(void *, struct cargs *);
void			 kbfunc_menu_client(void *, struct cargs *);
void			 kbfunc_menu_cmd(void *, struct cargs *);
void			 kbfunc_menu_group(void *, struct cargs *);
void			 kbfunc_menu_wm(void *, struct cargs *);
void			 kbfunc_menu_exec(void *, struct cargs *);
void			 kbfunc_menu_ssh(void *, struct cargs *);
void			 kbfunc_client_menu_label(void *, struct cargs *);
void			 kbfunc_exec_cmd(void *, struct cargs *);
void			 kbfunc_exec_lock(void *, struct cargs *);
void			 kbfunc_exec_term(void *, struct cargs *);

struct menu  		*menu_filter(struct screen_ctx *, struct menu_q *,
			     const char *, const char *, int,
			     void (*)(struct menu_q *, struct menu_q *, char *),
			     void (*)(struct menu *, int));
void			 menuq_add(struct menu_q *, void *, const char *, ...)
			    __attribute__((__format__ (printf, 3, 4)));
void			 menuq_clear(struct menu_q *);

int			 probe_is_command(const char *);
int			 probe_run(const char *);

int			 parse_config(const char *, struct conf *);

void			 conf_autogroup(struct conf *, int, const char *,
			     const char *);
int			 conf_bind_key(struct conf *, const char *,
    			     const char *);
int			 conf_bind_mouse(struct conf *, const char *,
    			     const char *);
void			 conf_clear(struct conf *);
void			 conf_client(struct client_ctx *);
void			 conf_cmd_add(struct conf *, const char *,
			     const char *);
void			 conf_wm_add(struct conf *, const char *,
			     const char *);
void			 conf_cursor(struct conf *);
void			 conf_grab_kbd(Window);
void			 conf_grab_mouse(Window);
void			 conf_init(struct conf *);
void			 conf_ignore(struct conf *, const char *);
int			 conf_ribbonrule(struct conf *, const char *,
			     const char *, const char *);
int			 conf_ribbonrule_match(struct client_ctx *);
void			 conf_screen(struct screen_ctx *);
void			 conf_group(struct screen_ctx *);

void			 xev_process(void);

int			 xu_get_prop(Window, Atom, Atom, long, unsigned char **);
int			 xu_get_strprop(Window, Atom, char **);
void			 xu_ptr_get(Window, int *, int *);
void			 xu_ptr_set(Window, int, int);
void			 xu_get_wm_state(Window, long *);
void			 xu_set_wm_state(Window, long);
void			 xu_send_clientmsg(Window, Atom, Time);
void 			 xu_xorcolor(XftColor, XftColor, XftColor *);

void			 xu_atom_init(void);
void			 xu_ewmh_net_supported(struct screen_ctx *);
void			 xu_ewmh_net_supported_wm_check(struct screen_ctx *);
void			 xu_ewmh_net_desktop_geometry(struct screen_ctx *);
void			 xu_ewmh_net_desktop_viewport(struct screen_ctx *);
void			 xu_ewmh_net_workarea(struct screen_ctx *);
void			 xu_ewmh_net_client_list(struct screen_ctx *);
void			 xu_ewmh_net_client_list_stacking(struct screen_ctx *);
void			 xu_ewmh_net_active_window(struct screen_ctx *, Window);
void			 xu_ewmh_net_number_of_desktops(struct screen_ctx *);
void			 xu_ewmh_net_showing_desktop(struct screen_ctx *);
void			 xu_ewmh_net_virtual_roots(struct screen_ctx *);
void			 xu_ewmh_net_current_desktop(struct screen_ctx *);
void			 xu_ewmh_net_desktop_names(struct screen_ctx *);
int			 xu_ewmh_get_net_wm_desktop(struct client_ctx *, long *);
void			 xu_ewmh_set_net_wm_desktop(struct client_ctx *);
Atom 			*xu_ewmh_get_net_wm_state(struct client_ctx *, int *);
void 			 xu_ewmh_handle_net_wm_state_msg(struct client_ctx *,
			     int, Atom, Atom);
void 			 xu_ewmh_set_net_wm_state(struct client_ctx *);
void 			 xu_ewmh_restore_net_wm_state(struct client_ctx *);

char			*u_argv(char * const *);
void			 u_exec(char *);
void			 u_spawn(char *);
void			 log_debug(int, const char *, const char *, ...)
			    __attribute__((__format__ (printf, 3, 4)))
			    __attribute__((__nonnull__ (3)));

void			*xcalloc(size_t, size_t);
void			*xmalloc(size_t);
void			*xreallocarray(void *, size_t, size_t);
char			*xstrdup(const char *);
int			 xasprintf(char **, const char *, ...)
			    __attribute__((__format__ (printf, 2, 3)))
			    __attribute__((__nonnull__ (2)));
int			 xvasprintf(char **, const char *, va_list)
			    __attribute__((__nonnull__ (2)));

#endif /* _CALMWM_H_ */
