#ifndef LCD_QUEUE_LOAD_TP_FW_H
#define LCD_QUEUE_LOAD_TP_FW_H

#ifdef BUILD_LK
/* Stub for the kernel function `lcd_queue_load_tp_fw()`.
 * In the original kernel this loads the in‑cell touch‑panel firmware.
 * The LK does not need to actually load anything – the hardware is already
 * powered and the driver will handle the firmware later – but we provide a
 * no‑op implementation to satisfy the linker.
 */
int lcd_queue_load_tp_fw(void);
#endif

#endif /* LCD_QUEUE_LOAD_TP_FW_H */