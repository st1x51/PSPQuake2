/* Russian Kingpin menu glyph encoding (ASCII slots in conchars2.tga).
 * The second argument is NOT transliteration: render with the Russian font.
 * Keep commands, cvar names and persisted key names outside this macro.
 */
#ifndef CLIENT_MENU_RU_H
#define CLIENT_MENU_RU_H
#ifdef KINGPIN_ENGINE
#define M_RUS(english, russian) russian
#else
#define M_RUS(english, russian) english
#endif
#endif
