#include "target.h"

int iDupa = 1488;
uint16_t u16externicznaDupa = 1488;
volatile uint32_t u32eterycznaDupa = 2137;
static uint16_t u16statycznaDupa = 420;
const uint8_t u8StalaDupa = 69;

typedef struct dupaStruct
{
    uint8_t dupa1;
    uint8_t dupa2;
    uint8_t dupa3;
} dupaStruct_t;

dupaStruct_t ustruktyryzowanaDupa = {0};

int main()
{
    printf("Adres stałej dupy %p\n", &u8StalaDupa);
    return 0;
}