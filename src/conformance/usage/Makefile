# Copyright 2013-2024, The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0

SHELL = /usr/bin/env bash

QUIET      ?= @
VERYQUIET  ?= @
PYTHON	   ?= python3
ifneq (,$(strip $(BUNDLER)))
ASCIIDOC   ?= bundle exec asciidoctor --trace
HEXAPDF    ?= bundle exec hexapdf
export BUNDLE_GEMFILE = $(SPECROOT)/Gemfile
else
ASCIIDOC   ?= asciidoctor
HEXAPDF    ?= hexapdf
endif
RM          = rm -f
RMRF        = rm -rf
MKDIR       = mkdir -p
CP          = cp -p
MV          = mv
ECHO        = @echo

SPECROOT   := $(CURDIR)/../../../specification

# Target directory for all generated files.
# This can be overridden.
GENDIR   := $(CURDIR)/generated

# Target directory for output files.
OUTDIR   ?= $(GENDIR)/out

SCRIPTS  := $(SPECROOT)/scripts


MAKE_RELATIVE = $(patsubst $(CURDIR)/%,%,$(1))

all: html pdf
.PHONY: all

################################################
# AsciiDoctor targets

# Gets extended incrementally
ASCIIDOCTOR_TARGETS :=


CTSUSAGE := $(OUTDIR)/cts_usage.html
CTSUSAGEPDF := $(OUTDIR)/cts_usage.pdf

USAGESRC = cts_usage.adoc
USAGEFILES = $(filter-out $(USAGESRC),$(wildcard [A-Za-z_]*.adoc)) \
             $(SPECROOT)/config/$(CSS_FILENAME) \
             docinfo.html

# Usage doc as HTML
$(CTSUSAGE): $(USAGESRC) $(USAGEFILES)
ASCIIDOCTOR_TARGETS += $(CTSUSAGE)

html: $(CTSUSAGE)
.PHONY: html

# Usage doc as PDF
$(CTSUSAGEPDF): $(USAGESRC) $(USAGEFILES)
$(CTSUSAGEPDF): BACKEND_ARGS=$(BACKEND_ARGS_PDF)
ASCIIDOCTOR_TARGETS += $(CTSUSAGEPDF)

pdf: $(CTSUSAGEPDF)
.PHONY: pdf

include asciidoctor-targets.mk

################################################
# Utility targets

clean:
	$(RM) $(CTSUSAGE) $(CTSUSAGEPDF)
.PHONY: clean

reflow:
	$(QUIET)$(PYTHON) $(SPECROOT)/scripts/reflow.py -ov $(wildcard [A-Za-z_]*.adoc)
.PHONY: reflow


################################################
# Meta builds for releases. Also copies into approximately the right layout for the OpenXR-Registry repo.
REGISTRYOUTDIR = $(GENDIR)/out/registry-release/cts-usage

$(REGISTRYOUTDIR)/cts_usage.pdf: $(CTSUSAGEPDF)
	$(ECHO) "[hexapdf]     $(call MAKE_RELATIVE,$@)"
	$(QUIET)$(MKDIR) "$(@D)"
	$(QUIET)$(HEXAPDF) optimize --force "$<" "$@"

$(REGISTRYOUTDIR)/cts_usage.html: $(CTSUSAGE)
	$(ECHO) "[cp]          $(call MAKE_RELATIVE,$@)"
	$(QUIET)$(MKDIR) "$(@D)"
	$(QUIET)$(CP) "$<" "$@"

release: $(REGISTRYOUTDIR)/cts_usage.pdf $(REGISTRYOUTDIR)/cts_usage.html
.PHONY: release
