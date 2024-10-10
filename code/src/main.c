#include "main.h"

p_function jump_to_application;
uint32_t jump_address;
uint8_t i;
uint8_t page_buffer[N32L40X_PAGE_SIZE];
uint32_t update_disable = 0xFFFFFFFF;

int main(void)
{
	if ( *((uint32_t *)flash_addr_boot_info) == boot_update_enable_flag)
	{
		log_init();
		
		LOG("start update app!\r\n");
		
		/* read backup app data and write to app page */
		for (i = 0; i < flash_page_update_total; i++)
		{
			if (n32l40x_flash_read_bytes(flash_addr_app_backup + (i*N32L40X_PAGE_SIZE), page_buffer, N32L40X_PAGE_SIZE) != RT_EOK)
			{
				error_handler();
			}
			if (n32l40x_flash_write_bytes(flash_addr_app + (i*N32L40X_PAGE_SIZE), page_buffer, N32L40X_PAGE_SIZE) != RT_EOK)
			{
				error_handler();
			}
			progress_printf(i+1, flash_page_update_total);
		}
		
		LOG("waiting seconds...\r\n");
		
		/* erase all backup page */
		for (i = 0; i < flash_page_update_total; i++)
		{
			if (n32l40x_flash_erase_one_page(flash_page_app_backup + i) != RT_EOK)
			{
				error_handler();
			}
		}
		
		/* reset boot info */
		if (n32l40x_flash_write_bytes(flash_addr_boot_info, (uint8_t *)&update_disable, sizeof(update_disable)) != RT_EOK)
		{
			error_handler();
		}
		
		LOG("update app successful! now run app!\r\n");
		
		__NVIC_SystemReset();
	}
	
	jump_to_app(flash_addr_app);
	
	return 0;
}

/*
error handler
*/
void error_handler(void)
{
	volatile uint32_t delay_cnt;
	
	while (1)
	{
		LOG("bootloader pain!\r\n");
		delay_cnt = 0x000FFFFF;
		while (delay_cnt--);
	}
}


/**
jump to application
*/
void jump_to_app(uint32_t address)
{
    /* Judge whether the top of stack address is legal or not */
    if (((*(__IO uint32_t*)address) & 0x2FFE0000) == 0x20000000)
    {
        /* Jump to user application */
        jump_address = *(__IO uint32_t*) (address + 4);
        jump_to_application = (p_function) jump_address;
			
        /* Initialize user application's Stack Pointer */
        __set_MSP(*(__IO uint32_t*) address);
        jump_to_application();
    }
}


/*
 * erase one page
 **/
int32_t n32l40x_flash_erase_one_page(uint8_t num)
{
	int32_t err = RT_EOK;
	
	FLASH_Unlock();

	if (FLASH_COMPL != FLASH_EraseOnePage(N32L40X_PAGE_INDEX_2_ADDR(num)))
	{
		err = -RT_ERROR;
	}

	FLASH_Lock();
	
	return err;
}

/*
 * read flash
 **/
int32_t n32l40x_flash_read_bytes(uint32_t start_addr, uint8_t *pdata, uint16_t length)
{
	uint8_t *p_addr = (uint8_t *)start_addr;

	if ((start_addr < N32L40X_PAGE_INDEX_2_ADDR(0)) ||
		((start_addr + length - 1) > (N32L40X_PAGE_INDEX_2_ADDR(N32L40X_PAGE_TOTAL)-1)))
	{
		LOG("err! read flash addr is overflow! addr:%x leng:%x\r\n", start_addr, length);
		return -RT_ERROR;
	}

	while (length > 0)
	{
		*pdata = *(__IO uint8_t*)(p_addr);
		pdata++;
		p_addr++;
		length--;
	}

	return RT_EOK;
}


/*
 * write flash
 **/
