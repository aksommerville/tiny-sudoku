# commands.mk
# All special entry points.

ifneq (,$(strip $(EXE_NATIVE)))
  run:$(EXE_NATIVE) data;$(EXE_NATIVE)
else
  run:;echo "EXE_NATIVE unset" ; exit 1
endif

ifneq (,$(strip $(TINY_BIN_SOLO)))
  launch:$(TINY_BIN_SOLO); \
    $(TINY_PKGROOT)/arduino/tools/bossac/1.7.0-arduino3/bossac -i -d --port=$(TINY_PORT) -U true -i -e -w $(TINY_BIN_SOLO) -R
else
  launch:;echo "TINY_BIN_SOLO unset" ; exit 1
endif

edit-audio:$(EXE_TOOL_audioedit) $(EXE_TOOL_sounds) $(EXE_TOOL_wavecvt);$(EXE_TOOL_audioedit)

deploy-menu sdcard test install help:;echo "TODO: make $@" ; exit 1

clean:;rm -rf mid out
