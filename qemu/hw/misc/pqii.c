/*
 * Princeton QEMU instrumentation interface
 * QEMU PCI device
 *
 * Copyright (c) 2020 Georgios Tziantzioulis
 * Copyright (c) 2012-2015 Jiri Slaby
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "qemu/units.h"
#include "hw/pci/pci.h"
#include "hw/hw.h"
#include "hw/pci/msi.h"
#include "qemu/timer.h"
#include "qemu/main-loop.h" /* iothread mutex */
#include "qemu/module.h"
#include "qapi/visitor.h"

// START: Georgios
#include "qemu/log.h"
#include "hw/pqii.h"
// END: Georgios

// START: Kaifeng
#include "migration/vmstate.h"
// END: Kaifeng


#define TYPE_PCI_PQII_DEVICE "pqii"
#define PQII(obj)        OBJECT_CHECK(PqiiState, obj, TYPE_PCI_PQII_DEVICE)

#define FACT_IRQ        0x00000001
#define DMA_IRQ         0x00000100

#define DMA_START       0x40000
#define DMA_SIZE        4096


//pqii_data_t g_pqii_data;


typedef struct {
    PCIDevice pdev;
    MemoryRegion mmio;

    QemuThread thread;
    QemuMutex thr_mutex;
    QemuCond thr_cond;
    bool stopping;

    uint32_t addr4;
    uint32_t fact;
#define PQII_STATUS_COMPUTING    0x01
#define PQII_STATUS_IRQFACT      0x80
    uint32_t status;

    uint32_t irq_status;

#define PQII_DMA_RUN             0x1
#define PQII_DMA_DIR(cmd)        (((cmd) & 0x2) >> 1)
# define PQII_DMA_FROM_PCI       0
# define PQII_DMA_TO_PCI         1
#define PQII_DMA_IRQ             0x4
    struct dma_state {
        dma_addr_t src;
        dma_addr_t dst;
        dma_addr_t cnt;
        dma_addr_t cmd;
    } dma;
    QEMUTimer dma_timer;
    char dma_buf[DMA_SIZE];
    uint64_t dma_mask;
} PqiiState;

static bool pqii_msi_enabled(PqiiState *pqii)
{
    return msi_enabled(&pqii->pdev);
}

static void pqii_raise_irq(PqiiState *pqii, uint32_t val)
{
    pqii->irq_status |= val;
    if (pqii->irq_status) {
        if (pqii_msi_enabled(pqii)) {
            msi_notify(&pqii->pdev, 0);
        } else {
            pci_set_irq(&pqii->pdev, 1);
        }
    }
}

static void pqii_lower_irq(PqiiState *pqii, uint32_t val)
{
    pqii->irq_status &= ~val;

    if (!pqii->irq_status && !pqii_msi_enabled(pqii)) {
        pci_set_irq(&pqii->pdev, 0);
    }
}

static bool within(uint64_t addr, uint64_t start, uint64_t end)
{
    return start <= addr && addr < end;
}

static void pqii_check_range(uint64_t addr, uint64_t size1, uint64_t start,
                uint64_t size2)
{
    uint64_t end1 = addr + size1;
    uint64_t end2 = start + size2;

    if (within(addr, start, end2) &&
            end1 > addr && within(end1, start, end2)) {
        return;
    }

    hw_error("PQII: DMA range 0x%016"PRIx64"-0x%016"PRIx64
             " out of bounds (0x%016"PRIx64"-0x%016"PRIx64")!",
            addr, end1 - 1, start, end2 - 1);
}

static dma_addr_t pqii_clamp_addr(const PqiiState *pqii, dma_addr_t addr)
{
    dma_addr_t res = addr & pqii->dma_mask;

    if (addr != res) {
        printf("PQII: clamping DMA %#.16"PRIx64" to %#.16"PRIx64"!\n", addr, res);
    }

    return res;
}

