NAME     = webserv
CXX      = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98 -Iinclude

SRC_DIR  = src
SRCS     = $(SRC_DIR)/main.cpp \
           $(SRC_DIR)/ConfigParser.cpp \
           $(SRC_DIR)/Socket.cpp \
           $(SRC_DIR)/Server.cpp \
           $(SRC_DIR)/Cgi.cpp \
           $(SRC_DIR)/Methods.cpp \
           $(SRC_DIR)/Http.cpp \
           $(SRC_DIR)/FileUtils.cpp

OBJS     = $(SRCS:.cpp=.o)
HEADERS  = include/Server.hpp include/Config.hpp include/Http.hpp include/FileUtils.hpp

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
