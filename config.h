struct Config config = {
	/* font, separate different fonts with comma */
	.font = "monospace:size=9,DejaVuSansMono:size=9",

	.urgentBG_color = "#2E3436",
	.urgentFG_color = "#000000",
	.focusedBG_color = "#3584E4",
	.focusedFG_color = "#000000",
	.unfocusedBG_color = "#E6E6E6",
	.unfocusedFG_color = "#000000",

	.title_height = 20,     /* width of the title bar on the top of windows */
	.border_width = 3,      /* width of the border around windows */
	.gapleft = 9,           /* gap to the left */
	.gapright = 9,          /* gap to the right */
	.gaptop = 9,            /* gap to the top */
	.gapbottom = 9,         /* gap to the bottom */
	.gapinner = 9,          /* gap between tiled windows */

	/* mouse buttons */
	.modifier = Mod1Mask,   /* modifier pressed with mouse button */
	.focusbuttons = 1,      /* bit mask of mousebuttons that focus windows */
	.raisebuttons = 1,      /* bit mask of mousebuttons that raise windows */

	/* behavior of single maximized windows */
	.ignoregaps = 1,        /* whether to ignore outer gaps when a single window is maximized */
	.ignoreborders = 1,     /* whether to ignore borders when a single window is maximized */

	/* titlebar configuration */
	.ignoretitle = 1,       /* whether to ignore title bars of windows */
	.titlealign = TitleCenter,

	/* dock configuration */
	.dockmode = DockBelow,  /* DockBelow or DockAside */
	.dockside = DockRight,  /* DockTop, DockBottom, DockLeft, or DockRight */
	.dockplace = DockEnd,   /* DockBegin, DockCenter, or DockEnd */
	.dockinverse = 1,       /* if nonzero, map dockapps from end to begin */
	.dockwidth = 64,        /* size of each dockapp */
	.dockborder = 0         /* (TODO) size of the dock border */
};
