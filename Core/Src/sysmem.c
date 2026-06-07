/**
 * @file sysmem.c
 * @brief STM32 Minimal System Memory Management
 */

#include <errno.h>
#include <stdint.h>

/**
 * Pointer to the current end of the heap. 
 */
static uint8_t *__sbrk_heap_end = NULL;

/**
 * @brief _sbrk() allocates memory to the newlib heap and is used by malloc
 *        and others from the C library
 *
 * @param incr The number of bytes to increment the stack by.
 * @return A pointer to the start of the new memory, or -1 on failure.
 */
void *_sbrk(ptrdiff_t incr)
{
  extern uint8_t _end; /* Symbol defined in the linker script */
  extern uint8_t _estack; /* Symbol defined in the linker script */
  extern uint32_t _Min_Stack_Size; /* Symbol defined in the linker script */
  const uint8_t *max_heap = (uint8_t *)((uint32_t)&_estack - (uint32_t)&_Min_Stack_Size);
  uint8_t *prev_heap_end;

  /* Initialize heap end if necessary */
  if (__sbrk_heap_end == NULL)
  {
    __sbrk_heap_end = &_end;
  }

  /* Check for heap overflow */
  if (__sbrk_heap_end + incr > max_heap)
  {
    errno = ENOMEM;
    return (void *)-1;
  }

  prev_heap_end = __sbrk_heap_end;
  __sbrk_heap_end += incr;

  return (void *)prev_heap_end;
}
