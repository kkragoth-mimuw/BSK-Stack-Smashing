#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <sys/wait.h>

/*

  gcc -m32 -pie -fpie calc.c -o calc-easy
  gcc -m32 -pie -fpie calc.c -D HARD -o calc-hard

  Usage:

  $ ./calc-easy
  2+2
  4
  10+10*10+10
  120
  2++2
  WTF?
  10*(10+10)
  200
  ^D
  $

  Goal: connect to a calculator running on a remote server (via socat)
  and read flag.txt in its current working directory.

 */

enum {
	TOKEN_START,	/* A dummy placeholder at start of input.  */
	TOKEN_NUM,	/* A number.  */
	TOKEN_LP,	/* ( */
	TOKEN_PLUS,	/* + */
	TOKEN_TIMES,	/* * */
};

FILE *in;
FILE *out;

void doit() {
	int token[32] = {0};
	int value[32] = {0};
	int sp = 0;
	int i = 0;
	token[sp] = TOKEN_START;
	while (1) {
		for (i = 0; i < 64; i++) {
			printf("token[%d]: %d\n", i, token[i]);
		}
		for (i = 0; i < 64; i++) {
			printf("value[%d]: %d\n", i, value[i]);
		}
		printf("sp: %d\n", sp);
		
		int c = getc(in);
		/* Skip spaces.  */
		printf("%c", c);
		if (c == ' ')
			continue;
		/* If the user went away, exit.  */
		if (c == EOF) {
			exit(0);
		}
		/* If it's a left paren, just push it.  */
		if (c == '(') {
			token[++sp] = TOKEN_LP;
			printf("%d\n", sp);
			continue;
		}
		/* If a digit has been read, and we're already parsing
		   a number, shift it by one digit and add the new one.
		   Otherwise, push a new number onto the stack.  */
		if (isdigit(c)) {
			if (token[sp] != TOKEN_NUM) {
				token[++sp] = TOKEN_NUM;
				value[sp] = 0;
			}
			value[sp] *= 10;
			value[sp] += c - '0';
			continue;
		}
		/* If a multiplication sign is found, just push it.  */
		if (c == '*') {
			token[++sp] = TOKEN_TIMES;
			continue;
		}
		/* If an addition sign (or stronger) is found, perform all
		   pending multiplications.  */
		while (token[sp] == TOKEN_NUM && token[sp-1] == TOKEN_TIMES) {
			value[sp-2] *= value[sp];
			sp -= 2;
		}
		/* If an addition sign is found, push it.  */
		if (c == '+') {
			token[++sp] = TOKEN_PLUS;
			continue;
		}
		/* Something stronger than addition sign -- perform all
		   pending additions.  */
		while (token[sp] == TOKEN_NUM && token[sp-1] == TOKEN_PLUS) {
			value[sp-2] += value[sp];
			sp -= 2;
		}
		/* Right paren -- make sure we had a left paren before, then
		   move the number inside parens down one slot.  */
		if (c == ')') {
			if (token[sp-1] != TOKEN_LP) {
				goto err;
			}
			value[sp-1] = value[sp];
			token[--sp] = TOKEN_NUM;
			continue;
		}
		/* Newline means we're done. */
		if (c == '\n') {
			if (token[sp-1] != TOKEN_START)
				goto err;
			fprintf(out, "%d\n", value[sp]);
			return;
		}
		/* Unrecognized character.  */
		goto err;
	}
err:
	fprintf(out, "WTF?\n");
#ifdef HARD
	exit(1);
#endif
}

int main() {
#ifdef HARD
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		return 1;
	}
	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_port = htons(31337);
	sin.sin_addr.s_addr = INADDR_ANY;
	int res = bind(fd, (void *)&sin, sizeof sin);
	if (res < 0) {
		perror("bind");
		return 1;
	}
	res = listen(fd, 1337);
	if (res < 0) {
		perror("listen");
		return 1;
	}
	while (1) {
		int fd2 = accept(fd, 0, 0);
		if (fd2 < 0) {
			perror("accept");
			return 1;
		}
		res = fork();
		if (res < 0) {
			perror("fork");
			return 1;
		}
		if (res == 0) {
			close(fd);
			in = out = fdopen(fd2, "r+");
			if (!in) {
				perror("fdopen");
				return 1;
			}
			break;
		}
		close(fd2);
		int meh;
		while (waitpid(-1, &meh, WNOHANG) > 0);
	}
#else
	in = stdin;
	out = stdout;
#endif
	setbuf(in, 0);
	setbuf(out, 0);
	while (1)
		doit();
	return 0;
}
