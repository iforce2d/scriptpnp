
#include "codeEditorPalettes.h"

const TextEditor::Palette myPalette = { {

    // ABGR

    0xff000000,	// None
    0xffff0c06,	// Keyword
    0xff008000,	// Number
    0xff2020a0,	// String
    0xff304070, // Char literal
    0xff000000, // Punctuation
    0xff406060,	// Preprocessor
    0xff404040, // Identifier
    0xff606010, // Known identifier
    0xffc040a0, // Preproc identifier
    0xff30a050, // Comment (single line)
    0xff30a050, // Comment (multi line)
    0xffffffff, // Background
    0xff000000, // Cursor
    0x25800000, // Selection
    0x250010ff, // ErrorMarker
    0x250090ff, // WarningMarker
    0x80f08000, // Breakpoint
    0xffb0b0b0, // Line number
    0x10000000, // Current line fill
    0x10808080, // Current line fill (inactive)
    0x10000000, // Current line edge
    0xff9999ff, // Error Marker Tooltip Title
    0xff99ffff, // Warning Marker Tooltip Title
    0xffdddddd, // Error/Warning Marker Tooltip Details
    0xff0befff, // Find highlight
} };

