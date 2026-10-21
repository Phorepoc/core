/* Copyright The Phorepoc Authors
 * SPDX-License-Identifier: Apache-2.0 */

// phkbd_langs.h -- OURS (Phorepoc): the installed keyboard languages of phkbd. To add a language: write its
// phkbd_lang_<code>.h (see phkbd_lang_en.h), include it here and add it to KPhLanguages.
#ifndef PHKBD_LANGS_H
#define PHKBD_LANGS_H

#include "phkbd_lang_en.h"

static const TPhLanguage* const KPhLanguages[] = { &KPhLangEn };
static const TInt KPhLanguageCount = sizeof( KPhLanguages ) / sizeof( KPhLanguages[0] );

#endif
