/*
 * loraNew.c
 *
 *  Created on: 30 Kas 2022
 *      Author: Tarik Esen
 */

#include "lora.h"

void init(Lora_Module* device,UART_HandleTypeDef* stream){
	/*Initialize UART Module*/
	device->stream = stream;

	/*Initialize Initial Mode*/
	setMode(device, MODE_0_NORMAL);

	/*Initialize BPS Rate*/
	device->bpsrate = UART_BPS_RATE_9600;

	/*Initialize Config*/
	device->config.HEAD = WRITE_CFG_PWR_DWN_SAVE; //Byte 0
	device->config.ADDH = 0x00; //Byte 1
	device->config.ADDL = 0x00; //Byte 2

	//Byte 3
	device->config.SPED.airDataRate =  AIR_DATA_RATE_010_24;
	device->config.SPED.uartBaudRate = UART_BPS_RATE_9600;
	device->config.SPED.uartParity = MODE_00_8N1;

	device->config.CHAN = 0x17; //Byte 4

	//Byte 5
	device->config.OPTION.ioDriveMode = IO_D_MODE_PUSH_PULLS_PULL_UPS;
	device->config.OPTION.fixedTransmission = FT_TRANSPARENT_TRANSMISSION;
	device->config.OPTION.wirelessWakeupTime = WAKE_UP_250;
	device->config.OPTION.fec =  FEC_1_ON;
	device->config.OPTION.transmissionPower = POWER_20;

	setConfiguration(device, &(device->config), WRITE_CFG_PWR_DWN_SAVE);

	/*Initialize Device Info*/
	device->info.HEAD = WRITE_CFG_PWR_DWN_SAVE;
	device->info.features = "UART-Based LoRa Module";
	device->info.frequency = 410 + (device->config.CHAN);
}


//Temel AUX PIN kontrolu ve indikasyonunu saglar
Status waitCompleteResponse(uint32_t timeout){
	//Durumu basarili olarak ayarla
	Status result = E32_SUCCESS;

	//Sistemin o anki zamanini al
	uint32_t t = HAL_GetTick();

	//Sayacin veri boyutu limitini asmasi durumunda sayaci sifirla
	if(((uint32_t)(t+timeout)) == 0) t = 0;

	//AUX Pinini 0 oldugu surece oku
	while(HAL_GPIO_ReadPin(AUXPIN) == 0){
		//Timeout suresi dolarsa ve AUX halen 0 ise basarisiz exception dondur
		if(HAL_GetTick()-t > timeout){
			result = ERR_E32_TIMEOUT;
			return result;
		}
	}

	/*Timeout suresi dolmadan AUX 1 olursa gerekli hazirlik delayini ver
	ve basarili dondur*/
	managedDelay(20);
	return result;
}

//Planli ve kontrol edilebilen gecikme fonksiyonu
void managedDelay(uint32_t timeout){
	uint32_t t = HAL_GetTick(); //Sistem sayacini oku

	//Sayacin veri boyutu limitini asmasi durumunda sayaci sifirla
	if (((uint32_t)(t + timeout)) == 0)t = 0;

	//Gerekli gecikme suresi kadar bekle
	while ((HAL_GetTick() - t) < timeout);
}

//Mod degistirme(Mode-Switch) islemini gercekleyen fonksiyon
Status setMode(Lora_Module* device, MODE_TYPE mode) {

	/*Datasheette yazilan mode-switch icin gerekli gecikme suresiyle
	sistemi beklet*/
	managedDelay(40);

	switch (mode){
		  case MODE_0_NORMAL:
			  // Mode 0 | Normal moda girmek icin gerekli degerleri pinlere yaz
			  HAL_GPIO_WritePin(M0PIN, 0);
			  HAL_GPIO_WritePin(M1PIN, 0);
			  break;
		  case MODE_1_WAKE_UP:
			  // Mode 1 | Uyanma moduna girmek icin gerekli degerleri pinlere yaz
			  HAL_GPIO_WritePin(M0PIN, 1);
			  HAL_GPIO_WritePin(M1PIN, 0);
			  break;
		  case MODE_2_POWER_SAVING:
			  // Mode 2 | Guc tasarrufu moduna girmek icin gerekli degerleri pinlere yaz
			  HAL_GPIO_WritePin(M0PIN, 0);
			  HAL_GPIO_WritePin(M1PIN, 1);
			  break;
		  case MODE_3_SLEEP:
			  // Mode 3 | Uyku/Program moduna girmek icin gerekli degerleri pinlere yaz
			  HAL_GPIO_WritePin(M0PIN, 1);
			  HAL_GPIO_WritePin(M1PIN, 1);
			  break;
		  default:
			//Farkli bir mod parametresi girilirse basarisiz dondur
			return ERR_E32_INVALID_PARAM;
		}

	//Pinlerin aktive edilmesi icin ekstra sure ver
	managedDelay(40);

	//AUX PIN'i yeniden uygun degere ulasana kadar bekle
	Status res = waitCompleteResponse(1000);

	//Islem basariliysa cihazin mod durumunu guncelle ve basarili dondur
	if (res == E32_SUCCESS)
		device->mode = mode;

	return res;
}

