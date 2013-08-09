.PHONY: all clean

all:

clean:
	find . -maxdepth 1 -type d | grep -v "^\.\?\.\?$$" | sed 's/.\///' | xargs -I 0 make -C 0 clean

