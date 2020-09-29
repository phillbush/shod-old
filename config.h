struct Config config = {
	.urgent_color = "#2E3436",
	.focused_color = "#3584E4",
	.unfocused_color = "#E6E6E6",

	.border_width = 10,
	.gapleft = 9,
	.gapright = 9,
	.gaptop = 9,
	.gapbottom = 9,
	.gapinner = 9,

	/* mouse buttons */
	.modifier = Mod1Mask,   /* modifier pressed with mouse button */
	.focusbuttons = 1,      /* bit mask of mousebuttons that focus windows */
	.raisebuttons = 1,      /* bit mask of mousebuttons that raise windows */

	/* behavior of single maximized windows */
	.ignoregaps = 1,        /* whether to ignore outer gaps when a single window is maximized */
	.ignoreborders = 1,     /* whether to ignore borders when a single window is maximized */

	/* dock configuration */
	.dockmode = DockBelow,  /* DockBelow or DockAside */
	.dockside = DockRight,  /* DockTop, DockBottom, DockLeft, or DockRight */
	.dockplace = 1,         /* if nonzero, map dockapps from right to left */
	.docksize = 64,
};