//Cihazin varolan modunu donduren fonksiyon
MODE_TYPE getMode(Lora_Module* device){
	return device->mode;
}

//Cihaza programlama modundayken istenilen komutu gonderen fonksiyon
void writeProgramCommand(Lora_Module* device, PROGRAM_COMMAND cmd){
	uint8_t CMD[3] = {cmd, cmd, cmd}; //Komutlari diziye yaz
	HAL_UART_Transmit(device->stream, CMD, 3, 100); //3 defa pesisira module ilet
	managedDelay(2); //Komutlari ilettikten sonra gerekli gecikmeyi ver
}

/*UART'in komutlanmak icin uygun konfigurasyona sahip olup olmadigini
kontrol eden fonksiyon*/
Status checkUARTConfiguration(Lora_Module* device, MODE_TYPE mode){
	if(mode == MODE_3_PROGRAM && device->bpsrate != UART_BPS_RATE_9600)
		return ERR_E32_WRONG_UART_CONFIG;
	else return E32_SUCCESS;
}

//Herhangi bir veri paketini byte'lar halinde programdan module gonderen temel fonksiyon
Status sendStruct(Lora_Module* device ,void *object, uint16_t size){
	Status result = E32_SUCCESS;

	//Gonderimek istenen veri maksimum boyutu asarsa basarisiz dondur
	if(size > MAX_SIZE_TX_PACKET + 2){
		return ERR_E32_PACKET_TOO_BIG;
	}

	/*Paketi istenilen adresten baslayarak byte'lar halinde
	istenilen boyutta teker teker gonder*/
	HAL_UART_Transmit(device->stream, (uint8_t*)object, size, 100);

	//AUX PIN'inin istenilen seviyeye ulasmasini bekle
	result = waitCompleteResponse(1000);
	if(result != E32_SUCCESS) return result;//Ulasmassa basarisiz dondur

	/*Ulasirsa bir sonraki veri transferi icin UART cevrebiriminin
	bufferini temizle ve basarili dondur*/
	cleanUARTBuffer(device);
	return result;
}

//Herhangi bir veri paketini byte'lar halinde modulden programa alan temel fonksiyon
Status receiveStruct(Lora_Module* device ,void* object, uint16_t size){
	Status result = E32_SUCCESS;

	/*Paketi istenilen adresten baslayarak byte'lar halinde
	istenilen boyutta teker teker al*/
	HAL_UART_Receive(device->stream, (uint8_t*)object, size, 100);

	//AUX PIN'inin istenilen seviyeye ulasmasini bekle
	result = waitCompleteResponse(1000);
	if(result != E32_SUCCESS) return result;//Ulasmassa basarisiz dondur

	return result; //Ulasirsa basarili dondur
}

//UART bufferini bir sonraki transferler icin temizleyen fonksiyon
void cleanUARTBuffer(Lora_Module* device){
	HAL_UART_Receive(device->stream, (uint8_t*)"", 1, 100);
}

/*Modulun adreslenme ve altyapisal ayarlarini barindiran konfigurasyon
  cercevesinin hazirlanmasi ve modulun bu cerceve uzerinden yapilandirilmasini
  saglayan temel fonksiyon*/
