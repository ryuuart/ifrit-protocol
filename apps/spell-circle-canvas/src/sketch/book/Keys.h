#pragma once

/** @file
 * The key a sketch is handed: the name a keyboard spells it by, and the
 * code p5 gives it.
 */

#include <QtCore/QString>
#include <string>
#include <utility>

/** THE KEY AS A SKETCH READS IT: the name a keyboard spells it by, and
 *  the code p5 gives it. The keys p5 names get p5's numbers — the
 *  arrows, Enter, Escape, Backspace, Delete, Tab, the modifiers, the
 *  function keys — a key that types a character is that character, with
 *  a letter's code its upper-case ASCII the way p5 reports it, and any
 *  other key keeps Qt's name and number.
 *
 *  The modifiers are Qt's, which on macOS reports the Command key as
 *  Control: the key a shortcut is SPELLED with rather than the one the
 *  keyboard is engraved with. A sketch comparing against p5's numbers is
 *  comparing against the same key it would press to save a file. */
std::pair<std::string, int> keyAs(int qtKey, const QString& text);
