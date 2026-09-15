/*******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for the T2G MCU P-DMA 1D memory to 
* peripheral transfer code example for ModusToolbox.
*
* Related Document: See README.md
*
*******************************************************************************
* (c) 2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
*
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
*******************************************************************************/

/*******************************************************************************
* Header Files
*******************************************************************************/

#include "cybsp.h"
#include "cy_pdl.h"
#include "cy_retarget_io.h"
#include <string.h>

/******************************************************************************
* Macros
*******************************************************************************/
#define BUFFER_SIZE       (16ul)

/* PDMA software trigger output */
#define PDMA_SW_TRIG     (TRIG_OUT_MUX_0_PDMA0_TR_IN0)

/* PDMA interrupt source */
#define PDMA_INTR        (PDMA_IRQ)

/* Interrupt priorities */
#define PDMA_INTR_PRIORITY   (6u)
#define GPIO_INTR_PRIORITY   (7u)

/* Delay in ms for polling */
#define DELAY_MS         (1u)

/******************************************************************************
*Global variables
*******************************************************************************/
/* Flag set when DMA transfer completes */
static volatile bool g_dmaComplete = false;

/* Flag set when user button is pressed */
static volatile bool g_buttonPressed = false;

/* For Body high and cluster devices MTB-HAL (COMPONENT_MTB_HAL) is used for cy_retarget_io_init() function initialization */
#if defined (CY_DEVICE_TVIIBH8M) || defined (CY_DEVICE_TVIIBH4M) || defined (CY_DEVICE_TVIIBH16M) || defined (CY_DEVICE_TVIIC2D6M) || defined (CY_DEVICE_TVIIC2D4M)
/* For the Retarget-IO (Debug UART) usage */
static cy_stc_scb_uart_context_t    UART_context;
static mtb_hal_uart_t               UART_hal_obj;
#endif

/* Source buffer in SRAM */
uint8_t  UART_TX_Buffer[BUFFER_SIZE] = { 0ul };

/*******************************************************************************
* Private Variables/Constants
*********************************************************************************/
/* P-DMA interrupt configuration */
static const cy_stc_sysint_t PDMA_INT_CFG =
{
    .intrSrc = (cy_sysint_int_src_t)((NvicMux4_IRQn << CY_SYSINT_INTRSRC_MUXIRQ_SHIFT) | PDMA_IRQ ),
    .intrPriority = PDMA_INTR_PRIORITY
};

/* Button GPIO Interrupt configuration structure */
static const cy_stc_sysint_t btnIrqCfg =
{
    .intrSrc = (cy_sysint_int_src_t)((NvicMux3_IRQn << CY_SYSINT_INTRSRC_MUXIRQ_SHIFT) | CYBSP_USER_BTN_IRQ),
    .intrPriority = GPIO_INTR_PRIORITY
};

/*******************************************************************************
* Function Prototypes
*********************************************************************************/
static void handle_PDMA_Interrupt(void);
static void HandleGPIOIntr(void);
static void init_source_buffer(void);
static void print_buffer(const char *name, const uint8_t *buffer, uint32_t count);

/*******************************************************************************
* Function Definitions
*******************************************************************************/


/*******************************************************************************
* Function Name: HandleDMACIntr
********************************************************************************
* Summary:
* P-DMA completion interrupt handler. Sets the transfer-complete flag.
* 
* Parameters:
*  none
*
* Return:
*  none
*
*******************************************************************************/
static void handle_PDMA_Interrupt(void)
{
    uint32_t masked;

    masked = Cy_DMA_Channel_GetInterruptStatusMasked(PDMA_HW, PDMA_CHANNEL);
    if ((masked & CY_DMA_INTR_MASK) != 0UL)
    {
        Cy_DMA_Channel_ClearInterrupt(PDMA_HW, PDMA_CHANNEL);
        g_dmaComplete = true;
    }
}

/*******************************************************************************
* Function Name: HandleGPIOIntr
********************************************************************************
* Summary:
* User button interrupt handler. Sets the button-pressed flag.
* 
* Parameters:
*  none
*
* Return:
*  none
*
*******************************************************************************/
static void HandleGPIOIntr(void)
{
    Cy_GPIO_ClearInterrupt(CYBSP_USER_BTN_PORT, CYBSP_USER_BTN_NUM);
    g_buttonPressed = true;
}

/*******************************************************************************
* Function Name: init_source_buffer
********************************************************************************
* Summary:
* Fills source buffer with incrementing pattern (0x30, 0x31, 0x32, ...).
*
* Parameters:
*  none
*
* Return:
*  none
*
*******************************************************************************/
static void init_source_buffer(void)
{
    for (uint32_t i = 0u; i < BUFFER_SIZE; i++)
    {
        UART_TX_Buffer[i] = (i + 48ul);
    }
}

/*******************************************************************************
* Function Name: print_buffer
********************************************************************************
* Summary:
*  Prints buffer contents in hex format to the serial terminal.
*
* Parameters:
*  name - Name of the buffer
*  buffer - Pointer to the buffer
*  count - Number of elements to print
*
* Return:
*  none
*
*******************************************************************************/
static void print_buffer(const char *name, const uint8_t *buffer, uint32_t count)
{
    printf("%s: ", name);
    for (uint32_t i = 0u; i < count; i++)
    {
        printf("0x%02X ", buffer[i]);
        if (((i + 1u) % 4u) == 0u)
        {
            printf("\r\n");
            if (i < (count - 1u))
            {
                printf("                   ");
            }
        }
    }
    printf("\r\n");
}

