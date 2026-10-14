CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I. -Icore -Iutils -Ientities -Isystems -Idata -Iscenes

SRCS = main.c \
       core/game.c core/transition.c \
       utils/text.c utils/effects.c \
       entities/player.c entities/luci.c entities/monitor.c entities/projectile.c \
       systems/collision.c systems/dialogue.c systems/quiz.c systems/battle.c \
       data/dialogues.c data/quizzes.c \
       scenes/menu.c scenes/intro.c scenes/sala.c scenes/corredor.c

OBJS = $(SRCS:.c=.o)

ifeq ($(OS),Windows_NT)
    TARGET = utopia.exe
    LDFLAGS = -lraylib -lopengl32 -lgdi32 -lwinmm
    CLEAN_OBJS = $(subst /,\,$(OBJS))
    RM = del /Q /F
else
    TARGET = utopia
    LDFLAGS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
    CLEAN_OBJS = $(OBJS)
    RM = rm -f
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(CLEAN_OBJS) $(TARGET)

.PHONY: all clean