static void pqii_dma_timer(void *opaque)
{
    PqiiState *pqii = opaque;
    bool raise_irq = false;

    if (!(pqii->dma.cmd & PQII_DMA_RUN)) {
        return;
    }

    if (PQII_DMA_DIR(pqii->dma.cmd) == PQII_DMA_FROM_PCI) {
        uint64_t dst = pqii->dma.dst;
        pqii_check_range(dst, pqii->dma.cnt, DMA_START, DMA_SIZE);
        dst -= DMA_START;
        pci_dma_read(&pqii->pdev, pqii_clamp_addr(pqii, pqii->dma.src),
                pqii->dma_buf + dst, pqii->dma.cnt);
    } else {
        uint64_t src = pqii->dma.src;
        pqii_check_range(src, pqii->dma.cnt, DMA_START, DMA_SIZE);
        src -= DMA_START;
        pci_dma_write(&pqii->pdev, pqii_clamp_addr(pqii, pqii->dma.dst),
                pqii->dma_buf + src, pqii->dma.cnt);
    }

    pqii->dma.cmd &= ~PQII_DMA_RUN;
    if (pqii->dma.cmd & PQII_DMA_IRQ) {
        raise_irq = true;
    }

    if (raise_irq) {
        pqii_raise_irq(pqii, DMA_IRQ);
    }
}

static void dma_rw(PqiiState *pqii, bool write, dma_addr_t *val, dma_addr_t *dma,
                bool timer)
{
    if (write && (pqii->dma.cmd & PQII_DMA_RUN)) {
        return;
    }

    if (write) {
        *dma = *val;
    } else {
        *val = *dma;
    }

    if (timer) {
        timer_mod(&pqii->dma_timer, qemu_clock_get_ms(QEMU_CLOCK_VIRTUAL) + 100);
    }
}

static uint64_t pqii_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    PqiiState *pqii = opaque;
    uint64_t val = ~0ULL;
    // qemu_log("*** Georgios: pqii.c:pqii_mmio_read(hwaddr: %lx, size: %u) -> val: %lx\n", addr, size, val);
    
    if (addr < 0x80 && size != 4) {
        return val;
    }

    if (addr >= 0x80 && size != 4 && size != 8) {
        return val;
    }
        
    switch (addr) {
    case 0x00:
        val = 0x010000edu;
        // START: Georgios
        qemu_log("*** Georgios: pqii.c:pqii_mmio_read(hwaddr: %lx, size: %u) -> val: %lx\n", addr, size, val);
        // END: Georgios
        break;
    case 0x04:
        val = pqii->addr4;
        // START: Georgios
        qemu_log("*** Georgios: pqii.c:pqii_mmio_read(hwaddr: %lx, size: %u) -> val: %lx\n", addr, size, val);
        // END: Georgios
        break;
    case 0x08:
        qemu_mutex_lock(&pqii->thr_mutex);
        val = pqii->fact;
        qemu_mutex_unlock(&pqii->thr_mutex);
        break;
    case 0x20:
        val = atomic_read(&pqii->status);
        break;
    case 0x24:
        val = pqii->irq_status;
        break;
    case 0x80:
        dma_rw(pqii, false, &val, &pqii->dma.src, false);
        break;
    case 0x88:
        dma_rw(pqii, false, &val, &pqii->dma.dst, false);
        break;
    case 0x90:
        dma_rw(pqii, false, &val, &pqii->dma.cnt, false);
        break;
    case 0x98:
        dma_rw(pqii, false, &val, &pqii->dma.cmd, false);
        break;
    }

    return val;
}