static uint8_t flash_buffer[N32L40X_PAGE_SIZE] = {0};
int32_t n32l40x_flash_write_bytes(uint32_t start_addr, uint8_t *pdata, uint16_t length)
{
	uint8_t *p_buffer = RT_NULL;
	uint32_t page_addr_start, counter;
	uint32_t offset;
	uint8_t *p_temp;
	
	/* step1 judge address overflow? */
	if ((start_addr < N32L40X_PAGE_INDEX_2_ADDR(0)) ||
		((start_addr + length - 1) > (N32L40X_PAGE_INDEX_2_ADDR(N32L40X_PAGE_TOTAL)-1)))
	{
		LOG("err! write flash addr is overflow! addr:%x leng:%x\r\n", start_addr, length);
		return -RT_ERROR;
	}


	/* step2 page buffer 2KB */
	p_buffer = flash_buffer;

	/* step3 write */
	while (length)
	{
		memset((void *)p_buffer, 0, N32L40X_PAGE_SIZE);
		
		/* step3.1 get start addr */
		page_addr_start = N32L40X_PAGE_INDEX_2_ADDR(0) + ((start_addr - N32L40X_PAGE_INDEX_2_ADDR(0))/N32L40X_PAGE_SIZE)*N32L40X_PAGE_SIZE;

		/* step3.2 read page data that will be erase */
		if (n32l40x_flash_read_bytes(page_addr_start, p_buffer, N32L40X_PAGE_SIZE) != RT_EOK)
		{
			LOG("err! write flash failed: cannot read!\r\n");
			goto flag_free_err;
		}

		/* step3.3 buffer */
		offset = start_addr - page_addr_start;
		while ((offset < N32L40X_PAGE_SIZE) && (length > 0))
		{
			p_buffer[offset++] = *pdata++;
			length--;
		}

		/* step3.4 init flash clk */
		if(FLASH_HSICLOCK_DISABLE == FLASH_ClockInit())
		{
			LOG("err! write flash failed HSI not yet ready\r\n");
			goto flag_free_err;
		}

		/* step3.5 Unlocks the FLASH Program Erase Controller */
		FLASH_Unlock();

		/* step3.6 Erase */
		if (FLASH_COMPL != FLASH_EraseOnePage(page_addr_start))
		{
			LOG("Flash EraseOnePage Error. Please Deal With This Error Promptly\r\n");
			goto flag_lock_err;
		}

		/* step3.7 Program */
		p_temp = p_buffer;
		for (counter = 0; counter < N32L40X_PAGE_SIZE; counter += 4)
		{
			if (FLASH_COMPL != FLASH_ProgramWord(page_addr_start + counter, *((uint32_t *)p_temp)))
			{		
				LOG("Flash ProgramWord Error.\r\n");
				goto flag_lock_err;
			}
			p_temp += 4;
		}

		/* step3.8 Locks the FLASH Program Erase Controller */
		FLASH_Lock();

		/* step3.9 Check */
		p_temp = p_buffer;
		for (counter = 0; counter < N32L40X_PAGE_SIZE; counter += 4)
		{
			if (*((uint32_t *)p_temp) != (*(__IO uint32_t*)(page_addr_start + counter)))
			{
				LOG("Flash Program Test Failed\r\n");
				goto flag_free_err;
			}
			p_temp += 4;
		}

		/* step3.10 next round */		
		/* offset address */
		start_addr = page_addr_start + N32L40X_PAGE_SIZE;
	}
	
	return RT_EOK;

flag_lock_err:
	/* flash lock */
	FLASH_Lock();
flag_free_err:
	/* free */
	return -RT_ERROR;
}


/*
	log init
*/
void log_init(void)
{
	GPIO_InitType gpio_init;
	USART_InitType usart_init;

	/* step 1 enable gpio tx clk */
	LOG_TX_GPIO_CLK_ENABLE();

	/* step 2 enable uart clk */
	LOG_UART_CLK_ENABLE();

	/* step 3 gpio config */
	/* Initialize gpio_init */
	GPIO_InitStruct(&gpio_init);
	/* Configure USARTy Tx as alternate function push-pull */
	gpio_init.Pin            = LOG_TX_GPIO_PIN;    
	gpio_init.GPIO_Mode      = GPIO_Mode_AF_PP;
	gpio_init.GPIO_Alternate = LOG_TX_GPIO_AF;
	GPIO_InitPeripheral(LOG_TX_GPIO_PORT, &gpio_init);

	/* step 4 uart config */
	USART_StructInit(&usart_init);
	usart_init.BaudRate            = LOG_UART_BAUDRATE;
	usart_init.WordLength          = USART_WL_8B;
	usart_init.StopBits            = USART_STPB_1;
	usart_init.Parity              = USART_PE_NO;
	usart_init.HardwareFlowControl = USART_HFCTRL_NONE;
	usart_init.Mode                = USART_MODE_TX;
	USART_Init(LOG_UART, &usart_init);

	/* step 4.1 uart enable */
	USART_Enable(LOG_UART, ENABLE);
}


void progress_printf(uint8_t now, uint8_t total)
{
	if (total >= now)
	{
		total -= now;
	}
	else
	{
		total = 0;
	}
	
	LOG("\r\n");
	LOG("updating:");
	while (now--)
	{
		LOG(">");
	}
	
	while (total--)
	{
		LOG("*");
	}
	LOG("\r\n");
}

int fputc(int ch, FILE* f)
{
    USART_SendData(LOG_UART, (uint8_t)ch);
    while (USART_GetFlagStatus(LOG_UART, USART_FLAG_TXDE) == RESET);

    return (ch);
}

