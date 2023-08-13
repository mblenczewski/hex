.PHONY: all build clean cleanall dist extra install uninstall

PREFIX			?= /usr/local

CC			?= cc
AR			?= ar
RANLIB			?= ranlib
TAR			?= tar

SRC			:= src
INC			:= include
OBJ			:= obj

WARN			:= -Wall -Wextra -Wpedantic -Werror

CFLAGS			:= -std=c11 $(WARN) -Og -g
CPPFLAGS		:= -I$(INC)
LDFLAGS			:=

HEX_SERVER_TARGET	:= hex-server

HEX_SERVER_SOURCES	:= $(SRC)/hex.c \
			   $(SRC)/server.c \
			   $(SRC)/board.c \
			   $(SRC)/proto.c \
			   $(SRC)/utils.c

HEX_SERVER_OBJECTS	:= $(HEX_SERVER_SOURCES:$(SRC)/%.c=$(OBJ)/%.o)
HEX_SERVER_OBJDEPS	:= $(HEX_SERVER_OBJECTS:%.o=%.d)
HEX_SERVER_FLAGS	:=

HEX_AGENT_SOURCES	:= $(wildcard agents/*)

ARCHIVE_TARGET		:= hex-server.tar

ARCHIVE_SOURCES		:= .editorconfig \
			   .gitignore \
			   Makefile \
			   README.md \
			   schedule.txt \
			   tournament-host.py \
			   agents/ \
			   include/ \
			   src/

HEX_AGENT_USERS		:= $(shell seq 1 16)

all: build extra

build: $(HEX_SERVER_TARGET)

clean:
	rm -rf $(HEX_SERVER_TARGET) $(OBJ)

cleanall: clean | $(HEX_AGENT_SOURCES)
	@for d in $(HEX_AGENT_SOURCES); do make -C $$d clean; done

dist: cleanall
	$(TAR) cf $(ARCHIVE_TARGET) $(ARCHIVE_SOURCES)

extra: | $(HEX_AGENT_SOURCES)
	@for d in $(HEX_AGENT_SOURCES); do make -C $$d; done

install: build
	for i in $(HEX_AGENT_USERS); do \
		if [ ! $$(id hex-agent-$$i -u 2>/dev/null) ]; then \
			useradd -M -N -e '' hex-agent-$$i; \
		fi; \
	done
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	install -m 0755 $(HEX_SERVER_TARGET) $(DESTDIR)$(PREFIX)/bin/$(HEX_SERVER_TARGET)

uninstall:
	for i in $(HEX_AGENT_USERS); do \
		if [ $$(id hex-agent-$$i -u 2>/dev/null) ]; then \
			userdel hex-agent-$$i; \
		fi; \
	done
	rm -f $(DESTDIR)$(PREFIX)/bin/$(HEX_SERVER_TARGET)

$(HEX_SERVER_TARGET): $(HEX_SERVER_OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS) $(HEX_SERVER_FLAGS)

-include $(HEX_SERVER_OBJDEPS)

$(OBJ)/%.o: $(SRC)/%.c | $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) -MMD -o $@ -c $< $(CFLAGS) $(CPPFLAGS)

$(OBJ):
	mkdir $@
