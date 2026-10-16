user: src/user.c
	gcc -Wall -g -o user src/user.c

clean:
	rm -f user

.PHONY: clean