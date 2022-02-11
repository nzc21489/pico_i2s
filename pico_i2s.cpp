/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 nzc21489
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#pragma GCC optimize("O2")

#include "pico_i2s.pio.h"
#include "pico_i2s_24bit.pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"
#include "hardware/resets.h"
#include "hardware/pio.h"
#include <cstring>

PIO pio_i2s = pio0;
uint sm_i2s;
volatile uint8_t i2s_buff_num = 0;
uint8_t dma_channel_i2s[2];
dma_channel_config dma_config_i2s[2];
uint offset_i2s;
volatile int int_count_i2s = 0;
uint32_t i2s_buff_size = (4096 * 4);
int16_t **i2s_buff = NULL;
const pio_program_t *i2s_pio_program;
volatile int i2s_buff_count = 1;

int audio_bit = 16;

void __isr __time_critical_func(dma_handler_i2s0)()
{
    if (dma_irqn_get_channel_status(0, dma_channel_i2s[0]))
    {
        dma_irqn_acknowledge_channel(0, dma_channel_i2s[0]);

        if (int_count_i2s >= i2s_buff_count)
        {
            memset(&i2s_buff[1][0], 0, (i2s_buff_size + 4) * 2);
        }

        dma_channel_set_read_addr(dma_channel_i2s[0], &i2s_buff[0][0], false);
        int_count_i2s++;
    }
}

void __isr __time_critical_func(dma_handler_i2s1)()
{
    if (dma_irqn_get_channel_status(1, dma_channel_i2s[1]))
    {
        dma_irqn_acknowledge_channel(1, dma_channel_i2s[1]);

        if (int_count_i2s >= i2s_buff_count)
        {
            memset(&i2s_buff[0][0], 0, (i2s_buff_size + 4) * 2);
        }

        dma_channel_set_read_addr(dma_channel_i2s[1], &i2s_buff[1][0], false);
        int_count_i2s++;
    }
}

