.PHONY: all build clean

CC		?= cc

SRC		:= src
INC		:= include
DEPINC		:= ../../include
OBJ		:= obj

WARN		:= -Wall -Wextra -Wpedantic -Werror

CFLAGS		:= -std=c11 $(WARN) -Og -g
CPPFLAGS	:= -I$(INC) -I$(DEPINC)
LDFLAGS		:=

TARGET		:= agent
SOURCES		:= $(SRC)/agent.c
OBJECTS		:= $(SOURCES:$(SRC)/%.c=$(OBJ)/%.o)
OBJDEPS		:= $(OBJECTS:%.o=%.d)

all: build

build: $(TARGET)

clean:
	rm -rf $(TARGET) $(OBJ)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

-include $(OBJDEPS)

$(OBJ)/%.o: $(SRC)/%.c | $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) -MMD -o $@ -c $< $(CFLAGS) $(CPPFLAGS)

$(OBJ):
	mkdir -p $@
