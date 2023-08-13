.PHONY: all build clean

CC		?= cc
TAR		?= tar

SRC		:= src
INC		:= include
DEPINC		:= ../../include
OBJ		:= obj

WARN		:= -Wall -Wextra -Wpedantic -Werror

CFLAGS		:= -std=c17 $(WARN) -Og -g -flto
CPPFLAGS	:= -I$(INC) -I$(DEPINC)
LDFLAGS		:= -lm -flto

TARGET		:= hexes
SOURCES		:= $(SRC)/hexes.c \
		   $(SRC)/agent.c \
		   $(SRC)/agent/mcts.c \
		   $(SRC)/agent/random.c \
		   $(SRC)/board.c \
		   $(SRC)/log.c \
		   $(SRC)/network.c \
		   $(SRC)/threadpool.c \
		   $(SRC)/utils.c

OBJECTS		:= $(SOURCES:$(SRC)/%.c=$(OBJ)/%.o)
OBJDEPS		:= $(OBJECTS:%.o=%.d)

all: build

build: $(TARGET)

clean:
	rm -rf $(TARGET) $(OBJ)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJ)/%.o: $(SRC)/%.c | $(OBJ)
	@mkdir -p $(dir $@)
	$(CC) -MMD -o $@ -c $< $(CFLAGS) $(CPPFLAGS)

-include $(OBJDEPS)

$(OBJ):
	mkdir -p $@
