# configure.mk
# Runs first, and establishes host and target platforms, etc.

#-------------------------------------------------------------
# Configuration for the Tiny build.
# This requires an installation of Arduino Studio (arduino.cc)
# You will very likely need to tweak these -- I have no idea what I'm doing.

ifeq (,) # greyskull
  TINY_PORT:=ttyACM0
  TINY_IDEROOT:=/opt/arduino-1.8.16
  TINY_IDEVERSION:=10816
  TINY_BUILDER_OPTS:=
  TINY_PKGROOT:=$(wildcard ~/.arduino15/packages)
  TINY_LIBSROOT:=$(wildcard ~/Arduino/libraries)
else # MacBook Air
  TINY_PORT:=tty.usbmodemFA131
  TINY_IDEROOT:=/Applications/Arduino.app/Contents/Java
  TINY_IDEVERSION:=10819
  TINY_BUILDER_OPTS:=
  TINY_PKGROOT:=$(wildcard ~/Library/Arduino15/packages)
  TINY_LIBSROOT:=$(wildcard ~/Documents/Arduino/libraries)
endif

TINY_BUILDER:=$(TINY_IDEROOT)/arduino-builder

#--------------------------------------------------------------
# Configuration for the Native build.
# Cross-compilers could go here, and also platform variants (eg linux-vulkan?)
# "none" is not an option; no matter what, we do need to be able to build some compile-time tools.
# Building for the Tiny is a different concern.

ifndef BC_CONFIG
  UNAMES:=$(shell uname -s)
  ifeq ($(UNAMES),Linux)
    # Distinguishing our three Linux variants by host name, probly not the best method.
    UNAMEN:=$(shell uname -n)
    ifeq ($(UNAMEN),atarivcs)
      BC_CONFIG:=linux-vcs
    else ifeq ($(NAMEN),raspberrypi)
      # Using VideoCore. Newer Pi models should probably use linux-default.
      BC_CONFIG:=linux-raspi
    else
      BC_CONFIG:=linux-default
    endif
  else ifeq ($(UNAMES),Darwin)
    BC_CONFIG:=macos-default
  else ifneq (,$(strip $(filter MINGW%,$(UNAMES))))
    BC_CONFIG:=mswin-default
  else
    $(warning Unable to guess default config from kernel name '$(UNAMES)', using 'generic')
    BC_CONFIG:=generic
  endif
endif

BC_PLATFORM:=$(firstword $(subst -, ,$(BC_CONFIG)))

MIDDIR:=mid/$(BC_CONFIG)
OUTDIR:=out/$(BC_CONFIG)

# Unset if you don't want to build the native app.
EXE_NATIVE:=$(OUTDIR)/sudoku

include etc/config/$(BC_CONFIG).mk

CC+=$(patsubst %,-DBC_USE_%=1,$(OPT_ENABLE)) -DBC_PLATFORM=BC_PLATFORM_$(BC_PLATFORM)
