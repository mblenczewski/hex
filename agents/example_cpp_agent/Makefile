.PHONY: all build clean

CCX		?= c++

SRC		:= src
INC		:= include
DEPINC		:= ../../include
OBJ		:= obj

WARN		:= -Wall -Wextra -Wpedantic -Werror

CFLAGS		:= -std=c++14 $(WARN) -Og -g
CPPFLAGS	:= -I$(INC) -I$(DEPINC)
LDFLAGS		:=

TARGET		:= agent
SOURCES		:= $(SRC)/agent.cpp
OBJECTS		:= $(SOURCES:$(SRC)/%.cpp=$(OBJ)/%.o)
OBJDEPS		:= $(OBJECTS:%.o=%.d)

all: build

build: $(TARGET)

clean:
	rm -rf $(TARGET) $(OBJ)

$(TARGET): $(OBJECTS)
	$(CCX) -o $@ $^ $(LDFLAGS)

-include $(OBJDEPS)

$(OBJ)/%.o: $(SRC)/%.cpp | $(OBJ)
	@mkdir -p $(dir $@)
	$(CCX) -MMD -o $@ -c $< $(CFLAGS) $(CPPFLAGS)

$(OBJ):
	mkdir -p $@
