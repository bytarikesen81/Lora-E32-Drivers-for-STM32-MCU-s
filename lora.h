/*
 * lora.h
 *
 *  Created on: 30 Kas 2022
 *      Author: Tarik Esen
 */

#ifndef USER_INC_LORA_H_
#define USER_INC_LORA_H_

#include "stm32f4xx_hal.h"
#include <stdlib.h>

#define MAX_SIZE_TX_PACKET 58

#define AUXPIN GPIOB, GPIO_PIN_4
#define M0PIN  GPIOB, GPIO_PIN_6
#define M1PIN  GPIOB, GPIO_PIN_5

typedef enum{
  MODE_0_NORMAL 		= 0,
  MODE_1_WAKE_UP 		= 1,
  MODE_2_POWER_SAVING 	= 2,
  MODE_3_SLEEP 			= 3,
  MODE_3_PROGRAM 		= 3,
  MODE_INIT 			= 0xFF
}MODE_TYPE;

typedef enum
{
  MODE_00_8N1 = 0b00,
  MODE_01_8O1 = 0b01,
  MODE_10_8E1 = 0b10,
  MODE_11_8N1 = 0b11
}UART_PARITY;

typedef enum
{
  UART_BPS_1200 = 0b000,
  UART_BPS_2400 = 0b001,
  UART_BPS_4800 = 0b010,
  UART_BPS_9600 = 0b011,
  UART_BPS_19200 = 0b100,
  UART_BPS_38400 = 0b101,
  UART_BPS_57600 = 0b110,
  UART_BPS_115200 = 0b111
}UART_BPS_TYPE;

typedef enum{
   UART_BPS_RATE_1200 	  = 0,
   UART_BPS_RATE_2400 	  = 1,
   UART_BPS_RATE_4800 	  = 2,
   UART_BPS_RATE_9600 	  = 3,
   UART_BPS_RATE_19200 	  = 4,
   UART_BPS_RATE_38400 	  = 5,
   UART_BPS_RATE_57600 	  = 6,
   UART_BPS_RATE_115200   = 7,
}UART_BPS_RATE;

typedef enum
{
  AIR_DATA_RATE_000_03 = 0b000,
  AIR_DATA_RATE_001_12 = 0b001,
  AIR_DATA_RATE_010_24 = 0b010,
  AIR_DATA_RATE_011_48 = 0b011,
  AIR_DATA_RATE_100_96 = 0b100,
  AIR_DATA_RATE_101_192 = 0b101,
  AIR_DATA_RATE_110_192 = 0b110,
  AIR_DATA_RATE_111_192 = 0b111
}AIR_DATA_RATE;

typedef enum{
  WRITE_CFG_PWR_DWN_SAVE  	= 0xC0,
  READ_CONFIGURATION 		= 0xC1,
  WRITE_CFG_PWR_DWN_LOSE 	= 0xC2,
  READ_MODULE_VERSION   	= 0xC3,
  WRITE_RESET_MODULE     	= 0xC4
}PROGRAM_COMMAND;

typedef enum{
	E32_SUCCESS,
	ERR_E32_TIMEOUT,
	ERR_E32_INVALID_PARAM,
	ERR_E32_WRONG_UART_CONFIG,
	ERR_E32_HEAD_NOT_RECOGNIZED,
	ERR_E32_PACKET_TOO_BIG
}Status;

typedef enum
{
  POWER_20 = 0b00,
  POWER_17 = 0b01,
  POWER_14 = 0b10,
  POWER_10 = 0b11
}TRANSMISSION_POWER;

typedef enum
{
  FEC_0_OFF = 0b0,
  FEC_1_ON = 0b1
}FORWARD_ERROR_CORRECTION_SWITCH;

typedef enum
{
  WAKE_UP_250 = 0b000,
  WAKE_UP_500 = 0b001,
  WAKE_UP_750 = 0b010,
  WAKE_UP_1000 = 0b011,
  WAKE_UP_1250 = 0b100,
  WAKE_UP_1500 = 0b101,
  WAKE_UP_1750 = 0b110,
  WAKE_UP_2000 = 0b111
}WIRELESS_WAKE_UP_TIME;

