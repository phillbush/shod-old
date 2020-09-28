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
	.raisebuttons = ~0      /* bit mask of mousebuttons that raise windows */
};

struct Dock dock = {
	.mode = DockBelow,
	.position = DockRight,
	.orientation = 0,       /* if nonzero, map dockapps from right to left */
	.size = 64,

	.gapsides = 0,          /* the gap on the edges of the dock */
	.gapback = 0            /* the gap on the back of the dock */
};
