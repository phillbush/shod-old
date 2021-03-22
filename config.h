struct Config config = {
	/* gaps */
	.gapinner      = 0,     /* gap between windows */
	.gapouter      = 0,     /* gap between window and screen edges */

	/* behavior of tiled windows */
	.ignoregaps    = 1,     /* whether to ignore outer gaqps when a single window is tiled */
	.ignoretitle   = 1,     /* whether to ignore title bar when a single window is tiled */
	.ignoreborders = 1,     /* whether to ignore borders when a single window is tiled */
	.mergeborders  = 0,     /* whether to merge borders of tiled windows */
	.hidetitle     = 0,     /* whether to hide title bar */

	/* mouse control (these configuration cannot be set via X resources) */
	.modifier = Mod1Mask,   /* modifier pressed with mouse button */
	.focusbuttons = 1,      /* bit mask of mouse buttons that focus windows */
	.raisebuttons = 1,      /* bit mask of mouse buttons that raise windows */
};
