.PHONY: all build clean deps

JAVAC		?= javac
JAR		?= jar

SRC		:= src
OBJ		:= obj

JLINT		:= -deprecation -Xlint:unchecked
JCFLAGS		:= -d $(OBJ) -cp $(SRC) -g $(JLINT)

TARGET		:= agent.jar
MANIFEST	:= MANIFEST.txt
SOURCES		:= $(SRC)/Agent.java
OBJECTS		:= $(SOURCES:$(SRC)/%.java=$(OBJ)/%.class)

all: build

build: $(TARGET)

clean:
	rm -rf $(TARGET) $(OBJ)

deps: $(MANIFEST)

$(TARGET): deps $(OBJECTS)
	$(JAR) -cfmv $@ $(MANIFEST) $(wordlist 2,$(words $^),$^)

$(OBJ)/%.class: $(SRC)/%.java
	@mkdir -p $(dir $@)
	$(JAVAC) $(JCFLAGS) $<

$(OBJ):
	mkdir -p $@

