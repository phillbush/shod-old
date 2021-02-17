struct Config config = {
	/* window borders */
	.urgent_color = "#2E3436",
	.focused_color = "#3584E4",
	.unfocused_color = "#E6E6E6",
	.border_style  = BorderSolid,
	.border_width  = 3,     /* width of the border around windows */
	.corner_width  = 20,    /* maximum width of the border corners */

	/* gaps */
	.gapinner      = 0,     /* gap between windows */
	.gapouter      = 0,     /* gap between window and screen edges */

	/* behavior of single maximized windows */
	.ignoregaps    = 0,     /* whether to ignore outer gaqps when a single window is maximized */
	.ignoreborders = 0,     /* whether to ignore borders when a single window is maximized */

	/* mouse control (these configuration cannot be set via X resources) */
	.modifier = Mod1Mask,   /* modifier pressed with mouse button */
	.focusbuttons = 1,      /* bit mask of mouse buttons that focus windows */
	.raisebuttons = 1,      /* bit mask of mouse buttons that raise windows */
};
