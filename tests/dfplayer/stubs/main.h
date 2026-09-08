#ifndef DFPLAYER_TEST_MAIN_H
#define DFPLAYER_TEST_MAIN_H

#include <stdint.h>

typedef struct
{
  volatile uint32_t SR;
  volatile uint32_t DR;
  volatile uint32_t BRR;
  volatile uint32_t CR1;
  volatile uint32_t CR2;
  volatile uint32_t CR3;
} USART_TypeDef;

typedef struct
{
  uint32_t unused;
} GPIO_TypeDef;

typedef struct
{
  uint32_t Pin;
  uint32_t Mode;
  uint32_t Pull;
  uint32_t Speed;
} GPIO_InitTypeDef;

extern USART_TypeDef fake_uart4;
extern GPIO_TypeDef fake_gpioc;
extern uint32_t fake_hal_tick;

#define UART4 (&fake_uart4)
#define GPIOC (&fake_gpioc)
#define UART4_IRQn 52

#define GPIO_PIN_10            (1UL << 10)
#define GPIO_PIN_11            (1UL << 11)
#define GPIO_MODE_AF_PP        2U
#define GPIO_MODE_INPUT        0U
#define GPIO_NOPULL            0U
#define GPIO_PULLUP            1U
#define GPIO_SPEED_FREQ_HIGH   3U

#define USART_CR1_RE           (1UL << 2)
#define USART_CR1_TE           (1UL << 3)
#define USART_CR1_RXNEIE       (1UL << 5)
#define USART_CR1_TXEIE        (1UL << 7)
#define USART_CR1_UE           (1UL << 13)
#define USART_SR_PE            (1UL << 0)
#define USART_SR_FE            (1UL << 1)
#define USART_SR_NE            (1UL << 2)
#define USART_SR_ORE           (1UL << 3)
#define USART_SR_RXNE          (1UL << 5)
#define USART_SR_TXE           (1UL << 7)

#define __HAL_RCC_GPIOC_CLK_ENABLE() ((void)0)
#define __HAL_RCC_UART4_CLK_ENABLE() ((void)0)
#define __DMB() ((void)0)

#define HAL_NVIC_DisableIRQ(irq) ((void)(irq))
#define HAL_NVIC_EnableIRQ(irq) ((void)(irq))
#define HAL_NVIC_ClearPendingIRQ(irq) ((void)(irq))
#define HAL_NVIC_SetPriority(irq, preempt, sub) \
  do { (void)(irq); (void)(preempt); (void)(sub); } while (0)

void HAL_GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *config);
uint32_t HAL_RCC_GetPCLK1Freq(void);
uint32_t HAL_GetTick(void);

#endif /* DFPLAYER_TEST_MAIN_H */
