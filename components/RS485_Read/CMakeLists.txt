idf_component_register(SRCS ./RS485_Read.c
                       INCLUDE_DIRS "include"
                       REQUIRES base64 CSE7759B Nvs ServerTimer Uart0 Bluetooth ds18b20 Json_parse ota Smartconfig user_keydriver bsp E2prom Led RS485_Read Spi_flash Cache_data Http Mqtt RtcUsr Switch)                    # Edit following two lines to set component requirements (see docs)

