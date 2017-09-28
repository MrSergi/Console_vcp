#ifndef MICRORL_FUNC_H_
#define MICRORL_FUNC_H_

#include <stdint.h>


void consoleInit(void);
void consoleInput(uint8_t* Buf, uint32_t Len);
int fc_printf(const char * fmt, ...);

void microrl_terminalInit();
void microrl_terminalProcess();
void microrl_printString(const char *str);
void microrl_printStringWithEndl(const char *str);
void microrl_printEndl();

void cmdStatus(int argc, const char * const * argv);

#endif