typedef enum
{
  IO_D_MODE_OPEN_COLLECTOR = 0b0,
  IO_D_MODE_PUSH_PULLS_PULL_UPS = 0b1
}IO_DRIVE_MODE;

typedef enum
{
  FT_TRANSPARENT_TRANSMISSION = 0b0,
  FT_FIXED_TRANSMISSION = 0b1
}FIDEX_TRANSMISSION;

#pragma pack(push, 1)


typedef struct{
	UART_PARITY uartParity;
	UART_BPS_RATE uartBaudRate;
	AIR_DATA_RATE airDataRate;
}Lora_SpeedTDef;

typedef struct{
	FIDEX_TRANSMISSION fixedTransmission;
	IO_DRIVE_MODE ioDriveMode;
	WIRELESS_WAKE_UP_TIME wirelessWakeupTime;
	FORWARD_ERROR_CORRECTION_SWITCH fec;
	TRANSMISSION_POWER transmissionPower;
}Lora_OptionTDef;

typedef struct{
	PROGRAM_COMMAND HEAD;
	uint8_t ADDH;
	uint8_t ADDL;
	Lora_SpeedTDef SPED;
	uint8_t CHAN;
	Lora_OptionTDef OPTION;
}Lora_Configuration;

typedef struct{
	uint8_t HEAD;
	uint8_t frequency;
	uint8_t version;
	char *features;
}Lora_ModuleInfo;

typedef struct{
	Status code;
}ResponseStatus;

typedef struct{
	void *data;
	ResponseStatus status;
}ResponseStructContainer;

typedef struct{
	char* data;
	ResponseStatus status;
}ResponseContainer;


typedef struct{
	uint8_t ADDH;
	uint8_t ADDL;
	uint8_t CHAN;
	uint8_t DATA;
}fixedStransmission;

typedef struct{
	uint8_t PARAMS[6];
}CONF_MESSAGE;


//**** LoRa Module Instance(Main) Field ****//
typedef struct{
	UART_HandleTypeDef* stream;
	MODE_TYPE mode;
	Lora_Configuration config;
	Lora_ModuleInfo	info;
	UART_BPS_RATE bpsrate;

}Lora_Module;
//**** LoRa Module Instance(Main) Field ****//


#pragma pack(pop)

void init(Lora_Module*, UART_HandleTypeDef*);

Status waitCompleteResponse(uint32_t); 
void managedDelay(uint32_t); 
int available();
//void flush();

Status setMode(Lora_Module*, MODE_TYPE);
MODE_TYPE getMode();

void writeProgramCommand(Lora_Module*,PROGRAM_COMMAND);

Status checkUARTConfiguration(Lora_Module*, MODE_TYPE); 

ResponseStructContainer getConfiguration(Lora_Module*); 
ResponseStatus setConfiguration(Lora_Module*, Lora_Configuration*, PROGRAM_COMMAND);

CONF_MESSAGE generateConfMessage(Lora_Configuration*);
Lora_Configuration generateConfig(CONF_MESSAGE*);

void cleanUARTBuffer(Lora_Module*);

Status sendStruct(Lora_Module*, void*, uint16_t);
Status receiveStruct(Lora_Module*, void*, uint16_t);

ResponseStructContainer getModuleInformation(Lora_Module*);
ResponseStatus resetModule(Lora_Module*);

ResponseStructContainer receiveMessage(Lora_Module*, void*, const uint16_t);
ResponseContainer receiveMessageUntil(uint8_t);
ResponseStructContainer receiveMessageOf(const uint8_t);

ResponseStatus sendMessage(Lora_Module*, void*, const uint8_t);

ResponseStructContainer receiveMessageS(Lora_Module*, const uint8_t);

#endif /* USER_INC_LORA_H_ */
