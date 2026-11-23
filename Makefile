# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright (C) 2026 VGM Player for GameShell contributors
#
# VGM Player for GameShell (armv7l / Debian 9)
# Builds libvgm (player + emu + utils) from source and links an SDL front-end.

CXX      = g++
CC       = gcc
ROOT     ?= /tmp/libvgm
BUILD    = build
TARGET   = vgm_player

# -- libvgm feature selection: build every sound core -----------------------
DEFS = -DHAVE_STDINT_H -DVGM_LITTLE_ENDIAN -DSNDDEV_SELECT \
 -DSNDDEV_SN76496 -DEC_SN76496_MAME -DEC_SN76496_MAXIM \
 -DSNDDEV_YM2413 -DEC_YM2413_MAME -DEC_YM2413_EMU2413 -DEC_YM2413_NUKED \
 -DSNDDEV_YM2612 -DEC_YM2612_GPGX -DEC_YM2612_GENS -DEC_YM2612_NUKED \
 -DSNDDEV_YM2151 -DEC_YM2151_MAME -DEC_YM2151_NUKED \
 -DSNDDEV_SEGAPCM \
 -DSNDDEV_RF5C68 -DEC_RF5C68_MAME -DEC_RF5C68_GENS \
 -DSNDDEV_YM2203 -DSNDDEV_YM2608 -DSNDDEV_YM2610 \
 -DSNDDEV_YM3812 -DEC_YM3812_MAME -DEC_YM3812_ADLIBEMU -DEC_YM3812_NUKED \
 -DSNDDEV_YM3526 -DSNDDEV_Y8950 \
 -DSNDDEV_YMF262 -DEC_YMF262_MAME -DEC_YMF262_ADLIBEMU -DEC_YMF262_NUKED \
 -DSNDDEV_YMF278B -DSNDDEV_YMZ280B -DSNDDEV_YMF271 \
 -DSNDDEV_AY8910 -DEC_AY8910_MAME -DEC_AY8910_EMU2149 \
 -DSNDDEV_32X_PWM \
 -DSNDDEV_GAMEBOY -DEC_GB_MAME -DEC_GB_SAMEBOY \
 -DSNDDEV_NES_APU -DEC_NES_MAME -DEC_NES_NSFPLAY -DEC_NES_NSFP_FDS \
 -DSNDDEV_YMW258 -DSNDDEV_UPD7759 -DSNDDEV_MSM6258 -DSNDDEV_MSM6295 \
 -DSNDDEV_K051649 -DSNDDEV_K054539 \
 -DSNDDEV_C6280 -DEC_C6280_MAME -DEC_C6280_OOTAKE \
 -DSNDDEV_C140 -DSNDDEV_C219 -DSNDDEV_K053260 -DSNDDEV_POKEY \
 -DSNDDEV_QSOUND -DEC_QSOUND_MAME -DEC_QSOUND_CTR \
 -DSNDDEV_SCSP -DSNDDEV_WSWAN -DSNDDEV_VBOY_VSU \
 -DSNDDEV_SAA1099 -DEC_SAA1099_MAME -DEC_SAA1099_VB \
 -DSNDDEV_ES5503 -DSNDDEV_ES5506 -DSNDDEV_X1_010 -DSNDDEV_C352 \
 -DSNDDEV_GA20 -DSNDDEV_MIKEY -DSNDDEV_K007232 -DSNDDEV_K005289 \
 -DSNDDEV_MSM5205 -DSNDDEV_MSM5232 -DSNDDEV_BSMT2000 -DSNDDEV_ICS2115

INC   = -I$(ROOT) -I$(ROOT)/libs/include
WARN  = -Wall -Wno-unused-parameter -Wno-unused-variable -Wno-unused-but-set-variable \
        -Wno-sign-compare -Wno-unused-function -Wno-unknown-pragmas
CFLAGS   = -O2 -pthread $(DEFS) $(INC) $(WARN)
CXXFLAGS = -O2 -pthread -std=gnu++98 $(DEFS) $(INC) $(WARN)
LDLIBS   = -lSDL -lSDL_ttf -lz -lm -lpthread

EMU_C = $(filter-out $(ROOT)/emu/cores/adlibemu_opl_inc.c $(ROOT)/emu/cores/scsplfo.c,$(wildcard $(ROOT)/emu/*.c) $(wildcard $(ROOT)/emu/cores/*.c))
UTIL_C = $(ROOT)/utils/DataLoader.c $(ROOT)/utils/FileLoader.c $(ROOT)/utils/MemoryLoader.c \
         $(ROOT)/utils/OSMutex_POSIX.c $(ROOT)/utils/OSSignal_POSIX.c $(ROOT)/utils/OSThread_POSIX.c \
         $(ROOT)/utils/StrUtils-CPConv_IConv.c
PLAYER_C   = $(ROOT)/player/dblk_compr.c $(ROOT)/player/helper.c
PLAYER_CPP = $(wildcard $(ROOT)/player/*.cpp)

ALL_C   = $(EMU_C) $(UTIL_C) $(PLAYER_C)
ALL_CPP = $(PLAYER_CPP) vgm_player.cpp
OBJS    = $(patsubst $(ROOT)/%.c,$(BUILD)/%.o,$(ALL_C)) \
          $(patsubst $(ROOT)/%.cpp,$(BUILD)/%.o,$(PLAYER_CPP)) \
          $(BUILD)/vgm_player.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@ $(LDLIBS)
	@echo "Built $@"

$(BUILD)/%.o: $(ROOT)/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -std=gnu99 -c $< -o $@

$(BUILD)/%.o: $(ROOT)/%.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD) $(TARGET)
