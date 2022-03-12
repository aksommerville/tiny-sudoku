# build.mk
# Discover input files, determine what needs built, declare rules to do so.

# Declare all generated files (those which are not triggered by a source file).
GENERATED_FILES:=$(addprefix $(MIDDIR)/, \
  bc_config.h \
  test/int/bc_itest_toc.h \
)

# Digest selected optional units.
OPT_AVAILABLE:=$(notdir $(wildcard src/opt/*))
OPT_IGNORE:=$(filter-out $(OPT_ENABLE),$(OPT_AVAILABLE))
OPT_ERROR:=$(filter-out $(OPT_AVAILABLE),$(OPT_ENABLE))
ifneq (,$(strip $(OPT_ERROR)))
  $(warning Optional units requested by $(BC_CONFIG) but not found: $(OPT_ERROR))
endif
OPT_IGNORE_PATTERN:=$(foreach U,$(OPT_IGNORE), \
  src/opt/$U/% \
  $(MIDDIR)/opt/$U/% \
  src/test/unit/$U/% \
  src/test/int/$U/% \
)

# Discover source files and drop into a few buckets.
SRCFILES:=$(filter-out $(OPT_IGNORE_PATTERN),$(shell find src -type f) $(GENERATED_FILES))
CFILES:=$(filter %.c %.m %.s %.cpp,$(SRCFILES))
DATAFILES_EMBED:=$(filter src/data/embed/%,$(SRCFILES))
DATAFILES_INCLUDE:=$(filter src/data/include/%,$(SRCFILES))

# We need to declare the tools before embedded data, and embedded data before OFILES, so it's a little spaghettish...
TOOLS:=$(filter-out common,$(notdir $(wildcard src/tool/*)))
$(foreach U,$(TOOLS),$(eval EXE_TOOL_$U:=out/tool/$U))

# "Embedded" data files turn into C files and get baked in to the executable.
define EMBED_RULES # 1=middir 2=platform
  EMBED_CFILES_$1:=$(patsubst src/data/embed/%,$1/data/embed/%.c,$(DATAFILES_EMBED))
  CFILES+=$$(EMBED_CFILES_$1)
  $1/data/embed/%.png.c:src/data/embed/%.png $(EXE_TOOL_imgcvt);$$(PRECMD) $(EXE_TOOL_imgcvt) -o$$@ $$< --platform=$2
  $1/data/embed/%.sound.c:src/data/embed/%.sound $(EXE_TOOL_sounds);$$(PRECMD) $(EXE_TOOL_sounds) -o$$@ $$< --platform=$2
  $1/data/embed/%.map.c:src/data/embed/%.map $(EXE_TOOL_map);$$(PRECMD) $(EXE_TOOL_map) -o$$@ $$< --platform=$2
  $1/data/embed/%.mid.c:src/data/embed/%.mid $(EXE_TOOL_songcvt);$$(PRECMD) $(EXE_TOOL_songcvt) -o$$@ $$< --platform=$2
  $1/data/embed/%.wave.c:src/data/embed/%.wave $(EXE_TOOL_wavecvt);$$(PRECMD) $(EXE_TOOL_wavecvt) -o$$@ $$< --platform=$2
  $1/data/embed/%.c:src/data/embed/% $(EXE_TOOL_rawcvt);$$(PRECMD) $(EXE_TOOL_rawcvt) -o$$@ $$< --platform=$2
  .precious:$$(EMBED_CFILES_$1)
endef
$(eval $(call EMBED_RULES,$(MIDDIR),$(BC_PLATFORM)))
$(eval $(call EMBED_RULES,mid/tiny,tiny))

# C, Objective-C, and Assembly files all become Object files.
# Bucket these mostly by the product that needs them.
OFILES_ALL:=$(addsuffix .o,$(patsubst src/%,$(MIDDIR)/%,$(basename $(CFILES))))
OFILES_MAIN:=$(filter $(MIDDIR)/main/%,$(OFILES_ALL)) $(filter $(MIDDIR)/data/%,$(OFILES_ALL))
OFILES_COMMON:=$(filter $(MIDDIR)/common/%,$(OFILES_ALL)) $(filter $(MIDDIR)/opt/%,$(OFILES_ALL))
OFILES_ITEST:=$(filter $(MIDDIR)/test/int/%,$(OFILES_ALL))
OFILES_UTEST:=$(filter $(MIDDIR)/test/unit/%,$(OFILES_ALL))
OFILES_CTEST:=$(filter $(MIDDIR)/test/common/%,$(OFILES_ALL))
OFILES_TOOLS:=$(filter $(MIDDIR)/tool/%,$(OFILES_ALL))
OFILES_TOOLS_COMMON:=$(filter $(MIDDIR)/tool/common/% $(MIDDIR)/common/%,$(OFILES_ALL))

-include $(OFILES_ALL:.o=.d)

$(MIDDIR)/%.o:src/%.c;$(PRECMD) $(CC) -o $@ $<
$(MIDDIR)/%.o:src/%.m;$(PRECMD) $(CC) -xobjective-c -o $@ $<
$(MIDDIR)/%.o:src/%.s;$(PRECMD) $(CC) -xassembler-with-cpp -o $@ $<
$(MIDDIR)/%.o:src/%.cpp;$(PRECMD) $(CXX) -o $@ $<
$(MIDDIR)/%.o:$(MIDDIR)/%.c;$(PRECMD) $(CC) -o $@ $<
$(MIDDIR)/%.o:$(MIDDIR)/%.m;$(PRECMD) $(CC) -xobjective-c -o $@ $<
$(MIDDIR)/%.o:$(MIDDIR)/%.s;$(PRECMD) $(CC) -xassembler-with-cpp -o $@ $<
$(MIDDIR)/%.o:$(MIDDIR)/%.cpp;$(PRECMD) $(CXX) -o $@ $<

# Each directory under src/tool/ becomes an executable, except 'common'.
define TOOL_RULES # 1=tool
  OFILES_TOOL_$1:=$$(filter $(MIDDIR)/tool/$1/%,$(OFILES_TOOLS)) $(OFILES_TOOLS_COMMON)
  all:$$(EXE_TOOL_$1)
  $$(EXE_TOOL_$1):$$(OFILES_TOOL_$1);$$(PRECMD) $(LD) -o$$@ $$^ $(LDPOST)
endef
define TOOL_OPT # 1=tool 2=optunits
  $$(EXE_TOOL_$1):$(filter $(addprefix $(MIDDIR)/opt/,$(addsuffix /%,$2)),$(OFILES_ALL))
endef
$(foreach U,$(TOOLS),$(eval $(call TOOL_RULES,$U)))
$(eval $(call TOOL_OPT,audioedit,pulse inotify alsamidi))

# Native game executable is optional, maybe you're only building for Tiny.
ifneq (,$(strip $(EXE_NATIVE)))
  all:$(EXE_NATIVE)
  $(EXE_NATIVE):$(OFILES_MAIN) $(OFILES_COMMON);$(PRECMD) $(LD) -o $@ $(OFILES_MAIN) $(OFILES_COMMON) $(LDPOST)
endif

# "Included" data files mostly just copy, but may also go through a preprocess step.
# There are two outputs: One for Native and one for Tiny. They might preprocess differently.
define DATA_INCLUDE_RULES # config
  DATA_INCLUDE_$1:=$(patsubst src/data/include/%,out/$1/data/%, \
    $(patsubst %.png,%.tsv, \
    $(DATAFILES_INCLUDE) \
  ))
  data:$$(DATA_INCLUDE_$1)
  out/$1/data/%.tsv:src/data/include/%.png $(EXE_TOOL_imgcvt);$$(PRECMD) $(EXE_TOOL_imgcvt) -o$$@ $$<
  out/$1/data/%:src/data/include/%;$$(PRECMD) cp $$< $$@
endef
all:data
$(eval $(call DATA_INCLUDE_RULES,$(BC_CONFIG)))
$(eval $(call DATA_INCLUDE_RULES,tiny))

#TODO integration tests
#TODO unit tests
