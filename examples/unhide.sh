#!/bin/sh

lsw() {
	xprop -notype -f "_NET_CLIENT_LIST" 0x ' $0+\n' -root "_NET_CLIENT_LIST" |\
	cut -d' ' -f2- |\
	sed 's/, */\
/g'
}

ishidden() {
	xprop -notype -f "_NET_WM_STATE" 32a ' $0+\n' -id "$1" "_NET_WM_STATE" |\
	cut -d' ' -f2- |\
	sed 's/, */\
/g' | grep -q "_NET_WM_STATE_HIDDEN"
}

printname() {
	name="$(xprop -notype -f "_NET_WM_NAME" 8s ' $0+\n' -id "$1" "_NET_WM_NAME" 2>/dev/null)"
	[ "$(echo $name)" = "_NET_WM_NAME: not found." ] && name="$(xprop -notype -f "WM_NAME" 8s ' $0+\n' -id "$1" "WM_NAME" 2>/dev/null)"

	echo $name |\
	cut -d' ' -f2- |\
	sed 's/, */\
/g'
}

for win in $(lsw)
do
	ishidden $win && printf "%s: " $win && printname $win
done |\
dmenu -i -l 8 -p "unhide window:" |\
cut -d: -f1 |\
xargs wmctrl -b toggle,hidden -ir