ResponseStatus setConfiguration(Lora_Module* device, Lora_Configuration* config, PROGRAM_COMMAND saveType){
	ResponseStatus rc;
	CONF_MESSAGE msg;

	//Programlama modu icin UART cevrebiriminin uygunluk durumunu kontrol et
	rc.code = checkUARTConfiguration(device, MODE_3_PROGRAM);
	if (rc.code != E32_SUCCESS) return rc; //Uygun degilse basarisiz dondur

	//Uygunsa mevcut modu degiskene kaydederek programlama moduna gecis yap
	MODE_TYPE prevMode = getMode(device);
	rc.code = setMode(device, MODE_3_PROGRAM);
	if (rc.code != E32_SUCCESS) return rc;// Islem basarisiz olursa basarisiz dondur

	//Basarili olursa konfigurasyon okuma komutunu belli bir frekansla module ileterek modulu konfigurasyon sureci icin ayarla
	writeProgramCommand(device, READ_CONFIGURATION);

	//Konfigurasyonun kaydedilme tipini alinan arguman uzerinden belirle
	config->HEAD = saveType;

	//Hazirlanan konfigurasyon cercevesine module ileterek modulu ayarla
	msg = generateConfMessage(config);
	rc.code = sendStruct(device, (uint8_t *)&msg, sizeof(msg));

	//Iletim basarisiz olursa kaydedilen eski moda donerek basarisiz dondur
	if (rc.code != E32_SUCCESS) {
		setMode(device, prevMode);
		return rc;
	}

	//Basarili olursa eski moda don
	rc.code = setMode(device, prevMode);
	if (rc.code != E32_SUCCESS) return rc; //Mod degisimi basarisiz olursa basarisiz dondur

	//Mod restorasyonu ve konfigurasyon iletimi basariliysa kaydetme modunun uygunlugunu teyit et
	if (config->HEAD != WRITE_CFG_PWR_DWN_SAVE && config->HEAD != WRITE_CFG_PWR_DWN_LOSE ){
		rc.code = ERR_E32_HEAD_NOT_RECOGNIZED; // Teyit basarisizsa status flag'ini basarisiz'a set et ve dondur
	}

	device->config = *config;
	device->info.HEAD = config->HEAD;
	/*Mod degisimi, programlama, konfigurasyon kaydi, mod restorasyonu ve kaydetme teyiti islemleri sirayla basariliysa
	  islemi basarili dondur*/
	return rc;
}


//Cihaz uzerine yapilan konfigurasyondaki ayar parametrelerini nesne halinde dondur
ResponseStructContainer getConfiguration(Lora_Module* device){
	ResponseStructContainer rc;
	Lora_Configuration config;
	CONF_MESSAGE confMsg;

	rc.status.code = checkUARTConfiguration(device, MODE_3_PROGRAM);
	if (rc.status.code != E32_SUCCESS) return rc;

	MODE_TYPE prevMode = getMode(device);

	rc.status.code = setMode(device, MODE_3_PROGRAM);
	if (rc.status.code != E32_SUCCESS) return rc;


	writeProgramCommand(device, READ_CONFIGURATION);

	rc.data = (Lora_Configuration*) malloc(sizeof(Lora_Configuration));

	rc.status.code = receiveStruct(device, (uint8_t *)&confMsg, sizeof(CONF_MESSAGE));

	config = (Lora_Configuration)generateConfig(&confMsg);
	*((Lora_Configuration*)(rc.data)) = config;
	if (rc.status.code!=E32_SUCCESS) {
		setMode(device, prevMode);
		return rc;
	}

	rc.status.code = setMode(device, prevMode);
	if (rc.status.code!=E32_SUCCESS) return rc;

	if (0xC0 != ((Lora_Configuration *)rc.data)->HEAD && 0xC2 != ((Lora_Configuration *)rc.data)->HEAD){
		rc.status.code = ERR_E32_HEAD_NOT_RECOGNIZED;
	}
	return rc;
}

CONF_MESSAGE generateConfMessage(Lora_Configuration* config){
	CONF_MESSAGE msg;
	uint8_t sped = 0, opt = 0;
	msg.PARAMS[0] = config->HEAD;

	msg.PARAMS[1] = config->ADDH;

	msg.PARAMS[2] = config->ADDL;

	sped |= ( (config->SPED.uartParity) << 6);
	sped |= ( (config->SPED.uartBaudRate << 3));
	sped |= ( (config->SPED.airDataRate));
	msg.PARAMS[3] = sped;

	msg.PARAMS[4] = config->CHAN;

	opt |= ( (config->OPTION.fixedTransmission << 7));
	opt |= ( (config->OPTION.ioDriveMode << 6));
	opt |= ( (config->OPTION.wirelessWakeupTime << 3));
	opt |= ( (config->OPTION.fec << 2));
	opt |= ( (config->OPTION.transmissionPower));
	msg.PARAMS[5] = opt;

	return msg;
}

