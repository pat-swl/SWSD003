/*!
 * @file      main_sw_tx_test_tool.c
 *
 * @brief     Somewear Labs TX testing utility for LR1121 Nucleo Shields
 *
 * The Clear BSD License
 * Copyright Semtech Corporation 2022. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the disclaimer
 * below) provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Semtech corporation nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
 * NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SEMTECH CORPORATION BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * -----------------------------------------------------------------------------
 * --- DEPENDENCIES ------------------------------------------------------------
 */

#include <stdio.h>
#include <string.h>

#include "apps_common.h"
#include "apps_utilities.h"
#include "lr11xx_radio.h"
#include "smtc_hal_dbg_trace.h"
#include "uart_init.h"

#include "sx126x.h"
#include "lr11xx_radio.h"
#include "lr11xx_system.h"
#include "lr11xx_system_types.h"
#include "lr11xx_regmem.h"

#include "main_sw_tx_test_tool.h"

#define EMBEDDED_CLI_IMPL
#include "embedded_cli.h"

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE MACROS-----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE CONSTANTS -------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE TYPES -----------------------------------------------------------
 */

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE VARIABLES -------------------------------------------------------
 */

// CLI Setup
EmbeddedCliConfig *config;
EmbeddedCli *cli;

static lr11xx_hal_context_t* lr_ctx;

// Modem Config Variables
int tx_freq_hz = RF_FREQ_IN_HZ;
sx126x_lora_bw_t tx_bw = LORA_BANDWIDTH;
int tx_pwr_dbm = TX_OUTPUT_POWER_DBM;
sx126x_lora_sf_t tx_sf = LORA_SPREADING_FACTOR;
sx126x_lora_cr_t tx_coding = LORA_CODING_RATE;

// TX Variables
bool transmitting = false;
const uint8_t tx_buf[TX_MSG_LEN] = "TESTMSG";
int tx_msg_count = 0;

/*
 * -----------------------------------------------------------------------------
 * --- PRIVATE FUNCTIONS DECLARATION -------------------------------------------
 */

/// @brief Handler for sending characters from the CLI to the serial port
/// @param embeddedCli 
/// @param c 
void cliWriteChar(EmbeddedCli *embeddedCli, char c)
{
    hal_mcu_trace_print("%c", c);
}

/// @brief Handle uart character from the debug serial port
/// @param data 
void handle_uart(uint8_t data)
{
    embeddedCliReceiveChar(cli, (char)data);
}

/// @brief Returns a bandwidth in Hz for the given LoRa BW enum value
/// @param bw 
/// @return 
int get_lora_bw_hz(sx126x_lora_bw_t bw)
{
    switch (bw)
    {
        case SX126X_LORA_BW_007:
            return 7000;
        case SX126X_LORA_BW_010:
            return 10000;
        case SX126X_LORA_BW_015:
            return 15000;
        case SX126X_LORA_BW_020:
            return 20000;
        case SX126X_LORA_BW_031:
            return 31000;
        case SX126X_LORA_BW_041:
            return 41000;
        case SX126X_LORA_BW_062:
            return 62000;
        case SX126X_LORA_BW_125:
            return 125000;
        case SX126X_LORA_BW_250:
            return 250000;
        case SX126X_LORA_BW_500:
            return 500000;
        default:
            return 0;
    }
}

void print_tx_cfg()
{
    // Query current config fromn the LR1121
    
    // Print
    HAL_DBG_TRACE_INFO("Current TX config\n");
    HAL_DBG_TRACE_INFO("    Frequency: %d Hz\n", tx_freq_hz);
    HAL_DBG_TRACE_INFO("    Bandiwdth: %d Hz\n", get_lora_bw_hz(tx_bw));
    HAL_DBG_TRACE_INFO("        Power: %d dbm\n", tx_pwr_dbm);
    HAL_DBG_TRACE_INFO("           SF: %d\n", tx_sf);
    HAL_DBG_TRACE_INFO("       Coding: %d\n", tx_coding);
}

/*
 * -----------------------------------------------------------------------------
 * --- CLI HANDLER FUNCTIONS -------------------------------------------
 */
void onSetTxFreq(EmbeddedCli *cli, char *args, void *context)
{
    int tokens = embeddedCliGetTokenCount(args);
    if (tokens == 0 || tokens > 1)
    {
        HAL_DBG_TRACE_ERROR("set-tx-freq requires 1 argument (frequency in Hz)\n");
    }
    else
    {
        int freq_hz = atoi(embeddedCliGetToken(args, 1));
        HAL_DBG_TRACE_DEBUG("Setting TX frequency to %d Hz\n", freq_hz);
        if (lr11xx_radio_set_rf_freq(( void* ) lr_ctx, freq_hz) != LR11XX_STATUS_OK)
        {
            HAL_DBG_TRACE_ERROR("Failed to set TX frequency!\n");
        }
        else
        {
            tx_freq_hz = freq_hz;
            print_tx_cfg();
        }
        
    }
}

CliCommandBinding cmdSetTxFreq = {
    "set_tx_freq",
    "Set Transmit Frequency in MHz",
    true,
    NULL,
    onSetTxFreq
};

