# If RACK_DIR is not defined when calling the Makefile, default to subdur
RACK_DIR ?= ../Rack-SDK

include $(RACK_DIR)/arch.mk

# FLAGS will be passed to both the C and C++ compiler
FLAGS +=
CFLAGS +=
CXXFLAGS += 

# Careful about linking to shared libraries, since you can't assume much about the user's environment and library search path.
# Static libraries are fine, but they should be added to this plugin's build system.
LDFLAGS +=

FLAGS += -Isrc -Iinc/rnbo-export/common 

ET_DIR=extern/EraeTouchLite


ETL_DIR=${ET_DIR}/erae-api
RTMIDI_DIR=${ET_DIR}/external/rtmidi
MOODYCAM=${ET_DIR}/external/moodycamel

CXXFLAGS += -I ${ETL_DIR} 
CXXFLAGS += -I ${RTMIDI_DIR}  
CXXFLAGS += -I ${MOODYCAM}


# some examples if we use LibreMidi, see headeronly docs
ifdef ARCH_LIN
#CXXFLAGS += -std=c++17 -DLIBREMIDI_HEADER_ONLY=1
#CXXFLAGS += -std=c++17 -LIBREMIDI_ALSA=1
#CXXFLAGS += -std=c++17 -LIBREMIDI_JACK=1 
endif

ifdef ARCH_MAC
CXXFLAGS += -D__MACOSX_CORE__
# LDFLAGS += -framework CoreMIDI
#CXXFLAGS += -std=c++17 -DLIBREMIDI_HEADER_ONLY=1
#CXXFLAGS += -std=c++17 -DLIBREMIDI_COREAUDIO=1 
	ifdef ARCH_X64
	endif
	ifdef ARCH_ARM64
	endif
endif

ifdef ARCH_WIN
#CXXFLAGS += -std=c++17 -DLIBREMIDI_HEADER_ONLY=1
#CXXFLAGS += -std=c++17 -LIBREMIDI_WINUWP=1 
#CXXFLAGS += -std=c++17 -LIBREMIDI_WINMM=1 
#CXXFLAGS += -std=c++17 -LIBREMIDI_JACK=1 
endif

ETL_SRC = \
    ${ETL_DIR}/EraeApi.cpp\
    ${ETL_DIR}/MidiDevice.cpp\
    ${ETL_DIR}/RtMidiDevice.cpp\

RTMIDI_SRC = \
    ${RTMIDI_DIR}/RtMidi.cpp
	

#    ${ETL_DIR}/LibreMidiDevice.cpp

# Add .cpp files to the build
SOURCES += ${ETL_SRC}
SOURCES += ${RTMIDI_SRC}

SOURCES += \
src/plugin.cpp \
src/EraeTouch.cpp \


# Add files to the ZIP package when running `make dist`
# The compiled plugin and "plugin.json" are automatically added.
DISTRIBUTABLES += res
DISTRIBUTABLES += $(wildcard LICENSE*)
DISTRIBUTABLES += $(wildcard presets)

# Include the Rack plugin Makefile framework
include $(RACK_DIR)/plugin.mk

# required for RNBO
CXXFLAGS += -std=c++17

