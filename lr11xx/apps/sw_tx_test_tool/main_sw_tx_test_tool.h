#ifndef SW_TX_TEST_TOOL_H
#define SW_TX_TEST_TOOL_H

#ifdef __cplusplus
extern "C" {
#endif

#define TX_MSG_LEN  7

#define IRQ_MASK                                                                          \
    ( LR11XX_SYSTEM_IRQ_TX_DONE | LR11XX_SYSTEM_IRQ_RX_DONE | LR11XX_SYSTEM_IRQ_TIMEOUT | \
      LR11XX_SYSTEM_IRQ_HEADER_ERROR | LR11XX_SYSTEM_IRQ_CRC_ERROR | LR11XX_SYSTEM_IRQ_FSK_LEN_ERROR )

#ifdef __cplusplus
}
#endif

#endif