void init_i2s(int bps)
{
    audio_bit = bps;
    if (!i2s_buff)
    {
        i2s_buff = new int16_t *[2];
        i2s_buff[0] = new int16_t[i2s_buff_size + 4];
        i2s_buff[1] = new int16_t[i2s_buff_size + 4];

        memset(&i2s_buff[0][0], 0, (i2s_buff_size + 4) * 2);
        memset(&i2s_buff[1][0], 0, (i2s_buff_size + 4) * 2);
    }

    pio_gpio_init(pio_i2s, PICO_AUDIO_I2S_CLOCK_PIN_BASE);
    pio_gpio_init(pio_i2s, PICO_AUDIO_I2S_CLOCK_PIN_BASE + 1);
    pio_gpio_init(pio_i2s, PICO_AUDIO_I2S_DATA_PIN);

    sm_i2s = pio_claim_unused_sm(pio_i2s, true);

    i2s_pio_program = &audio_i2s_program;
    if (bps == 16)
    {
        i2s_pio_program = &audio_i2s_program;
        offset_i2s = pio_add_program(pio_i2s, &audio_i2s_program);
        audio_i2s_program_init(pio_i2s, sm_i2s, offset_i2s, PICO_AUDIO_I2S_DATA_PIN, PICO_AUDIO_I2S_CLOCK_PIN_BASE, 16); //16bit
    }
    else if (bps == 24)
    {
        i2s_pio_program = &audio_i2s_24bit_program;
        offset_i2s = pio_add_program(pio_i2s, &audio_i2s_24bit_program);
        audio_i2s_24bit_program_init(pio_i2s, sm_i2s, offset_i2s, PICO_AUDIO_I2S_DATA_PIN, PICO_AUDIO_I2S_CLOCK_PIN_BASE, 24); //24bit
    }
    else
    {
        i2s_pio_program = &audio_i2s_program;
        offset_i2s = pio_add_program(pio_i2s, &audio_i2s_program);
        audio_i2s_program_init(pio_i2s, sm_i2s, offset_i2s, PICO_AUDIO_I2S_DATA_PIN, PICO_AUDIO_I2S_CLOCK_PIN_BASE, 32); // 24/32bit
    }

    dma_channel_i2s[0] = dma_claim_unused_channel(true);
    dma_channel_i2s[1] = dma_claim_unused_channel(true);

    // Channel 0
    {
        dma_config_i2s[0] = dma_channel_get_default_config(dma_channel_i2s[0]);
        if (bps == 32)
        {
            channel_config_set_transfer_data_size(&dma_config_i2s[0], DMA_SIZE_32);
        }
        else if (bps == 24)
        {
            channel_config_set_transfer_data_size(&dma_config_i2s[0], DMA_SIZE_8);
        }
        else
        {
            channel_config_set_transfer_data_size(&dma_config_i2s[0], DMA_SIZE_16);
        }
        channel_config_set_read_increment(&dma_config_i2s[0], true);
        channel_config_set_write_increment(&dma_config_i2s[0], false);

        channel_config_set_dreq(&dma_config_i2s[0], pio_get_dreq(pio_i2s, sm_i2s, true));
        channel_config_set_chain_to(&dma_config_i2s[0], dma_channel_i2s[1]);
        dma_channel_set_irq0_enabled(dma_channel_i2s[0], true);
        irq_add_shared_handler(DMA_IRQ_0, dma_handler_i2s0, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
        irq_set_enabled(DMA_IRQ_0, true);

        if (bps == 32)
        {
            dma_channel_configure(dma_channel_i2s[0], &dma_config_i2s[0],
                                  &pio_i2s->txf[sm_i2s], // dst
                                  &i2s_buff[0][0],       // src
                                  i2s_buff_size / 2,     // transfer count
                                  false                  // start immediately
            );
        }
        else if (bps == 24)
        {
            dma_channel_configure(dma_channel_i2s[0], &dma_config_i2s[0],
                                  &pio_i2s->txf[sm_i2s], // dst
                                  &i2s_buff[0][0],       // src
                                  i2s_buff_size * 2,     // transfer count
                                  false                  // start immediately
            );
        }
        else
        {
            dma_channel_configure(dma_channel_i2s[0], &dma_config_i2s[0],
                                  &pio_i2s->txf[sm_i2s], // dst
                                  &i2s_buff[0][0],       // src
                                  i2s_buff_size,         // transfer count
                                  false                  // start immediately
            );
        }
    }

    // Channel 1
    {
        dma_config_i2s[1] = dma_channel_get_default_config(dma_channel_i2s[1]);
        if (bps == 32)
        {
            channel_config_set_transfer_data_size(&dma_config_i2s[1], DMA_SIZE_32);
        }
        else if (bps == 24)
        {
            channel_config_set_transfer_data_size(&dma_config_i2s[1], DMA_SIZE_8);
        }
        else
        {
            channel_config_set_transfer_data_size(&dma_config_i2s[1], DMA_SIZE_16);
        }
        channel_config_set_read_increment(&dma_config_i2s[1], true);
        channel_config_set_write_increment(&dma_config_i2s[1], false);

        channel_config_set_dreq(&dma_config_i2s[1], pio_get_dreq(pio_i2s, sm_i2s, true));
        channel_config_set_chain_to(&dma_config_i2s[1], dma_channel_i2s[0]);
        dma_channel_set_irq1_enabled(dma_channel_i2s[1], true);
        irq_add_shared_handler(DMA_IRQ_1, dma_handler_i2s1, PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
        irq_set_enabled(DMA_IRQ_1, true);

        if (bps == 32)
        {
            dma_channel_configure(dma_channel_i2s[1], &dma_config_i2s[1],
                                  &pio_i2s->txf[sm_i2s], // dst
                                  &i2s_buff[1][0],       // src
                                  i2s_buff_size / 2,     // transfer count
                                  false                  // start immediately
            );
        }
        else if (bps == 24)
        {
            dma_channel_configure(dma_channel_i2s[1], &dma_config_i2s[1],
                                  &pio_i2s->txf[sm_i2s], // dst
                                  &i2s_buff[1][0],       // src
                                  i2s_buff_size * 2,     // transfer count
                                  false                  // start immediately
            );
        }
        else
        {
            dma_channel_configure(dma_channel_i2s[1], &dma_config_i2s[1],
                                  &pio_i2s->txf[sm_i2s], // dst
                                  &i2s_buff[1][0],       // src
                                  i2s_buff_size,         // transfer count
                                  false                  // start immediately
            );
        }
    }
}

void deinit_i2s()
{
    dma_channel_abort(dma_channel_i2s[0]);
    dma_channel_abort(dma_channel_i2s[1]);
    pio_sm_set_enabled(pio_i2s, sm_i2s, false);
    pio_sm_clear_fifos(pio_i2s, sm_i2s);
    pio_sm_restart(pio_i2s, sm_i2s);

    pio_remove_program(pio_i2s, i2s_pio_program, offset_i2s);
    pio_sm_unclaim(pio_i2s, sm_i2s);
    dma_channel_unclaim(dma_channel_i2s[0]);
    dma_channel_unclaim(dma_channel_i2s[1]);
    irq_remove_handler(DMA_IRQ_0, dma_handler_i2s0);
    irq_remove_handler(DMA_IRQ_1, dma_handler_i2s1);
    irq_set_enabled(DMA_IRQ_0, false);
    irq_set_enabled(DMA_IRQ_1, false);

    if (dma_irqn_get_channel_status(0, dma_channel_i2s[0]))
    {
        dma_irqn_acknowledge_channel(0, dma_channel_i2s[0]);
    }
    if (dma_irqn_get_channel_status(1, dma_channel_i2s[1]))
    {
        dma_irqn_acknowledge_channel(1, dma_channel_i2s[1]);
    }

    // reset DMA
    reset_block(RESETS_RESET_DMA_BITS);
    unreset_block_wait(RESETS_RESET_DMA_BITS);

    int_count_i2s = 0;
    i2s_buff_count = 1;
    if (i2s_buff)
    {
        delete[] i2s_buff[0];
        delete[] i2s_buff[1];
        delete[] i2s_buff;
    }
    i2s_buff = NULL;
}

void i2s_start()
{
    pio_sm_clear_fifos(pio_i2s, sm_i2s);
    dma_channel_start(dma_channel_i2s[0]);
    pio_sm_set_enabled(pio_i2s, sm_i2s, true);
}