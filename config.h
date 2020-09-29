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
	.focusbuttons = 0,      /* bit mask of mousebuttons that focus windows */
	.raisebuttons = ~0,     /* bit mask of mousebuttons that raise windows */

	/* behavior of single maximized windows */
	.ignoregaps = 0,        /* whether to ignore outer gaps when a single window is maximized */
	.ignoreborders = 0      /* whether to ignore borders when a single window is maximized */
};

struct Dock dock = {
	.mode = DockBelow,
	.position = DockRight,
	.orientation = 1,       /* if nonzero, map dockapps from right to left */
	.size = 64,

	.gapsides = 0,          /* the gap on the edges of the dock */
	.gapback = 0            /* the gap on the back of the dock */
};