static void pqii_mmio_write(void *opaque, hwaddr addr, uint64_t val,
                unsigned size)
{
    PqiiState *pqii = opaque;

    // START: Georgios
    // qemu_log("*** Georgios: pqii.c:pqii_mmio_write(hwaddr: %lx, val: %lu, size: %u)\n", addr, val, size);
    // END: Georgios
    
    if (addr < 0x80 && size != 4) {
        return;
    }

    if (addr >= 0x80 && size != 4 && size != 8) {
        return;
    }

    switch (addr) {
    case 0x04:
        pqii->addr4 = ~val;
        break;
    case 0x08:
        if (atomic_read(&pqii->status) & PQII_STATUS_COMPUTING) {
            break;
        }
        /* PQII_STATUS_COMPUTING cannot go 0->1 concurrently, because it is only
         * set in this function and it is under the iothread mutex.
         */
        qemu_mutex_lock(&pqii->thr_mutex);
        pqii->fact = val;
        atomic_or(&pqii->status, PQII_STATUS_COMPUTING);
        qemu_cond_signal(&pqii->thr_cond);
        qemu_mutex_unlock(&pqii->thr_mutex);
        break;
    case 0x20:
        if (val & PQII_STATUS_IRQFACT) {
            atomic_or(&pqii->status, PQII_STATUS_IRQFACT);
        } else {
            atomic_and(&pqii->status, ~PQII_STATUS_IRQFACT);
        }
        break;
    case 0x60:
        pqii_raise_irq(pqii, val);
        break;
    case 0x64:
        pqii_lower_irq(pqii, val);
        break;
    case 0x80:
        dma_rw(pqii, true, &val, &pqii->dma.src, false);
        break;
    case 0x88:
        dma_rw(pqii, true, &val, &pqii->dma.dst, false);
        break;
    case 0x90:
        dma_rw(pqii, true, &val, &pqii->dma.cnt, false);
        break;
    case 0x98:
        if (!(val & PQII_DMA_RUN)) {
            break;
        }
        dma_rw(pqii, true, &val, &pqii->dma.cmd, true);
        break;
    case 0xa0: { // START: Georgios
        // DELETE: Temporary code to test PQII global data struct
        int pqii_status;
        pqii_status = g_pqii_data.status;
        g_pqii_data.status = !g_pqii_data.status;
				g_pqii_data.icount = 0;
        // End of temporary code
        qemu_log("*** Georgios: pqii.c:pqii_mmio_write(hwaddr: %lx, val: %lx, size: %u -- pqii status: %d)\n", addr, val, size, pqii_status);
        break; // END: Georgios
    }
		case 0xa8: { 
				// val[63:4]      Function ID
				// val[3:0]       Mark the invocation status: 4'd0 invoke, 4'd1 function start, 4'd2 function end, 4'd3 respond
        qemu_log("*** Kaifeng:,I4,hwaddr,%lx,val,%lx,size,%u\n", addr, val, size);
				break;
				}
    }
}

static const MemoryRegionOps pqii_mmio_ops = {
    .read = pqii_mmio_read,
    .write = pqii_mmio_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid = {
        .min_access_size = 4,
        .max_access_size = 8,
    },
    .impl = {
        .min_access_size = 4,
        .max_access_size = 8,
    },

};

/*
 * We purposely use a thread, so that users are forced to wait for the status
 * register.
 */
static void *pqii_fact_thread(void *opaque)
{
    PqiiState *pqii = opaque;

    while (1) {
        uint32_t val, ret = 1;

        qemu_mutex_lock(&pqii->thr_mutex);
        while ((atomic_read(&pqii->status) & PQII_STATUS_COMPUTING) == 0 &&
                        !pqii->stopping) {
            qemu_cond_wait(&pqii->thr_cond, &pqii->thr_mutex);
        }

        if (pqii->stopping) {
            qemu_mutex_unlock(&pqii->thr_mutex);
            break;
        }

        val = pqii->fact;
        qemu_mutex_unlock(&pqii->thr_mutex);

        while (val > 0) {
            ret *= val--;
        }

        /*
         * We should sleep for a random period here, so that students are
         * forced to check the status properly.
         */

        qemu_mutex_lock(&pqii->thr_mutex);
        pqii->fact = ret;
        qemu_mutex_unlock(&pqii->thr_mutex);
        atomic_and(&pqii->status, ~PQII_STATUS_COMPUTING);

        if (atomic_read(&pqii->status) & PQII_STATUS_IRQFACT) {
            qemu_mutex_lock_iothread();
            pqii_raise_irq(pqii, FACT_IRQ);
            qemu_mutex_unlock_iothread();
        }
    }

    return NULL;
}

