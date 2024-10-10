# Copyright 2013-2024, The Khronos Group Inc.
#
# SPDX-License-Identifier: Apache-2.0


ATTRIBOPTS   = -a revnumber="$(SPECREVISION)" \
               -a revremark="$(SPECREMARK)" \
               -a config=$(SPECROOT)/config \
               -a pdf-page-size=$(PAGESIZE) \
               -a pdf-stylesdir=$(SPECROOT)/config \
               -a pdf-style=pdf \

ADOCOPTS = --doctype book \
           $(ADOC_FAILURE_LEVEL) \
           $(ATTRIBOPTS)

SPECREVISION = 1.1.41

ifneq (,$(strip $(RELEASE)))
# No dates or internal commit hashes in release builds for reproducibility
ATTRIBOPTS   += -a revdate!
ATTRIBOPTS   += -a last-update-label!
ATTRIBOPTS   += -a reproducible
SPECREMARK   ?= $(GITREMARK)

else
ATTRIBOPTS   += -a revdate="$(SPECDATE)"
SPECREMARK   ?= $(GITREMARK) \
		commit: $(shell echo `git log -1 --format="%H"`)

endif

CSS_FILENAME := khronos.css

# Default to html5
BACKEND_ARGS := --backend html5 \
                -a stylesdir=$(SPECROOT)/config \
                -a stylesheet=$(CSS_FILENAME)

# PDF defaults to letter
PAGESIZE := LETTER

BACKEND_ARGS_PDF := --backend pdf \
                    --require asciidoctor-pdf \
                    -a compress

ifneq (,$(strip $(VERY_STRICT)))
ADOC_FAILURE_LEVEL := --failure-level INFO --verbose
else
ADOC_FAILURE_LEVEL := --failure-level ERROR
endif

# AsciiDoctor rule - customized by the places where these are described
$(ASCIIDOCTOR_TARGETS):
	$(ECHO) "[asciidoctor] $< -> $(call MAKE_RELATIVE,$@)"
	$(QUIET)$(MKDIR) "$(@D)"
	$(QUIET)$(ASCIIDOC) $(ADOCOPTS) $(BACKEND_ARGS) --out-file "$@" "$<"
	$(POSTPROCESS)