void onTx(EmbeddedCli *cli, char *args, void *context)
{
    // Get current TX state
    if (transmitting)
    {
        HAL_DBG_TRACE_INFO("Stopping TX\n");
        transmitting = false;
    }
    else
    {
        // Log print
        HAL_DBG_TRACE_INFO("Starting TX\n");
        // Prepare the packet
        if (lr11xx_regmem_write_buffer8(lr_ctx, tx_buf, TX_MSG_LEN) != LR11XX_STATUS_OK)
        {
            HAL_DBG_TRACE_ERROR("Failed to write TX packet to buffer\n");
            return;
        }
        // Setup for TX
        apps_common_lr11xx_handle_pre_tx();
        // Start TX
        if (lr11xx_radio_set_tx(lr_ctx, 0) != LR11XX_STATUS_OK)
        {
            HAL_DBG_TRACE_ERROR("Failed to start TX\n");
            return;
        }
        // Set flag after everything completes
        transmitting = true;
    }
    
}

CliCommandBinding cmdTx = {
    "tx",
    "Start/Stop transmit",
    false,
    NULL,
    onTx
};

void onReset(EmbeddedCli *cli, char *args, void *context)
{
    HAL_DBG_TRACE_WARNING("Resetting LR1121...");
    lr11xx_system_reset(( void* ) lr_ctx);
    HAL_DBG_TRACE_PRINTF("Done!\n");
};

CliCommandBinding cmdReset = {
    "reset",
    "Reset board",
    false,
    NULL,
    onReset,
};

/// @brief Create CLI bindings for embedded CLI
void createCliBindings()
{
    embeddedCliAddBinding(cli, cmdSetTxFreq);
    embeddedCliAddBinding(cli, cmdReset);
    embeddedCliAddBinding(cli, cmdTx);
}

/*
 * -----------------------------------------------------------------------------
 * --- TRANSMIT HANDLER FUNCTIONS -------------------------------------------
 */

/// @brief Handler called when packet TX completes
/// @param  
void on_tx_done(void)
{
    // Increment our counter
    tx_msg_count++;
    // Handle TX done low-level stuff
    apps_common_lr11xx_handle_post_tx();
    // If we're still transmitting, copy the message to the buffer and go again
    if (transmitting)
    {
        // Copy the message
        if (lr11xx_regmem_write_buffer8(lr_ctx, tx_buf, TX_MSG_LEN) != LR11XX_STATUS_OK)
        {
            HAL_DBG_TRACE_ERROR("Failed to write TX packet to buffer\n");
            transmitting = false;
            tx_msg_count = 0;
            return;
        }
        // Setup for TX
        apps_common_lr11xx_handle_pre_tx();
        // Start TX again
        if (lr11xx_radio_set_tx(lr_ctx, 0) != LR11XX_STATUS_OK)
        {
            HAL_DBG_TRACE_ERROR("Failed to start TX\n");
            transmitting = false;
            tx_msg_count = 0;
            return;
        }
    }
    else
    {
        HAL_DBG_TRACE_INFO("Sent %d TX packets\n", tx_msg_count);
        tx_msg_count = 0;
    }
}

/*
 * -----------------------------------------------------------------------------
 * --- PUBLIC FUNCTIONS DEFINITION ---------------------------------------------
 */

/**
 * @brief Main application entry point.
 */
int main( void )
{
    smtc_hal_mcu_init( );
    apps_common_shield_init( );

    // Setup UART
    uart_init_with_rx_callback(handle_uart);

    // Setup CLI
    config = embeddedCliDefaultConfig();
    config->maxBindingCount = 16;
    cli = embeddedCliNew(config);
    cli->writeChar = cliWriteChar;
    createCliBindings();

    // Clear Screen
    HAL_DBG_TRACE_PRINTF("\033[2J\033[H");

    // Startup Prints
    HAL_DBG_TRACE_INFO( "===== LR1121 SW Labs TX Test Tool =====\n" );
    HAL_DBG_TRACE_INFO(" Built on " __DATE__ " " __TIME__ "\n");

    // Print Driver Version
    apps_common_print_sdk_driver_version( );

    // Get LR1121 version
    lr_ctx = apps_common_lr11xx_get_context( );
    apps_common_lr11xx_system_init( ( void* ) lr_ctx );
    apps_common_lr11xx_fetch_and_print_version( ( void* ) lr_ctx );
    apps_common_lr11xx_radio_init( ( void* ) lr_ctx );

    // Setup interrupts
    ASSERT_LR11XX_RC( lr11xx_system_set_dio_irq_params( lr_ctx, IRQ_MASK, 0 ) );
    ASSERT_LR11XX_RC( lr11xx_system_clear_irq_status( lr_ctx, LR11XX_SYSTEM_IRQ_ALL_MASK ) );

    HAL_DBG_TRACE_INFO("Ready!\n");

    //ASSERT_LR11XX_RC( lr11xx_radio_set_tx_infinite_preamble( lr_ctx ) );

    // Reload watchdog counter to avoid reset
    while( 1 )
    {
        apps_common_lr11xx_irq_process(lr_ctx, IRQ_MASK);
        embeddedCliProcess(cli);
    }
}