static void pci_pqii_realize(PCIDevice *pdev, Error **errp)
{
    PqiiState *pqii = PQII(pdev);
    uint8_t *pci_conf = pdev->config;

    pci_config_set_interrupt_pin(pci_conf, 1);

    if (msi_init(pdev, 0, 1, true, false, errp)) {
        return;
    }

    timer_init_ms(&pqii->dma_timer, QEMU_CLOCK_VIRTUAL, pqii_dma_timer, pqii);

    qemu_mutex_init(&pqii->thr_mutex);
    qemu_cond_init(&pqii->thr_cond);
    qemu_thread_create(&pqii->thread, "pqii", pqii_fact_thread,
                       pqii, QEMU_THREAD_JOINABLE);

    memory_region_init_io(&pqii->mmio, OBJECT(pqii), &pqii_mmio_ops, pqii,
                    "pqii-mmio", 1 * MiB);
    pci_register_bar(pdev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &pqii->mmio);
}

static void pci_pqii_uninit(PCIDevice *pdev)
{
    PqiiState *pqii = PQII(pdev);

    qemu_mutex_lock(&pqii->thr_mutex);
    pqii->stopping = true;
    qemu_mutex_unlock(&pqii->thr_mutex);
    qemu_cond_signal(&pqii->thr_cond);
    qemu_thread_join(&pqii->thread);

    qemu_cond_destroy(&pqii->thr_cond);
    qemu_mutex_destroy(&pqii->thr_mutex);

    timer_del(&pqii->dma_timer);
    msi_uninit(pdev);
}

static void pqii_instance_init(Object *obj)
{
    PqiiState *pqii = PQII(obj);

    pqii->dma_mask = (1UL << 28) - 1;
    object_property_add_uint64_ptr(obj, "dma_mask",
                                   &pqii->dma_mask, OBJ_PROP_FLAG_READWRITE);
}

// START: Kaifeng
// I thought these 2 functions are used in loadvm/savevm 
// But they are only used in the device initialization
// Maybe we can delete them
static uint32_t pqii_pci_config_read(PCIDevice *d,
                                     uint32_t address, int len)
{
    return pci_default_read_config(d, address, len);
}

static void pqii_pci_config_write(PCIDevice *dev,
                                  uint32_t addr, uint32_t val, int l)
{
    return pci_default_write_config(dev, addr, val, l);
}
// END: Kaifeng

static int pqii_post_load(void *opaque, int version_id)
{
    g_pqii_data.status = 0; // Always disable the trace tool after loadvm
    qemu_log("*** Kaifeng: pqii.c:pqii_post_load, g_pqii_data.status: %d\n", g_pqii_data.status);

    return 0;
}

static const VMStateDescription vmstate_pqii = {
    .name = "PQII",
    .version_id = 0,
    .minimum_version_id = 0,
    .post_load = pqii_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_PCI_DEVICE(pdev, PqiiState),
        VMSTATE_END_OF_LIST()
    }
};

static void pqii_class_init(ObjectClass *class, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(class);
    PCIDeviceClass *k = PCI_DEVICE_CLASS(class);

    k->realize = pci_pqii_realize;
    k->exit = pci_pqii_uninit;
    k->vendor_id = PCI_VENDOR_ID_QEMU;
    // START: Georgios
    //k->device_id = 0x11e8;
    k->device_id = 0xBEEF;
    // END: Georgios
    k->revision = 0x10;
    k->class_id = PCI_CLASS_OTHERS;
    // START: Kaifeng 
    k->config_read = pqii_pci_config_read;
    k->config_write = pqii_pci_config_write;
    dc->vmsd = &vmstate_pqii;
    // END: Kaifeng
    set_bit(DEVICE_CATEGORY_MISC, dc->categories);

    // START: Georgios
    // Initialize PQII global data struct
    g_pqii_data.status = 0;
    // END: Georgios
}

static void pci_pqii_register_types(void)
{
    static InterfaceInfo interfaces[] = {
        { INTERFACE_CONVENTIONAL_PCI_DEVICE },
        { },
    };
    static const TypeInfo pqii_info = {
        .name          = TYPE_PCI_PQII_DEVICE,
        .parent        = TYPE_PCI_DEVICE,
        .instance_size = sizeof(PqiiState),
        .instance_init = pqii_instance_init,
        .class_init    = pqii_class_init,
        .interfaces = interfaces,
    };

    type_register_static(&pqii_info);
}
type_init(pci_pqii_register_types)
