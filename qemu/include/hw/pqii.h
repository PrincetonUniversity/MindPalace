#ifndef QEMU_PQII_H
#define QEMU_PQII_H

typedef struct pqii_data {
    uint8_t status;
    uint64_t icount;
} pqii_data_t;

extern pqii_data_t g_pqii_data;


#endif //QEMU_PQII_H
