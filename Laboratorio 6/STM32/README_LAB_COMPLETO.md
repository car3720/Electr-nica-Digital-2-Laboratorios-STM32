# Laboratorio 6 - Control de videojuego

Proyecto completo para **NUCLEO-F446RE / STM32F446RETx**, organizado con la
misma estructura del proyecto de referencia de Laboratorio 5.

## Contenido

- `Core/Inc`: encabezados de la aplicacion y de interrupciones.
- `Core/Src`: `main.c`, MSP, IRQ y soporte del sistema.
- `Core/Startup`: arranque para STM32F446RETx.
- `Drivers`: CMSIS y HAL STM32F4 incluidos.
- `Laboratorio_6.ioc`: configuracion para STM32CubeMX.
- Archivos `.project`, `.cproject`, `.mxproject` y launch de STM32CubeIDE.
- El sketch del Arduino Nano esta en `../ATmega328P`.

## Pines

| Funcion | Pin |
| --- | --- |
| Joystick vertical | PA0 / ADC1_IN0 |
| Joystick horizontal | PA1 / ADC1_IN1 |
| Recepcion desde Nano | PA10 / USART1_RX, 9600 8N1 |
| Transmision opcional al Nano | PA9 / USART1_TX |
| Terminal ST-LINK VCP | PA2/PA3 / USART2, 115200 8N1 |
| Nano SoftwareSerial TX | D11, mediante convertidor 5 V a 3.3 V |

## Uso

1. Importe `Laboratorio_6` como proyecto existente de STM32CubeIDE.
2. Compile y programe la Nucleo-F446RE.
3. Cargue `ATmega328P/atmega328p_control.ino` al Arduino Nano.
4. Una las tierras y conecte Nano D11 a PA10 mediante conversion de nivel.
5. Abra el puerto COM de ST-LINK a 115200 baudios.

La recepcion de USART1 se realiza mediante la interrupcion RXNE. ADC1 recorre
PA0 y PA1 continuamente y DMA2 Stream0 guarda ambas muestras en modo circular.

Toda la logica y configuracion agregada de `main.c` se encuentra exclusivamente dentro de
los espacios `USER CODE BEGIN` y `USER CODE END` designados por STM32CubeMX.
El resto del archivo conserva la plantilla base de la Nucleo proporcionada.

## Importante

No conecte directamente una salida de 5 V del Nano a PA10. Use el convertidor
de nivel indicado en la guia del laboratorio y comparta GND entre ambas placas.
