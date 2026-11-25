#!/usr/bin/make -f
# Tinplate Reverb (DPF) - see ../build.sh
# Copyright (C) 2026 Tinplate authors - SPDX-License-Identifier: GPL-3.0-or-later

NAME = tinplate_reverb

FILES_DSP = TinplatePlugin.cpp
FILES_UI  = TinplateUI.cpp

DPF_PATH       = ../dpf
DPF_TARGET_DIR ?= ../bin
DPF_BUILD_DIR  = ../build

include ../dpf/Makefile.plugins.mk

TARGETS += vst3 clap
ifneq ($(MACOS_OR_WINDOWS),true)
TARGETS += lv2_sep
endif

all: $(TARGETS)
