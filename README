                                Shod

Shod is a hybrid (tiling and floating) reparenting X11 window manager
which supports tabs, multiple monitors, themes, and window rules.  Shod
sets no keybindings; reads no configuration other than X resources; and
works only via mouse with a given key modifier (Alt by default), and by
responding to client messages with EWMH hints (so it is needed wmctrl(1)
to control shod).

WARNING: Shod is attempt of mine to write a Wm and better understand how
         X11 works, Shod is only meant for me for educational purposes.
         Shod is an experimental project, and should be used with caution.

WARNING: You must have wmctrl(1) installed in order to control Shod.

Non-features:
• No maximization (Shod uses maximization hints to implement tiling).
• No tags/groups (Shod uses desktops rather than dwm-like tags).
• No viewports (Shod does not support fvwm-like viewports).
• No keybindings (Use sxhkd(1) for binding keys).
• No bar/panels (I do not use panels, so I add no support for them).

Features:
• Shod maps transient windows, like dialogs, the same way macos does:
  inside the frame of the main window.

• Shod can tab windows. Just move the tab from a window to another with
  the right mouse button.  Windows can be automatically tabbed by
  setting the `shod.autoTab` X resource (see the manual).

• Shod uses EWMH-based maximization to implement tiled windows; that is,
  when you maximize a window (seting BOTH '_NET_WM_STATE_MAXIMIZED_HORZ'
  and '_NET_WM_STATE_MAXIMIZED_VERT') it is tiled rather than maximized
  (that means that classic maximization isn't possible with shod).

• Shod uses a columnated window tiling style (the same used by wmii(1)
  and acme(1)).  In this style, each window occupies a row in a column.
  In order to change the column of a window, just move it left or right
  with wmctrl(1); and to swap a window with the one above or below, just
  move it up or down.

• Shod places floating windows in unoccupied regions of the monitor.
  The first floating window is spawned in the center of the monitor.

• Each monitor has its own set of workspaces.  For example, if you have
  two monitors you will have two desktops being shown at the same time,
  one on each monitor.  In order to change the selected monitor just
  click on a window or on the root window of that monitor.  In order to
  change the monitor of a window just move this window to another
  monitor or move this window to a workspace bound to another monitor.

• You can use the shod.focusButtons X resource to use either a
  focus-follow-pointer style or a click-to-focus style.  You can even
  specify which mouse button triggers the click-to-focus style.  The
  related shod.raiseButtons option specify which mouse buttons raise
  a window.

TODO.
• Handle window urgency and _NET_WM_STATE_DEMANDS_ATTENTION state.
• Add support to panels/bars.

See also.
For scripting, see also lsd[2] and lsc[3], which list useful information
about desktop and clients.  For a desktop menu and desktop prompt, check
out xmenu[4] and xprompt[5].  For other handful utilities for X11, check
out xutils[6].
[2] https://github.com/phillbush/lsd
[3] https://github.com/phillbush/lsc
[4] https://github.com/phillbush/xmenu
[5] https://github.com/phillbush/xprompt
[6] https://github.com/phillbush/xutils

Thanks.
Shod was written based on code and/or inspiration from the following
window managers.  I'd like to thank their authors for their work that
helped me writing Shod.
• berry:    https://berrywm.org
• dwm:      https://dwm.suckless.org
• i3:       https://i3wm.org
• katriawm: https://www.uninformativ.de/git/katriawm/file/README.html
• sowm:     https://github.com/dylanaraps/sowm
• wmii:     https://github.com/0intro/wmii
