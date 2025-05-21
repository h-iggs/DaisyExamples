C_SOURCES += ym3438.c
CFLAGS += $(INC)
CFLAGS += -g -O0
CPPFLAGS = -g -O0

# Project Name
TARGET = Nuked

# Sources
CPP_SOURCES = Nuked.cpp

# Library Locations
LIBDAISY_DIR = ../../libDaisy
DAISYSP_DIR = ../../DaisySP

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# Strip out -O2 and extra inlining but keep all other flags (debug, includes, dependency, etc.)
override CFLAGS   := $(filter-out -O2,$(CFLAGS))
override CXXFLAGS := $(filter-out -O2 -finline -finline-functions-called-once,$(CXXFLAGS))