/*******************************************************************************
* Function Name: main
*********************************************************************************
* Summary:
* This is the main function. It configures P-DMA for 1D transfer of elements from 
* memory to peripheral, using button interrupt to trigger the P-DMA 
* transfer and results are printed on the serial terminal.
*
* Parameters:
*  none
*
* Return:
*  int
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;
    uint32_t transferCount = 0u;

    /* Initialize the device and board peripherals */
    result = cybsp_init();
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    #if defined (CY_DEVICE_TVIIBH8M) || defined (CY_DEVICE_TVIIBH4M) || defined (CY_DEVICE_TVIIBH16M) || defined (CY_DEVICE_TVIIC2D6M) || defined (CY_DEVICE_TVIIC2D4M)
    /* Disable caches for DMA coherency */   
    SCB_DisableICache();
    SCB_DisableDCache();

    /* Debug UART init */
    result = (cy_rslt_t)Cy_SCB_UART_Init(UART_HW, &UART_config, &UART_context);

    /* UART init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    Cy_SCB_UART_Enable(UART_HW);
    
    /* Setup the HAL UART */
    result = mtb_hal_uart_setup(&UART_hal_obj, &UART_hal_config, &UART_context, NULL);

    /* HAL UART init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    result = cy_retarget_io_init(&UART_hal_obj);

    /* HAL retarget_io init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }
    #endif

    /* For body entry CYT2BL series, CY-HAL (CY_USING_HAL) is used for cy_retarget_io_init() function initialization*/
    #if defined CY_DEVICE_TVIIBE
    cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, CY_RETARGET_IO_BAUDRATE);
    #endif

    /* Configure GPIO interrupt */
    Cy_SysInt_Init(&btnIrqCfg, &HandleGPIOIntr);
    NVIC_ClearPendingIRQ((IRQn_Type)NvicMux3_IRQn);
    NVIC_EnableIRQ(NvicMux3_IRQn);

    /* Initialize Source Buffer */
    init_source_buffer();

    if (Cy_DMA_Descriptor_Init(&PDMA_Descriptor_0, &PDMA_Descriptor_0_config) != CY_DMA_SUCCESS)
    {
        CY_ASSERT(0);
    }
    if (Cy_DMA_Channel_Init(PDMA_HW, PDMA_CHANNEL, &PDMA_channelConfig) != CY_DMA_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Configure descriptor: source and destination addresses */
    Cy_DMA_Descriptor_SetSrcAddress(&PDMA_Descriptor_0, (uint8_t *) UART_TX_Buffer);
    Cy_DMA_Descriptor_SetDstAddress(&PDMA_Descriptor_0, (uint32_t *) &SCB0->TX_FIFO_WR );

    /* Initialize and enable interrupt from PDMA */
    Cy_SysInt_Init(&PDMA_INT_CFG, &handle_PDMA_Interrupt);
    NVIC_EnableIRQ((IRQn_Type) NvicMux4_IRQn);

    /* Set channel priority and enable completion interrupt */
    Cy_DMA_Channel_SetPriority(PDMA_HW, PDMA_CHANNEL, 0UL);
    Cy_DMA_Channel_SetInterruptMask(PDMA_HW, PDMA_CHANNEL, CY_DMA_INTR_MASK);

    /* Enable the P-DMA block */
    Cy_DMA_Enable(DW0);

    printf("\x1b[2J\x1b[;H");
    printf("************************************************************\r\n");
    printf("P-DMA 1D memory to peripheral transfer code example\r\n");
    printf("************************************************************\r\n\n");
    printf(">> Press USER_BTN1 to trigger a P-DMA 1D memory to peripheral transfer \r\n\n");

    for (;;)
    {
        /* Wait for button press */
        g_buttonPressed = false;
        while (!g_buttonPressed)
        {
            Cy_SysLib_Delay(DELAY_MS);
        }

        transferCount++;
        printf("[Transfer #%lu] Starting P-DMA 1D memory to peripheral transfer \r\n\n", (unsigned long)transferCount);
        printf("Sending data from UART TX Buffer \r\n\n "); 
                
        /* Configure channel descriptor and enable the channel */
        Cy_DMA_Channel_SetDescriptor(PDMA_HW, PDMA_CHANNEL, &PDMA_Descriptor_0);
        Cy_DMA_Channel_Enable(PDMA_HW, PDMA_CHANNEL);

        /* Clear completion flag */
        g_dmaComplete = false;

        /* Trigger the DMA transfer via software */
        if (Cy_TrigMux_SwTrigger(PDMA_SW_TRIG, CY_TRIGGER_TWO_CYCLES) != CY_TRIGMUX_SUCCESS)
        {
            printf("[ERROR] SW Trigger failed!\r\n");
            CY_ASSERT(0);
        }

        /* Wait for DMA transfer completion */
        while (!g_dmaComplete)
        {
            Cy_SysLib_Delay(DELAY_MS);
        }
        
        print_buffer("Data transferred", UART_TX_Buffer, BUFFER_SIZE);
        printf("P-DMA 1D memory to peripheral transfer complete \r\n\n");
        printf("--------------------------------------------------------------\r\n");
        printf("\r\n");
        printf(">> Press USER_BTN1 for next transfer...\r\n");
        printf("\r\n");
        
}
}

/* [] END OF FILE */
