#include <stdio.h>
#include <string.h>

/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#define EEPROM_ADDR          (0x50 << 1)  // EEPROM address
#define DS1307_ADDR          (0X68 << 1)  // RTC address

#define EEPROM_SIZE          4096
#define EEPROM_PAGE_SIZE     32
#define LOG_SIZE             (6 + sizeof(float)*2)

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void read_data_from_SHT31(float *temp, float *humi)
{
    uint8_t cmd[2] = {0x24, 0x00};
    uint8_t data[6];
    uint16_t raw_temp, raw_hum;

    HAL_I2C_Master_Transmit(&hi2c2, 0x88, cmd, 2, 100);
    HAL_Delay(20);

    HAL_I2C_Master_Receive(&hi2c2, 0x88, data, 6, 100);
    HAL_Delay(20);

    raw_temp = (data[0] << 8) | data[1];
    raw_hum  = (data[3] << 8) | data[4];

    *temp = -45.0 + (175.0 * raw_temp / 65535.0);
    *humi = 100.0 * raw_hum / 65535.0;
}

// Write EEPROM
void EEPROM_Write(uint16_t memAddress, uint8_t* data, uint16_t size)
{
    uint16_t bytes_to_write;
    uint16_t page_remaining;

    while (size > 0)
    {
        page_remaining = EEPROM_PAGE_SIZE - (memAddress % EEPROM_PAGE_SIZE);

        if (size < page_remaining)
            bytes_to_write = size;
        else
            bytes_to_write = page_remaining;

        HAL_I2C_Mem_Write(&hi2c1,
                          EEPROM_ADDR,
                          memAddress,
                          I2C_MEMADD_SIZE_16BIT,
                          data,
                          bytes_to_write,
                          100);

        HAL_Delay(5);

        memAddress += bytes_to_write;
        data += bytes_to_write;
        size -= bytes_to_write;
    }
}

// read fun from EEPROM
void EEPROM_Read(uint16_t memAddress, uint8_t* data, uint16_t size)
{
	HAL_I2C_Mem_Read(&hi2c1, EEPROM_ADDR, memAddress, I2C_MEMADD_SIZE_16BIT, data, size, 100);
}

// Save data to EEPROM
void save_log_to_eeprom(uint16_t address,
                        uint8_t hour, uint8_t min, uint8_t sec,
                        uint8_t date, uint8_t month, uint8_t year,
                        float temp, float hum)
{
    uint8_t buffer[LOG_SIZE];

    buffer[0] = hour;
    buffer[1] = min;
    buffer[2] = sec;
    buffer[3] = date;
    buffer[4] = month;
    buffer[5] = year;

    memcpy(&buffer[6], &temp, 4);
    memcpy(&buffer[10], &hum, 4);

    EEPROM_Write(address, buffer, LOG_SIZE);
}

// read data from the EEPROM
void read_log_from_eeprom(uint16_t address,
                         uint8_t *hour, uint8_t *min, uint8_t *sec,
                         uint8_t *date, uint8_t *month, uint8_t *year,
                         float *temp, float *hum)
{
    uint8_t buffer[LOG_SIZE];

    EEPROM_Read(address, buffer, LOG_SIZE);

    *hour  = buffer[0];
    *min   = buffer[1];
    *sec   = buffer[2];
    *date  = buffer[3];
    *month = buffer[4];
    *year  = buffer[5];

    memcpy(temp, &buffer[6], 4);
    memcpy(hum,  &buffer[10], 4);
}

// conversion
uint8_t dec_to_bcd(uint8_t val)
{
    return ((val / 10) << 4) | (val % 10);
}

uint8_t bcd_to_dec(uint8_t val)
{
    return ((val >> 4) * 10) + (val & 0x0F);
}

// set the credentials to RTC => DS13072
void DS13072_Set_Time(uint8_t sec, uint8_t min, uint8_t hour,
		              uint8_t day, uint8_t date, uint8_t month,
					  uint8_t year)
{
	uint8_t data[7] ;

	data[0] = dec_to_bcd(sec);
	data[0] &= 0X7F;              // clear CH bit (7th bit of sec register)
	data[1] = dec_to_bcd(min);
	data[2] = dec_to_bcd(hour);
	data[3] = dec_to_bcd(day);
	data[4] = dec_to_bcd(date);
	data[5] = dec_to_bcd(month);
	data[6] = dec_to_bcd(year);

	HAL_I2C_Mem_Write(&hi2c1, DS1307_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, data, 7, 100);
}

// read the credentials from the RTC
void DS1307_GetTime(uint8_t* sec, uint8_t* min, uint8_t* hour,
		            uint8_t* day, uint8_t* date, uint8_t* month,
					uint8_t* year)
{
	uint8_t data[7];

	HAL_I2C_Mem_Read(&hi2c1, DS1307_ADDR, 0x00, I2C_MEMADD_SIZE_8BIT, data, 7, 100);

	*sec = bcd_to_dec(data[0]);
	*min = bcd_to_dec(data[1]);
	*hour = bcd_to_dec(data[2]);
	*day = bcd_to_dec(data[3]);
	*date = bcd_to_dec(data[4]);
	*month = bcd_to_dec(data[5]);
	*year = bcd_to_dec(data[6]);
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
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_I2C2_Init();
  /* USER CODE BEGIN 2 */
    uint16_t eeprom_address = 0;
    float temperature, humidity;
    uint8_t sec, min, hour, day, date, month, year;
    /* ##################       Set once   ####################### */
    //DS13072_Set_Time(0, 26, 16, 22, 22, 4, 26); // Example
    char log[] = "System Started..!\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t*)log, strlen(log), 1000);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  // 1. Read sensor
	  	      read_data_from_SHT31(&temperature, &humidity);

	  	      // 2. Read RTC
	  	      DS1307_GetTime(&sec, &min, &hour, &day, &date, &month, &year);

	  	      // 3. Save everything
	  	      save_log_to_eeprom(eeprom_address,
	  	                         hour, min, sec,
	  	                         date, month, year,
	  	                         temperature, humidity);

	  //	      HAL_Delay(1000);

	  	      // 4. Read back for verification
	  	      float t, h;
	  	      uint8_t r_sec, r_min, r_hour, r_date, r_month, r_year;

	  	      read_log_from_eeprom(eeprom_address,
	  	                           &r_hour, &r_min, &r_sec,
	  	                           &r_date, &r_month, &r_year,
	  	                           &t, &h);

	  	      // 5. Print log
	  	      char msg[120];
	  	      sprintf(msg,
	  	          "%02d:%02d:%02d %02d/%02d/20%02d -> Temp: %.2f C, Hum: %.2f %%\r\n",
	  	          r_hour, r_min, r_sec,
	  	          r_date, r_month, r_year,
	  	          t, h);

	  	      HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), 1000);

	  	      // 6. Next address
	  	      eeprom_address += LOG_SIZE;

	  	      if (eeprom_address >= 4096)
	  	          eeprom_address = 0;

	  	      HAL_Delay(10000);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 100000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
