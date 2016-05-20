#ifndef PARSE_SETTINGS_XML_H_
#define PARSE_SETTINGS_XML_H_

int settings_init();
void settings_close(); 

const char *get_settings_str();
extern char *dsp_ipv6_addr;
#endif
