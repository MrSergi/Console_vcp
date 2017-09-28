#include <string.h>				// for strcmp, strstr
#include <stdio.h>
#include "console.h"
#include "stm32f1xx_hal.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "cmsis_os.h"
#include "microrl.h"

#include "main.h"
#include <stdarg.h>

//Define this macros with correct write/read terminal functions
//#define microrl_getChar			USB_GetChar
//#define microrl_sendString		USB_SendString

/* The queue used to store command line. */
static xQueueHandle xQueueCmdBuffer = NULL;

microrl_t rl;					// Main terminal object
microrl_t * prl = &rl;

void microrl_sendString (const char * str);

static void cmdHelp(int argc, const char * const * argv);
static void cmdClear(int argc, const char * const * argv);

static int prv_execute(int argc, const char * const * argv);
#ifdef _USE_COMPLETE
static char ** prv_complet (int argc, const char * const * argv);
#endif
#ifdef _USE_CTLR_C
static void prv_sigint (void);
#endif

void consoleInit(void)
{
	xQueueCmdBuffer = xQueueCreate(16, sizeof(char));
}

void microrl_terminalInit()
{
	microrl_init(prl, microrl_sendString);
	microrl_set_execute_callback (prl, prv_execute);

	#ifdef _USE_COMPLETE
	microrl_set_complete_callback (prl, prv_complet);
	#endif

	#ifdef _USE_CTLR_C
	microrl_set_sigint_callback (prl, prv_sigint);
	#endif
}
void microrl_terminalProcess()
{
	uint8_t ch;

	if (xQueueCmdBuffer)
	{
		xQueueReceive(xQueueCmdBuffer, &ch, portMAX_DELAY);
		microrl_insert_char (prl, (int)ch);
	}
}

void microrl_printString(const char *str)
{
	microrl_sendString(str);
}

void microrl_printStringWithEndl(const char *str)
{
	microrl_sendString(str);
	microrl_sendString(ENDL);
}

void microrl_printEndl()
{
	microrl_sendString(ENDL);
}

//*****************************************************************************
// Обертка функции printf
//
int fc_printf(const char * fmt, ...)
{
	int n = 0;
    va_list va;

    va_start(va, fmt);

	n = vprintf(fmt, va);

    va_end(va);

    return n;
}

/* Таблица команд консоли */
typedef struct _console_cmd_t
{
	char * name;				/* Имя команды */
	void   (*console_cmd)(); 	/* Указатель на функцию команды консоли */
} console_cmd_t;

static const console_cmd_t cmd_keyworld[] =
{
	{ "help",		&cmdHelp 			},
	{ "clear",		&cmdClear 			},
	{ "status",		&cmdStatus 			},
};

#define _NUM_OF_CMD 	(sizeof(cmd_keyworld) / sizeof(cmd_keyworld[0]))

// array for comletion
char * compl_world [_NUM_OF_CMD + 1];

static void cmdHelp(int argc, const char * const * argv)
{
    uint32_t i = 0;

    printf("Available commands:\r\n\r\n");

    for (i = 0; i < _NUM_OF_CMD; i++) // выводим все возможные команды
        printf("%s\r\n", cmd_keyworld[i].name);
}

static void cmdClear(int argc, const char * const * argv)
{
	microrl_printString ("\033[2J");    // ESC seq for clear entire screen
	microrl_printString ("\033[H");     // ESC seq for move cursor at left-top corner
}

static int prv_execute(int argc, const char * const * argv)
{
	int i;

	for(i = 0; i < _NUM_OF_CMD; i++)
	{
		if (strcasecmp(argv[0], cmd_keyworld[i].name) == 0)		// Сравниваем имя команды
		{
			if (cmd_keyworld[i].console_cmd)
				cmd_keyworld[i].console_cmd(argc, argv);	// Вызываем функцию команды
			return 0;
		}
	}

	microrl_printStringWithEndl("Unknown command. Type \"help\" to see available commands");
	return 0;
}

#ifdef _USE_COMPLETE
//TODO simplify this. Quite difficult.
static char ** prv_complet (int argc, const char * const * argv)
{
	int j = 0;

	compl_world [0] = NULL;

	// if there is token in cmdline
	if (argc == 1)
	{
		// get last entered token
		char * bit = (char*)argv [argc-1];
		// iterate through our available token and match it
		for (int i = 0; i < _NUM_OF_CMD; i++) {
			// if token is matched (text is part of our token starting from 0 char)
			if (strstr(cmd_keyworld[i].name, bit) == cmd_keyworld[i].name) {
				// add it to completion set
				compl_world [j++] = cmd_keyworld[i].name;
			}
		}
	}
	else
	{ // if there is no token in cmdline, just printf all available token
		for (; j < _NUM_OF_CMD; j++)
		{
			compl_world[j] = cmd_keyworld[j].name;
		}
	}

	// note! last ptr in array always must be NULL!!!
	compl_world [j] = NULL;
	// return set of variants
	return compl_world;
}
#endif

#ifdef _USE_CTLR_C
static void prv_sigint (void)
{
	NVIC_SystemReset();
}
#endif

//*****************************************************************************
//	Обработчик прерывания по приходу данных по USB
void consoleInput(uint8_t* Buf, uint32_t Len)
{
	uint32_t i;
	portBASE_TYPE xHigherPriorityTaskWoken;

	/* We have not woken a task at the start of the ISR. */
	xHigherPriorityTaskWoken = pdFALSE;

	if (xQueueCmdBuffer)
	{
		for (i = 0; i < Len; i++)
		{
			xQueueSendFromISR(xQueueCmdBuffer, &Buf[i], &xHigherPriorityTaskWoken);
//			xQueueSend(xQueueCmdBuffer, &Buf[i], 0U);
		}
	}

//	HAL_GPIO_TogglePin(LED_ERR_GPIO_Port, LED_ERR_Pin);
	/* Now the buffer is empty we can switch context if necessary. */
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// print to stream callback
void microrl_sendString (const char * str)
{
	CDC_Transmit_FS((uint8_t *)str, strlen(str));
	vTaskDelay(1);
}
