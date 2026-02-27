# Project Name
TARGET = EmuOsc
APP_TYPE = BOOT_QSPI

# Sources
CPP_SOURCES = \
	EmuOsc.cpp \
	third_party/reSID/src/envelope.cpp \
	third_party/reSID/src/extfilt.cpp \
	third_party/reSID/src/filter.cpp \
	third_party/reSID/src/pot.cpp \
	third_party/reSID/src/sid.cpp \
	third_party/reSID/src/version.cpp \
	third_party/reSID/src/voice.cpp \
	third_party/reSID/src/wave.cpp

# Includes
C_INCLUDES += -Ithird_party/reSID/src

# Library Locations
LIBDAISY_DIR = ../../libDaisy
DAISYSP_DIR = ../../DaisySP

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
