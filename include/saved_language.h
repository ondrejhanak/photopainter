#ifndef SAVED_LANGUAGE_H
#define SAVED_LANGUAGE_H

#ifdef __cplusplus
extern "C" {
#endif

// Language from the last settings.ini read, kept in the CPU flash; -1 if none was saved
int saved_language_load(void);
// Save the language if it differs from the saved one (erases a flash sector, so only on changes)
void saved_language_save(int language);

#ifdef __cplusplus
}
#endif

#endif
