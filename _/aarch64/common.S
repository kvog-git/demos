// ================================================================================================
// Shared macros
//
// License:
//     Copyright (c) 2026 Hunter Kvalevog
//
//     Permission to use, copy, modify, and/or distribute this software for any
//     purpose with or without fee is hereby granted.
//
//     THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
//     WITH REGARD TO THIS SOFTWARE.
// ================================================================================================

#ifdef __APPLE__
#   define SYM(LABEL) _##LABEL
#   define TEXT_SEGMENT .section __TEXT,__text
#else
#   define SYM(LABEL) LABEL
#   define TEXT_SEGMENT .section .text
#endif

