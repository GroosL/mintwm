/*
 * mint-keysd configuration
 *
 * Customize keybindings, chord modes, and launcher commands here.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <xkbcommon/xkbcommon.h>

#define WLR_MODIFIER_SHIFT (1 << 0)
#define WLR_MODIFIER_CTRL  (1 << 2)
#define WLR_MODIFIER_ALT   (1 << 3)
#define WLR_MODIFIER_LOGO  (1 << 6)

/* Main modifier key (Logo / Super / Windows key) */
#define MODKEY WLR_MODIFIER_LOGO

/* Chord timeout in seconds */
#define CHORD_TIMEOUT_SECS 2

/*
 * Chord modes:
 * Add any custom modes here. MODE_NORMAL is the default state.
 */
enum chord_mode {
	MODE_NORMAL = 0,
	MODE_SUPER_D,
	MODE_SUPER_W,
	/* Example custom mode:
	 * MODE_SUPER_SPACE,
	 */
};

/* Helper macro for 1-9 workspace bindings */
#define WSKEYS(mode, mod, action) \
	{ mode, mod, XKB_KEY_1, action " 1", MODE_NORMAL }, \
	{ mode, mod, XKB_KEY_2, action " 2", MODE_NORMAL }, \
	{ mode, mod, XKB_KEY_3, action " 3", MODE_NORMAL }, \
	{ mode, mod, XKB_KEY_4, action " 4", MODE_NORMAL }, \
	{ mode, mod, XKB_KEY_5, action " 5", MODE_NORMAL }, \
	{ mode, mod, XKB_KEY_6, action " 6", MODE_NORMAL }, \
	{ mode, mod, XKB_KEY_7, action " 7", MODE_NORMAL }, \
	{ mode, mod, XKB_KEY_8, action " 8", MODE_NORMAL }, \
	{ mode, mod, XKB_KEY_9, action " 9", MODE_NORMAL }

/*
 * Keybindings table
 *
 * Fields:
 * 1. Active mode (e.g. MODE_NORMAL, MODE_SUPER_D)
 * 2. Modifiers   (e.g. MODKEY, MODKEY | WLR_MODIFIER_SHIFT, or 0)
 * 3. Key         (e.g. XKB_KEY_Return, XKB_KEY_d, XKB_KEY_q)
 * 4. Command     (e.g. "sh foot", "close", "workspace 1", or NULL if entering a chord)
 * 5. Next mode   (MODE_NORMAL to finish, or a MODE_* to enter a chord mode)
 */
static const struct Binding bindings[] = {
	/* Mode          Modifier                      Key               Command             Next Mode */

	/* --- Chord Triggers --- */
	{ MODE_NORMAL,   MODKEY,                       XKB_KEY_d,        NULL,               MODE_SUPER_D },
	{ MODE_NORMAL,   MODKEY,                       XKB_KEY_w,        NULL,               MODE_SUPER_W },

	/* --- Window Management --- */
	{ MODE_NORMAL,   MODKEY,                       XKB_KEY_Return,   "sh footclient",          MODE_NORMAL },
	{ MODE_NORMAL,   MODKEY | WLR_MODIFIER_SHIFT,  XKB_KEY_q,        "close",            MODE_NORMAL },
	{ MODE_NORMAL,   MODKEY,                       XKB_KEY_j,        "focus next",       MODE_NORMAL },
	{ MODE_NORMAL,   MODKEY,                       XKB_KEY_k,        "focus prev",       MODE_NORMAL },
	{ MODE_NORMAL,   MODKEY | WLR_MODIFIER_SHIFT,  XKB_KEY_Return,   "swap",             MODE_NORMAL },
	{ MODE_NORMAL,   MODKEY,                       XKB_KEY_f,        "fullscreen",       MODE_NORMAL },
	{ MODE_NORMAL,   MODKEY | WLR_MODIFIER_SHIFT,  XKB_KEY_e,        "exit",             MODE_NORMAL },

	/* --- Workspaces 1-9 --- */
	{ MODE_NORMAL,   MODKEY,                       XKB_KEY_Tab,      "workspace back",   MODE_NORMAL },
	WSKEYS(MODE_NORMAL, MODKEY,                        "workspace"),
	WSKEYS(MODE_NORMAL, MODKEY | WLR_MODIFIER_SHIFT,   "moveto"),

	/* --- Chord Mode: Super + d --- */
	{ MODE_SUPER_D,  0,                            XKB_KEY_d,        "sh dmenu_run_history",    MODE_NORMAL },
	{ MODE_SUPER_D,  0,                            XKB_KEY_p,        "sh passmenu2 -i",  MODE_NORMAL },
	{ MODE_SUPER_D,  0,							   XKB_KEY_c,		 "sh =",			 MODE_NORMAL},
	{ MODE_SUPER_D,  0,                            XKB_KEY_Escape,   NULL,               MODE_NORMAL },

	/* --- Chord Mode: Super + w --- */
	{ MODE_SUPER_W,  0,                            XKB_KEY_Tab,      "workspace back",   MODE_NORMAL },
	WSKEYS(MODE_SUPER_W, 0,                            "workspace"),
	{ MODE_SUPER_W,  0,                            XKB_KEY_Escape,   NULL,               MODE_NORMAL },
};

#endif /* CONFIG_H */
