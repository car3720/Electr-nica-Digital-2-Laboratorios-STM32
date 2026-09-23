#include "lab_audio.h"
#include "audio_dac.h"
#include "audio_pwm.h"
#include <string.h>

extern UART_HandleTypeDef huart2;
extern DAC_HandleTypeDef hdac;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim6;

typedef enum { PLAYER_IDLE, PLAYER_DAC, PLAYER_PWM } PlayerMode;

static volatile PlayerMode mode = PLAYER_IDLE;
static volatile uint32_t sample_index = 0;
static volatile uint8_t finished = 0;
static uint8_t rx_byte;

static const char menu[] =
    "\r\n===== REPRODUCTOR DE AUDIO =====\r\n"
    "1: Reproducir audio 1 por DAC\r\n"
    "2: Reproducir audio 2 por PWM\r\n"
    "s: Detener reproduccion\r\n"
    "Seleccione una opcion: ";

static uint32_t timer_clock_apb1(void)
{
    RCC_ClkInitTypeDef clocks;
    uint32_t latency;
    HAL_RCC_GetClockConfig(&clocks, &latency);
    uint32_t pclk = HAL_RCC_GetPCLK1Freq();
    return (clocks.APB1CLKDivider == RCC_HCLK_DIV1) ? pclk : (2U * pclk);
}

static void configure_timers(void)
{
    uint32_t timer_clk = timer_clock_apb1();

    /* TIM6: una interrupcion por muestra, exactamente 16 kHz cuando sea posible. */
    htim6.Instance->PSC = 0;
    htim6.Instance->ARR = (timer_clk / AUDIO_SAMPLE_RATE) - 1U;
    htim6.Instance->EGR = TIM_EGR_UG;

    /* TIM3: portadora PWM cercana a 100 kHz. */
    uint32_t pwm_period = timer_clk / 100000U;
    if (pwm_period < 256U) pwm_period = 256U;
    htim3.Instance->PSC = 0;
    htim3.Instance->ARR = pwm_period - 1U;
    htim3.Instance->CCR1 = 0;
    htim3.Instance->EGR = TIM_EGR_UG;
}

static void stop_audio(void)
{
    HAL_TIM_Base_Stop_IT(&htim6);
    HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
    HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 2048U);
    HAL_DAC_Stop(&hdac, DAC_CHANNEL_1);
    mode = PLAYER_IDLE;
    sample_index = 0;
}

static void start_audio(PlayerMode requested)
{
    stop_audio();
    mode = requested;
    sample_index = 0;
    finished = 0;

    if (mode == PLAYER_DAC) {
        HAL_DAC_Start(&hdac, DAC_CHANNEL_1);
        HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 2048U);
    } else {
        HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    }
    __HAL_TIM_SET_COUNTER(&htim6, 0);
    HAL_TIM_Base_Start_IT(&htim6);
}

void LAB_Audio_Init(void)
{
    configure_timers();
    HAL_UART_Transmit(&huart2, (uint8_t *)menu, strlen(menu), HAL_MAX_DELAY);
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
}

void LAB_Audio_Task(void)
{
    if (finished) {
        __disable_irq();
        finished = 0;
        __enable_irq();
        stop_audio();
        const char done[] = "\r\nAudio finalizado.\r\n";
        HAL_UART_Transmit(&huart2, (uint8_t *)done, strlen(done), HAL_MAX_DELAY);
        HAL_UART_Transmit(&huart2, (uint8_t *)menu, strlen(menu), HAL_MAX_DELAY);
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        if (rx_byte == '1') {
            start_audio(PLAYER_DAC);
        } else if (rx_byte == '2') {
            start_audio(PLAYER_PWM);
        } else if (rx_byte == 's' || rx_byte == 'S') {
            stop_audio();
        }
        HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
    }
}

static uint8_t amplify_sample(uint8_t sample)
{
    int32_t centered = (int32_t)sample - 128;

    /* Ganancia digital x2 */
    centered *= 2;

    /* Saturación para evitar desbordamiento */
    if (centered > 127) {
        centered = 127;
    }

    if (centered < -128) {
        centered = -128;
    }

    return (uint8_t)(centered + 128);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM6 || mode == PLAYER_IDLE) return;

    if (mode == PLAYER_DAC) {
        if (sample_index >= AUDIO_DAC_LEN) {
            HAL_TIM_Base_Stop_IT(&htim6);
            finished = 1;
            return;
        }
        uint8_t sample = audio_dac[sample_index++];
        uint8_t amplified = amplify_sample(sample);
        uint32_t value12 = ((uint32_t)amplified) << 4;
        HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, value12);
    } else {
        if (sample_index >= AUDIO_PWM_LEN) {
            HAL_TIM_Base_Stop_IT(&htim6);
            finished = 1;
            return;
        }
        uint32_t period = __HAL_TIM_GET_AUTORELOAD(&htim3) + 1U;
        uint8_t sample = audio_pwm[sample_index++];
        uint8_t amplified = amplify_sample(sample);
        uint32_t duty = ((uint32_t)amplified * period) / 255U;
        __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, duty);
    }
}
