# Linux / GEC6818: make CROSS_COMPILE=arm-linux- (or arm-linux-gnueabi-)
CROSS_COMPILE ?= arm-linux-
CC = $(CROSS_COMPILE)gcc
CPPFLAGS = -Iinc -Iinc/system
CFLAGS ?= -O2 -std=c99 -Wall -Wextra -Wpedantic -Werror -Wvla -Wformat=2
LDFLAGS ?=
LDLIBS = -lm
APP_SRC = src/main.c src/System.c src/MainScreenSystem.c src/AlbumSystem.c src/GameSystem.c src/GamesMenuSystem.c src/GomokuSystem.c src/ToeSystem.c src/ChessSystem.c src/boardgames.c src/game.c src/WhiteboardSystem.c src/LoginSystem.c src/system/lcd.c src/system/file_manager.c src/system/print.c src/system/touch_cal.c src/system/calibrate.c src/system/platform.c
APP_OBJ = $(patsubst src/%.c,build/obj/%.o,$(APP_SRC))
FONT_OBJ = build/obj/system/truetype.o
.PHONY: all clean
all: build/terminal
build/terminal: $(APP_OBJ) $(FONT_OBJ)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
build/obj/system/truetype.o: src/system/truetype.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -std=c99 -O2 -MMD -MP -c $< -o $@
build/obj/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@
-include $(APP_OBJ:.o=.d) $(FONT_OBJ:.o=.d)
clean:
	rm -rf build/obj build/terminal
