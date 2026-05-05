ubercursor: ubercursor.c cursor_image.h ubercursor-window.h ubercursor-window.c
	gcc `pkg-config --cflags x11 xext gtk+-3.0` -o ubercursor ubercursor.c ubercursor-window.c `pkg-config --libs x11 xext gtk+-3.0`

clean:
	rm -f ubercursor

.PHONY: clean
