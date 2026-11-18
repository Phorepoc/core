/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: EPL-2.0 OR GPL-2.0-or-later
 *
 * phe_main.c -- zsh's main() (in place of Src/main.c): a start/exit line on the debug log ("[zsh] ..."), then zsh.
 */
#include "zsh.mdh"

void phe_debug(const char *fmt, int a, int b);

int main(int argc, char **argv)
{
	int r;
	phe_debug("[zsh] start, %d arg(s), pid %d", argc, (int)getpid());
	r = zsh_main(argc, argv);
	phe_debug("[zsh] main returned %d (pid %d)", r, (int)getpid());
	return r;
}