Lora_Configuration generateConfig(CONF_MESSAGE* confMessage){
	Lora_Configuration config;
	uint8_t sped = 0,opt = 0;

	config.HEAD = confMessage->PARAMS[0];
	config.ADDH = confMessage->PARAMS[1];
	config.ADDL = confMessage->PARAMS[2];

	sped = confMessage->PARAMS[3];
	config.SPED.uartParity = ( (sped & (3 << 6)) >> 6);
	config.SPED.uartBaudRate = ( (sped & (7 << 3)) >> 3);
	config.SPED.airDataRate = (sped & 7);

	config.CHAN = confMessage->PARAMS[4];

	opt = confMessage->PARAMS[5];
	config.OPTION.fixedTransmission = ( (opt & (1 << 7)) >> 7);
	config.OPTION.ioDriveMode = ( (opt & (1 << 6)) >> 6);
	config.OPTION.wirelessWakeupTime = ( (opt & (7 << 3)) >> 3);
	config.OPTION.fec = ( (opt & (1 << 2)) >> 2);
	config.OPTION.transmissionPower = (opt & 3);

	return config;
}


ResponseStructContainer getModuleInformation(Lora_Module* device){
	ResponseStructContainer rc;

	rc.status.code = checkUARTConfiguration(device, MODE_3_PROGRAM);
	if (rc.status.code != E32_SUCCESS) return rc;


	MODE_TYPE prevMode = getMode(device);
	rc.status.code = setMode(device, MODE_3_PROGRAM);
	if (rc.status.code != E32_SUCCESS) return rc;


	writeProgramCommand(device, READ_MODULE_VERSION);

	Lora_ModuleInfo *moduleInformation = (Lora_ModuleInfo*) malloc(sizeof(moduleInformation));
	rc.status.code = receiveStruct(device, (uint8_t *)moduleInformation, sizeof(moduleInformation));
	if (rc.status.code != E32_SUCCESS) {
		setMode(device, prevMode);
		return rc;
	}

	rc.status.code = setMode(device, prevMode);
	if (rc.status.code!=E32_SUCCESS) return rc;

	if (0xC3 != moduleInformation->HEAD){
		rc.status.code = ERR_E32_HEAD_NOT_RECOGNIZED;
	}
	else rc.data = moduleInformation; 

	return rc;
}

ResponseStatus resetModule(Lora_Module* device){
	ResponseStatus status;

	status.code = checkUARTConfiguration(device, MODE_3_PROGRAM);
	if (status.code != E32_SUCCESS) return status;

	MODE_TYPE prevMode = getMode(device);

	status.code = setMode(device, MODE_3_PROGRAM);
	if (status.code!=E32_SUCCESS) return status;

	writeProgramCommand(device, WRITE_RESET_MODULE);

	status.code = waitCompleteResponse(1000);
	if (status.code!=E32_SUCCESS)  {
		setMode(device, prevMode);
		return status;
	}


	status.code = setMode(device, prevMode);
	if (status.code != E32_SUCCESS) return status;

	init(device, device->stream);
	return status;
}

ResponseStructContainer receiveMessage(Lora_Module* device, void* message, const uint16_t size){
	ResponseStructContainer rc;

	rc.status.code = receiveStruct(device, (uint8_t *)message, size);

	cleanUARTBuffer(device);
	if (rc.status.code != E32_SUCCESS) {
		return rc;
	}
	rc.data = message;
	return rc;
}

ResponseStructContainer receiveMessageS(Lora_Module* device, const uint8_t size){
	ResponseStructContainer rc;

	rc.data = malloc(size);
	rc.status.code = receiveStruct(device, (uint8_t *)rc.data, size);
	cleanUARTBuffer(device);
	if (rc.status.code!=E32_SUCCESS) {
		return rc;
	}

	return rc;
}

ResponseStatus sendMessage(Lora_Module* device, void *message, const uint8_t size){
	ResponseStatus status;

	status.code = sendStruct(device, (uint8_t *)message, size);

	if (status.code!=E32_SUCCESS) return status;
	return status;
}

//END LINE
