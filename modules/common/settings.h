#if !defined (SETTINGS_H)
#define SETTINGS_H

inline void cc_set_setting(const char* filename, const char *name, const char *value) {
    getplatform()->getlogr("df.settings")->info("cc_set_setting(%s.%s = %s)", filename, name, value);
}
inline const char *cc_get_setting(const char *name) {
    getplatform()->getlogr("df.settings")->info("cc_get_setting(%s)", name);
    return NULL;
}

#endif