# ubercursor

This is a hack based fork of swcursor to replicate mouse overlays on linux.

This is a simple software cursor for X11 environments, it creates an
overlay window with an (large) image that follows the cursor
around. You will need to arrange for your normal cursor to be hidden,
or screen record using ffmpeg's kmsgrab which often does not capture
harware cursors at all.


## Installation and Usage

You will need to have working development libraries for gtk-3.0,
gdk and xlib.


```
$ make
$ ./ubercursor
```

ubercursor is to be used as wrapper for other processes (games for example)

```
Usage: ./ubercursor [-i file] [-r number] -- <command>

-i [file.png]
 load the png file as mouse cursor

-r [number]
set the refresh rate per seconds

Example:
./ubercursor -i cursors/cursor-large.png -r 144 -- <command>
```

based on https://github.com/neetsots/swcursor