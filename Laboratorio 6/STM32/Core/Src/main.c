/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ADC_CENTER       2048U //Definición del valor central del ADC, min:0, máx:4095
#define ADC_DEAD_ZONE     150U //Define una zona muerta de 150 unidades alrededor del centro: 2048+150 & 2048-150
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */
volatile uint32_t adcValues[2];
volatile uint8_t controlByte;
int8_t previousVertical = 2; //Posición vertical
int8_t previousHorizontal = 2; //Posición Horizontal
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */
static void LAB_ADC_DMA_Init(void);
static void LAB_UART_Init(void);
static void Terminal_Print(const char *text);
static void Joystick_Process(void);
static void Control_Process(uint8_t command);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void Terminal_Print(const char *text)
{
  while (*text != '\0')
  {
    while ((USART2->SR & USART_SR_TXE) == 0U)
    {
    }
    USART2->DR = (uint8_t)*text;
    text++;
  }
}

static uint32_t UART_BRR(uint32_t peripheralClock, uint32_t baudRate)
{
  return (peripheralClock + (baudRate / 2U)) / baudRate;
}

static void LAB_UART_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();
  __HAL_RCC_USART2_CLK_ENABLE();

  GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  USART1->BRR = UART_BRR(HAL_RCC_GetPCLK2Freq(), 9600U);
  USART1->CR1 = USART_CR1_RE|USART_CR1_TE|USART_CR1_RXNEIE|USART_CR1_UE;
  USART1->CR2 = 0U;
  USART1->CR3 = 0U;

  USART2->BRR = UART_BRR(HAL_RCC_GetPCLK1Freq(), 115200U);
  USART2->CR1 = USART_CR1_RE|USART_CR1_TE|USART_CR1_UE;
  USART2->CR2 = 0U;
  USART2->CR3 = 0U;

  HAL_NVIC_SetPriority(USART1_IRQn, 1U, 0U);
  HAL_NVIC_EnableIRQ(USART1_IRQn);
}

static void LAB_ADC_DMA_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_DMA2_CLK_ENABLE();
  __HAL_RCC_ADC1_CLK_ENABLE();

  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  DMA2_Stream0->CR &= ~DMA_SxCR_EN;
  while ((DMA2_Stream0->CR & DMA_SxCR_EN) != 0U)
  {
  }
  DMA2->LIFCR = DMA_LIFCR_CFEIF0|DMA_LIFCR_CDMEIF0|DMA_LIFCR_CTEIF0
              |DMA_LIFCR_CHTIF0|DMA_LIFCR_CTCIF0;
  DMA2_Stream0->PAR = (uint32_t)&ADC1->DR;
  DMA2_Stream0->M0AR = (uint32_t)adcValues;
  DMA2_Stream0->NDTR = 2U;
  DMA2_Stream0->CR = DMA_SxCR_MINC|DMA_SxCR_CIRC
                    |DMA_SxCR_MSIZE_1|DMA_SxCR_PSIZE_1;
  DMA2_Stream0->FCR = 0U;
  DMA2_Stream0->CR |= DMA_SxCR_EN;

  ADC->CCR = ADC_CCR_ADCPRE_0;
  ADC1->CR1 = ADC_CR1_SCAN;
  ADC1->CR2 = ADC_CR2_CONT|ADC_CR2_DMA|ADC_CR2_DDS;
  ADC1->SMPR2 = (4U << ADC_SMPR2_SMP0_Pos)|(4U << ADC_SMPR2_SMP1_Pos);
  ADC1->SQR1 = (1U << ADC_SQR1_L_Pos);
  ADC1->SQR3 = (1U << ADC_SQR3_SQ2_Pos);
  ADC1->CR2 |= ADC_CR2_ADON;
  ADC1->CR2 |= ADC_CR2_SWSTART;
}

static void Joystick_Process(void)
{
  int8_t vertical = 0;
  int8_t horizontal = 0;

  //Carga la zona ADC del centro es el case 0, a la versión vertical de la zona muerta
  if (adcValues[0] > (ADC_CENTER + ADC_DEAD_ZONE)) vertical = 1;
  else if (adcValues[0] < (ADC_CENTER - ADC_DEAD_ZONE)) vertical = -1;

  //Carga la zona ADC del centro es el case 1, a la versión horizontal de la zona muerta
  if (adcValues[1] > (ADC_CENTER + ADC_DEAD_ZONE)) horizontal = 1;
  else if (adcValues[1] < (ADC_CENTER - ADC_DEAD_ZONE)) horizontal = -1;

  if (vertical != previousVertical)
  {
	//Si el case "vertical" es mayor a cero imprime "Arriba"
    if (vertical > 0) Terminal_Print("Control 1: Arriba\r\n");
    //Si el case "vertical" es menor a cero imprime "Abajo"
    else if (vertical < 0) Terminal_Print("Control 1: Abajo\r\n");
    previousVertical = vertical;
  }

  if (horizontal != previousHorizontal)
  {
	//Si el case "horizontal" es mayor a cero imprime "Derecha"
    if (horizontal > 0) Terminal_Print("Control 1: Derecha\r\n");
    //Si el case "horizontal" es mayor a cero imprime "Izquierda"
    else if (horizontal < 0) Terminal_Print("Control 1: Izquierda\r\n");
    previousHorizontal = horizontal;
  }
}

static void Control_Process(uint8_t command)
{
  switch (command)
  {
    case 'U': Terminal_Print("Control 2: Arriba\r\n"); break;
    case 'D': Terminal_Print("Control 2: Abajo\r\n"); break;
    case 'R': Terminal_Print("Control 2: Derecha\r\n"); break;
    case 'L': Terminal_Print("Control 2: Izquierda\r\n"); break;
    case 'A': Terminal_Print("Control 2: A\r\n"); break;
    case 'B': Terminal_Print("Control 2: B\r\n"); break;
    default: break;
  }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  /* USER CODE BEGIN 2 */
  LAB_UART_Init();
  LAB_ADC_DMA_Init();
  Terminal_Print("\r\nLaboratorio 6 - STM32F446RE\r\n");
  Terminal_Print("Control de videojuego iniciado\r\n\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    Joystick_Process();
    HAL_Delay(25U);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : USART_TX_Pin USART_RX_Pin */
  GPIO_InitStruct.Pin = USART_TX_Pin|USART_RX_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void USART1_IRQHandler(void)
{
  if ((USART1->SR & USART_SR_RXNE) != 0U)
  {
    uint8_t receivedCharacter = (uint8_t)USART1->DR;

    while ((USART2->SR & USART_SR_TXE) == 0U)
    {
    }

    USART2->DR = receivedCharacter;
  }

  if ((USART1->SR & USART_SR_ORE) != 0U)
  {
    (void)USART1->DR;
  }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
  * @brief Reports the name of the source file and the source line number.
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
