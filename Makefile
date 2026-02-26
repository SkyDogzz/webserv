NAME := webserv
CC := g++
CFLAGS := -Wall -Wextra -Werror -Wunused-function -MMD -MP -Ofast
LDFLAGS :=
INCLUDES := -I ./include


ifdef DEBUG
	CFLAGS += -g3
	CFLAGS += -D DEBUG=true
endif

ifdef FSAN
	CFLAGS += -fsanitize=address
endif

SRC_PATH := src/

OBJ_PATH := obj/

SRC := $(shell find $(SRC_PATH) -type f -name "*.cpp" -printf "%P\n")

SRCS := $(addprefix $(SRC_PATH), $(SRC))
OBJS := $(addprefix $(OBJ_PATH), $(SRC:.cpp=.o))

all: $(NAME)

$(OBJ_PATH)%.o: $(SRC_PATH)%.cpp
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(NAME): $(OBJS) $(LUA_LIB)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

-include $(wildcard $(OBJS:.o=.d))

clean:
	rm -rf $(OBJ_PATH)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
