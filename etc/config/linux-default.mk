# linux-default.mk

OPT_ENABLE:=pulse x11 evdev linux inotify alsamidi

CCWARN:=-Werror -Wimplicit
CC:=gcc -c -MMD -O2 -Isrc -Isrc/common -I$(MIDDIR) $(CCWARN)
LD:=gcc
LDPOST:=-lpulse-simple -lX11 -lpthread -lm -lz -lasound
