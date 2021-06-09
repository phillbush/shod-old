struct Config config = {
	/* font, in the old X Logical Font Description style */
	.font          = "-misc-fixed-medium-r-semicondensed--13-120-75-75-c-60-iso8859-1",

	/* gaps */
	.gapinner      = 0,     /* gap between windows */
	.gapouter      = 0,     /* gap between window and screen edges */

	/* behavior of general windows */
	.hidetitle     = 0,     /* whether to hide title bar */

	/* behavior of tiled windows */
	.ignoregaps    = 0,     /* whether to ignore outer gaqps when a single window is tiled */
	.ignoretitle   = 0,     /* whether to ignore title bar when a single window is tiled */
	.ignoreborders = 0,     /* whether to ignore borders when a single window is tiled */
	.mergeborders  = 0,     /* whether to merge borders of tiled windows */

	/* whether to ignore requests from indirect sources, accept only direct requests */
	.ignoreindirect = 0,

	/* mouse control (these configuration cannot be set via X resources) */
	.modifier = Mod1Mask,   /* modifier pressed with mouse button */
	.focusbuttons = 1,      /* bit mask of mouse buttons that focus windows */
	.raisebuttons = 1,      /* bit mask of mouse buttons that raise windows */

	/* number of desktops */
	.ndesktops = 10,

	/* notification placement */
	.notifgravity = "NE",   /* where in the screen to place notification windows */
	.notifgap = 3,          /* gap between notification windows */
};
