/*
 * Simple interactive console line reading function,
 * with no editing support other than handling backspace.
 *
 * Copyright (C) 1997 Massachusetts Institute of Technology
 * See section "MIT License" in the file LICENSES for licensing terms.
 *
 * Derived from the MIT Exokernel and JOS.
 */

#include <inc/stdio.h>
#include <inc/unistd.h>
#include <inc/assert.h>
#include <inc/string.h>

int key(int echoing, int hist_pref_len, int buf_len, int *start);
int hist_search(char *pref, int *start);

#define BUFLEN 1024
#define MAX_HISTORY 32
static char buf[BUFLEN];
static char history[MAX_HISTORY][BUFLEN];
static int  hist_index = 0;

char *
readline(const char *prompt)
{
	int i, c, echoing, 
	start = hist_index, // Where to start scanning the history
	keep_hist, 
	hist_len;						// Length of history prefix

	if (prompt != NULL)
		fprintf(stdout, "%s", prompt);

	i = keep_hist = 0;
	echoing = isatty(0);
	while (1) {
		if(!keep_hist) {
			// cprintf("updating hist_len from %d to %d\n", hist_len, i);
			hist_len = i;
		}
		if(start < 0) start = 0;
		c = getchar();
		if (c < 0) {
			cprintf("Got input...%c|%d. EOF: %d\n", c, c, EOF);
			if (c != EOF)
				cprintf("read error: %e\n", c);
			return NULL;
		} 
		if (c == 27) {
			c = getchar();
			assert(c == 91);
			c = getchar();
			int k;
			keep_hist = 1;	// Mark that hist_i should not be updated
			// cprintf("hist_i: %d, i: %d\n", hist_len, i);
			switch (c) {
				case 65:
					// cprintf("%d", start);
					i = key(echoing, hist_len, i, &start);
					break;
				case 66:
					// i = key(echoing, i, 1, &start);
					for(k=0; k < hist_index; k++)
						printf("%d: %s\n", k, history[k]);
					break;
				case 67:
					cprintf("i: %d", hist_index);
					break;
				case 68:
					cprintf("s: %d", start);
					break;
			}
			continue;
		} 
		keep_hist = 0;
		if ((c == '\b' || c == '\x7f') && i > 0) {
			if (echoing)
				putchar('\b');
			i--;
		} else if (c >= ' ' && i < BUFLEN-1) {
			if (echoing)
				putchar(c);
			buf[i++] = c;
		} else if (c == '\n' || c == '\r') {
			if (echoing) {
				putchar('\n');
				fflush(stdout);
			}
			buf[i] = 0;
			// If not empty, and not the same command as last (unless its the first command)
			if(i > 0 && (hist_index == 0 || strcmp(buf, history[hist_index-1]))) { 
				// cprintf("readline: adding to %s to %d history\n", buf, hist_index);
				// if(hist_index > 0)
					// cprintf("previous was %s\n", history[hist_index-1]);
				strcpy(history[hist_index++], buf);
				if(hist_index > MAX_HISTORY) 
					hist_index = 0; // History starts over :(
			}
			return buf;
		}
	}
}

// Takes whether the output is echoing, the number put so far
// and whether up (0) or down (1) was pressed
// Returns the new end of buf index
int key(int echoing, int hist_pref_len, int buf_len, int *start) {
	char prefix[hist_pref_len+1]; // +1 for \0
	char *cmd;
	int i, hist_i;
	if(hist_pref_len > 0) {
		memmove(prefix, buf, hist_pref_len);
	}
	prefix[hist_pref_len] = '\0';
	// cprintf("key: prefix (%s), numput: %d\n", prefix, numput);
	hist_i = hist_search(prefix, start);
	if(hist_i == -1)  {// No command found
		// cprintf("key: no matching command\n");
		return buf_len;
	}
	cmd = history[hist_i];
	// cprintf("key: cmd %s found at index %d\n", cmd, hist_i);
	if(echoing) {
		for(; buf_len > 0; buf_len--) {
			putchar('\b');
		}
	}
	for(i=0; i<strlen(cmd); i++) {
		buf[i] = cmd[i];
		if(echoing)
			putchar(cmd[i]);
	}
	return i;
}

int hist_search(char *pref, int *start) {
	int i, found, h = *start;
	// cprintf("hist_search: h: %d, pref: %s\n", h, pref);
	while(h >= 0) {
		found = 1;	// Assume this is a match
		for(i=0; i<strlen(pref); i++) {	// Scan the string, comparing
			if(pref[i] != history[h][i]) {
				found = 0; // Turned out not to be a match
			}
		}
		if(found) {
			*start = h - 1;	// Next time, start search on the one before the one found
			// cprintf("hist_search: return %d\n", h);
			return h;
		}
		h--;	// Go up the list
	}
	// cprintf("hist_search: pref is (%s)", pref);
	// No matches
	return -1;
}

