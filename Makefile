

CC_FLAG = -I./include
OTHER_FLAGS = -v -g -Wall

LINK_TARGET = build/saas

SRC_FILES = main.c debug.c chunk.c value.c vm.c compiler.c scanner.c object.c memory.c hashmap.c

TARGET_OBJS = $(SRC_FILES:%.c=build/%.o)

vpath %.c src src/bytecode src/vm src/compiler src/datastructures

vpath %.h include include/bytecode include/vm include/compiler include/datastructures

build:
	mkdir -p build
all: $(LINK_TARGET) 
	echo All done


$(LINK_TARGET): $(TARGET_OBJS) 
	gcc -o $@ $^ $(CC_FLAG) $(OTHER_FLAGS) -g
#codesign -s - -f --entitlements build/segv.entitlements build/main 
build/segv.entitlements:
        /usr/libexec/PlistBuddy -c "Add :com.apple.security.get-task-allow bool true" $@ 

build/%.o: %.c
	gcc -o $@ -c $< $(CC_FLAG) $(OTHER_FLAGS)
# Automatic variables are set by make after a rule is matched. There include:
# $@: the target filename.
# $*: the target filename without the file extension.
# $<: the first prerequisite filename.
# $^: the filenames of all the prerequisites, separated by spaces, discard duplicates.
# $+: similar to $^, but includes duplicates.
# $?: the names of all prerequisites that are newer than the target, separated by spaces.
clean:
	rm -rf build/*
	echo cleaning done

main.c: common.h chunk.h debug.h vm.h
debug.c: debug.h
chunk.c: chunk.h memory.h
value.c: value.h memory.h
vm.c: common.h vm.h
compiler.c: compiler.h common.h scanner.h
scanner.c: common.h scanner.h
object.c: object.h vm.h value.h memory.h
memory.c: object.h vm.h memory.h
hashmap.c: object.h value.h memory.h hashmap.h


#daily
