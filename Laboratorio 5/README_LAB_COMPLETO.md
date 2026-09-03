# Laboratorio 5 completo - Partes 1, 2 y 3

Proyecto para la tarjeta **NUCLEO-F446RE** que mide la frecuencia de una señal
cuadrada mediante captura de entrada de TIM2, envía el resultado continuamente
por USART2 y genera simultáneamente dos señales cuadradas para LEDs.

## Conexiones

- Salida del generador de funciones -> **PA0 (A0 / TIM2_CH1)**.
- Tierra del generador -> **GND** de la Nucleo.
- Comunicación serial -> el mismo cable USB del ST-LINK (USART2: PA2/PA3).
- Ajustar la señal cuadrada a **0-3.3 V** antes de conectarla. No aplicar voltajes
  negativos ni superar 3.3 V en PA0.

## Configuración de la terminal serial

- Velocidad: **115200 baudios**.
- Datos: **8 bits**.
- Paridad: **ninguna**.
- Bits de parada: **1**.
- Control de flujo: **ninguno**.

El programa reporta cada 500 ms una línea como:

```text
Frecuencia medida: 1000 Hz
```

Si no se detectan flancos durante 2 segundos, reporta 0 Hz. El LED LD2 cambia
de estado cada vez que se completa un nuevo promedio de medición.

## Configuración implementada

- Reloj del sistema: 180 MHz.
- TIM2: reloj de 90 MHz, prescaler 89, contador de 32 bits a 1 MHz.
- TIM2_CH1: captura por flanco ascendente en PA0.
- Medición: diferencia entre capturas sucesivas y promedio de 8 períodos.
- USART2: 115200, 8N1, transmisión por el puerto virtual del ST-LINK.

Los TIM3 y TIM4 del proyecto original se conservaron para que la parte 3 pueda
funcionar simultáneamente, como solicita la nota final del laboratorio.

## Parte 3 - Dos señales cuadradas

Las salidas se generan con interrupciones de dos temporizadores diferentes. No
se utiliza `HAL_Delay()`, por lo que la medición de frecuencia y la transmisión
UART continúan ejecutándose mientras los LEDs parpadean.

| Señal | Timer | Salida | Conector | Cambio de estado | Período completo |
|---|---|---|---|---:|---:|
| LED rápido | TIM3 | PB0 | A3 (CN8 pin 4) | 250 ms | 500 ms |
| LED lento | TIM4 | PB1 | Morpho CN10 pin 24 | 1 s | 2 s |

### Conexión de los LEDs

Para cada salida utilice una resistencia de 220 a 330 ohmios:

```text
PB0/A3 ---------- resistencia ---------- ánodo LED 1
                                             cátodo ---------- GND

PB1/CN10-24 ----- resistencia ---------- ánodo LED 2
                                             cátodo ---------- GND
```

El ánodo normalmente es la pata larga y el cátodo la pata corta o el lado plano
del encapsulado. Ambos LEDs deben compartir GND con la Nucleo.

### Cálculo de los tiempos

TIM3 y TIM4 reciben 90 MHz. Con prescaler 8999, ambos cuentan a 10 kHz:

```text
90 000 000 / (8999 + 1) = 10 000 cuentas/s
```

- TIM3 usa ARR = 2499: interrumpe cada 2500/10000 = 0.25 s. Como el pin cambia
  en cada interrupción, 250 ms permanece apagado y 250 ms encendido; el período
  completo es 500 ms.
- TIM4 usa ARR = 9999: interrumpe cada 10000/10000 = 1 s. El pin permanece 1 s
  apagado y 1 s encendido; el período completo es 2 s.

## Prueba sugerida

1. Abrir el proyecto en STM32CubeIDE y compilarlo.
2. Programar la NUCLEO-F446RE.
3. Abrir el puerto COM del ST-LINK a 115200, 8N1.
4. Comenzar con una señal cuadrada de 1 kHz y 0-3.3 V en PA0.
5. Verificar una lectura cercana a 1000 Hz.
6. Cambiar a 500 Hz y 2 kHz y comprobar que el valor se actualice.
7. Desconectar la señal y comprobar que, después de 2 segundos, aparezca 0 Hz.
8. Comprobar que el LED de PB0 parpadee cada 500 ms y el de PB1 cada 2 s, mientras
   la frecuencia continúa actualizándose por UART.
