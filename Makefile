NAME := webserv
CC := g++
CFLAGS := -Wall -Wextra -Werror -Wunused-function -std=c++98 -MMD -MP
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

SRC := $(shell find $(SRC_PATH) -type f -name "*.cpp" | sed 's#^$(SRC_PATH)##')

SRCS := $(addprefix $(SRC_PATH), $(SRC))
OBJS := $(addprefix $(OBJ_PATH), $(SRC:.cpp=.o))
CPPCHECK := cppcheck
CPPCHECK_FLAGS := --enable=all --inconclusive --std=c++98 --force --inline-suppr -I ./include \
	--suppress=unusedFunction --suppress=unusedPrivateFunction --suppress=missingIncludeSystem
FORMAT := clang-format
FORMAT_FILES := $(shell find src include -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \))

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

cppcheck:
	$(CPPCHECK) $(CPPCHECK_FLAGS) $(SRCS)

format:
	$(FORMAT) -i $(FORMAT_FILES)

help:
	@echo "Targets:"
	@echo "  all       Build $(NAME)"
	@echo "  clean     Remove object files"
	@echo "  fclean    Remove object files and binary"
	@echo "  re        Rebuild everything"
	@echo "  cppcheck  Run static analysis with cppcheck"
	@echo "  format    Format source files with clang-format"
	@echo "  help      Show this help"

.PHONY: all clean fclean re cppcheck format help
