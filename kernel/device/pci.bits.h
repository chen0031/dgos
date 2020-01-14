// THIS FILE IS AUTOMATICALLY GENERATED
// from /home/doug/code/dgos/src/kernel/device/pci.bits

//
// PCI_ID: Identifiers
#define PCI_ID               0x00

#define PCI_ID_DID_BIT       16
#define PCI_ID_VID_BIT       0

#define PCI_ID_DID_BITS      16
#define PCI_ID_VID_BITS      16
#define PCI_ID_DID_MASK      ((1U << PCI_ID_DID_BITS)-1)
#define PCI_ID_VID_MASK      ((1U << PCI_ID_VID_BITS)-1)

// Device ID
#define PCI_ID_DID           (PCI_ID_DID_MASK << PCI_ID_DID_BIT)

// Vendor ID
#define PCI_ID_VID           (PCI_ID_VID_MASK << PCI_ID_VID_BIT)

#define PCI_ID_DID_n(n)      ((n) << PCI_ID_DID_BIT)
#define PCI_ID_VID_n(n)      ((n) << PCI_ID_VID_BIT)

#define PCI_ID_DID_GET(n)    (((n) >> PCI_ID_DID_BIT) & PCI_ID_DID_MASK)
#define PCI_ID_VID_GET(n)    (((n) >> PCI_ID_VID_BIT) & PCI_ID_VID_MASK)

#define PCI_ID_DID_SET(r,n)  ((r) = ((r) & ~PCI_ID_DID) | PCI_ID_DID_n((n)))
#define PCI_ID_VID_SET(r,n)  ((r) = ((r) & ~PCI_ID_VID) | PCI_ID_VID_n((n)))

//
// PCI_CMD: Command
#define PCI_CMD                0x04

#define PCI_CMD_ID_BIT         10
#define PCI_CMD_FBE_BIT        9
#define PCI_CMD_SEE_BIT        8
#define PCI_CMD_PEE_BIT        6
#define PCI_CMD_VGA_BIT        5
#define PCI_CMD_MWIE_BIT       4
#define PCI_CMD_SCE_BIT        3
#define PCI_CMD_BME_BIT        2
#define PCI_CMD_MSE_BIT        1
#define PCI_CMD_IOSE_BIT       0

#define PCI_CMD_ID_BITS        1
#define PCI_CMD_FBE_BITS       1
#define PCI_CMD_SEE_BITS       1
#define PCI_CMD_PEE_BITS       1
#define PCI_CMD_VGA_BITS       1
#define PCI_CMD_MWIE_BITS      1
#define PCI_CMD_SCE_BITS       1
#define PCI_CMD_BME_BITS       1
#define PCI_CMD_MSE_BITS       1
#define PCI_CMD_IOSE_BITS      1
#define PCI_CMD_ID_MASK        ((1U << PCI_CMD_ID_BITS)-1)
#define PCI_CMD_FBE_MASK       ((1U << PCI_CMD_FBE_BITS)-1)
#define PCI_CMD_SEE_MASK       ((1U << PCI_CMD_SEE_BITS)-1)
#define PCI_CMD_PEE_MASK       ((1U << PCI_CMD_PEE_BITS)-1)
#define PCI_CMD_VGA_MASK       ((1U << PCI_CMD_VGA_BITS)-1)
#define PCI_CMD_MWIE_MASK      ((1U << PCI_CMD_MWIE_BITS)-1)
#define PCI_CMD_SCE_MASK       ((1U << PCI_CMD_SCE_BITS)-1)
#define PCI_CMD_BME_MASK       ((1U << PCI_CMD_BME_BITS)-1)
#define PCI_CMD_MSE_MASK       ((1U << PCI_CMD_MSE_BITS)-1)
#define PCI_CMD_IOSE_MASK      ((1U << PCI_CMD_IOSE_BITS)-1)

// Interrupt pin disable (does not affect MSI)
#define PCI_CMD_ID             (PCI_CMD_ID_MASK << PCI_CMD_ID_BIT)

// Fast back-to-back enable
#define PCI_CMD_FBE            (PCI_CMD_FBE_MASK << PCI_CMD_FBE_BIT)

// SERR# Enable
#define PCI_CMD_SEE            (PCI_CMD_SEE_MASK << PCI_CMD_SEE_BIT)

// Parity error response enable
#define PCI_CMD_PEE            (PCI_CMD_PEE_MASK << PCI_CMD_PEE_BIT)

// VGA palette snooping enable
#define PCI_CMD_VGA            (PCI_CMD_VGA_MASK << PCI_CMD_VGA_BIT)

// Memory write and invalidate enable
#define PCI_CMD_MWIE           (PCI_CMD_MWIE_MASK << PCI_CMD_MWIE_BIT)

// Special cycle enable
#define PCI_CMD_SCE            (PCI_CMD_SCE_MASK << PCI_CMD_SCE_BIT)

// Bus master enable
#define PCI_CMD_BME            (PCI_CMD_BME_MASK << PCI_CMD_BME_BIT)

// Memory space enable
#define PCI_CMD_MSE            (PCI_CMD_MSE_MASK << PCI_CMD_MSE_BIT)

// I/O space enable
#define PCI_CMD_IOSE           (PCI_CMD_IOSE_MASK << PCI_CMD_IOSE_BIT)

#define PCI_CMD_ID_n(n)        ((n) << PCI_CMD_ID_BIT)
#define PCI_CMD_FBE_n(n)       ((n) << PCI_CMD_FBE_BIT)
#define PCI_CMD_SEE_n(n)       ((n) << PCI_CMD_SEE_BIT)
#define PCI_CMD_PEE_n(n)       ((n) << PCI_CMD_PEE_BIT)
#define PCI_CMD_VGA_n(n)       ((n) << PCI_CMD_VGA_BIT)
#define PCI_CMD_MWIE_n(n)      ((n) << PCI_CMD_MWIE_BIT)
#define PCI_CMD_SCE_n(n)       ((n) << PCI_CMD_SCE_BIT)
#define PCI_CMD_BME_n(n)       ((n) << PCI_CMD_BME_BIT)
#define PCI_CMD_MSE_n(n)       ((n) << PCI_CMD_MSE_BIT)
#define PCI_CMD_IOSE_n(n)      ((n) << PCI_CMD_IOSE_BIT)

#define PCI_CMD_ID_GET(n)      (((n) >> PCI_CMD_ID_BIT) & PCI_CMD_ID_MASK)
#define PCI_CMD_FBE_GET(n)     (((n) >> PCI_CMD_FBE_BIT) & PCI_CMD_FBE_MASK)
#define PCI_CMD_SEE_GET(n)     (((n) >> PCI_CMD_SEE_BIT) & PCI_CMD_SEE_MASK)
#define PCI_CMD_PEE_GET(n)     (((n) >> PCI_CMD_PEE_BIT) & PCI_CMD_PEE_MASK)
#define PCI_CMD_VGA_GET(n)     (((n) >> PCI_CMD_VGA_BIT) & PCI_CMD_VGA_MASK)
#define PCI_CMD_MWIE_GET(n)    (((n) >> PCI_CMD_MWIE_BIT) & PCI_CMD_MWIE_MASK)
#define PCI_CMD_SCE_GET(n)     (((n) >> PCI_CMD_SCE_BIT) & PCI_CMD_SCE_MASK)
#define PCI_CMD_BME_GET(n)     (((n) >> PCI_CMD_BME_BIT) & PCI_CMD_BME_MASK)
#define PCI_CMD_MSE_GET(n)     (((n) >> PCI_CMD_MSE_BIT) & PCI_CMD_MSE_MASK)
#define PCI_CMD_IOSE_GET(n)    (((n) >> PCI_CMD_IOSE_BIT) & PCI_CMD_IOSE_MASK)

#define PCI_CMD_ID_SET(r,n)    ((r) = ((r) & ~PCI_CMD_ID) | PCI_CMD_ID_n((n)))
#define PCI_CMD_FBE_SET(r,n) \
    ((r) = ((r) & ~PCI_CMD_FBE) | PCI_CMD_FBE_n((n)))
#define PCI_CMD_SEE_SET(r,n) \
    ((r) = ((r) & ~PCI_CMD_SEE) | PCI_CMD_SEE_n((n)))
#define PCI_CMD_PEE_SET(r,n) \
    ((r) = ((r) & ~PCI_CMD_PEE) | PCI_CMD_PEE_n((n)))
#define PCI_CMD_VGA_SET(r,n) \
    ((r) = ((r) & ~PCI_CMD_VGA) | PCI_CMD_VGA_n((n)))
#define PCI_CMD_MWIE_SET(r,n) \
    ((r) = ((r) & ~PCI_CMD_MWIE) | PCI_CMD_MWIE_n((n)))
#define PCI_CMD_SCE_SET(r,n) \
    ((r) = ((r) & ~PCI_CMD_SCE) | PCI_CMD_SCE_n((n)))
#define PCI_CMD_BME_SET(r,n) \
    ((r) = ((r) & ~PCI_CMD_BME) | PCI_CMD_BME_n((n)))
#define PCI_CMD_MSE_SET(r,n) \
    ((r) = ((r) & ~PCI_CMD_MSE) | PCI_CMD_MSE_n((n)))
#define PCI_CMD_IOSE_SET(r,n) \
    ((r) = ((r) & ~PCI_CMD_IOSE) | PCI_CMD_IOSE_n((n)))

//
// PCI_STS: Device status
#define PCI_STS                0x06

#define PCI_STS_DPE_BIT        15
#define PCI_STS_SSE_BIT        14
#define PCI_STS_RMA_BIT        13
#define PCI_STS_RTA_BIT        12
#define PCI_STS_STO_BIT        11
#define PCI_STS_DEVT_BIT       9
#define PCI_STS_DPD_BIT        8
#define PCI_STS_FBC_BIT        7
#define PCI_STS_C66_BIT        5
#define PCI_STS_CL_BIT         4
#define PCI_STS_IS_BIT         3

#define PCI_STS_DPE_BITS       1
#define PCI_STS_SSE_BITS       1
#define PCI_STS_RMA_BITS       1
#define PCI_STS_RTA_BITS       1
#define PCI_STS_STO_BITS       1
#define PCI_STS_DEVT_BITS      2
#define PCI_STS_DPD_BITS       1
#define PCI_STS_FBC_BITS       1
#define PCI_STS_C66_BITS       1
#define PCI_STS_CL_BITS        1
#define PCI_STS_IS_BITS        1
#define PCI_STS_DPE_MASK       ((1U << PCI_STS_DPE_BITS)-1)
#define PCI_STS_SSE_MASK       ((1U << PCI_STS_SSE_BITS)-1)
#define PCI_STS_RMA_MASK       ((1U << PCI_STS_RMA_BITS)-1)
#define PCI_STS_RTA_MASK       ((1U << PCI_STS_RTA_BITS)-1)
#define PCI_STS_STO_MASK       ((1U << PCI_STS_STO_BITS)-1)
#define PCI_STS_DEVT_MASK      ((1U << PCI_STS_DEVT_BITS)-1)
#define PCI_STS_DPD_MASK       ((1U << PCI_STS_DPD_BITS)-1)
#define PCI_STS_FBC_MASK       ((1U << PCI_STS_FBC_BITS)-1)
#define PCI_STS_C66_MASK       ((1U << PCI_STS_C66_BITS)-1)
#define PCI_STS_CL_MASK        ((1U << PCI_STS_CL_BITS)-1)
#define PCI_STS_IS_MASK        ((1U << PCI_STS_IS_BITS)-1)

// Detected parity error
#define PCI_STS_DPE            (PCI_STS_DPE_MASK << PCI_STS_DPE_BIT)

// Signalled system error
#define PCI_STS_SSE            (PCI_STS_SSE_MASK << PCI_STS_SSE_BIT)

// Received master abort
#define PCI_STS_RMA            (PCI_STS_RMA_MASK << PCI_STS_RMA_BIT)

// Received target abort
#define PCI_STS_RTA            (PCI_STS_RTA_MASK << PCI_STS_RTA_BIT)

// Signaled target abort
#define PCI_STS_STO            (PCI_STS_STO_MASK << PCI_STS_STO_BIT)

// DEVSEL# timing
#define PCI_STS_DEVT           (PCI_STS_DEVT_MASK << PCI_STS_DEVT_BIT)

// Master data parity error detected
#define PCI_STS_DPD            (PCI_STS_DPD_MASK << PCI_STS_DPD_BIT)

// Fast back to back capable
#define PCI_STS_FBC            (PCI_STS_FBC_MASK << PCI_STS_FBC_BIT)

// 66MHz capable
#define PCI_STS_C66            (PCI_STS_C66_MASK << PCI_STS_C66_BIT)

// Capabilities list present
#define PCI_STS_CL             (PCI_STS_CL_MASK << PCI_STS_CL_BIT)

// Interrupt status (1=asserted)
#define PCI_STS_IS             (PCI_STS_IS_MASK << PCI_STS_IS_BIT)

#define PCI_STS_DPE_n(n)       ((n) << PCI_STS_DPE_BIT)
#define PCI_STS_SSE_n(n)       ((n) << PCI_STS_SSE_BIT)
#define PCI_STS_RMA_n(n)       ((n) << PCI_STS_RMA_BIT)
#define PCI_STS_RTA_n(n)       ((n) << PCI_STS_RTA_BIT)
#define PCI_STS_STO_n(n)       ((n) << PCI_STS_STO_BIT)
#define PCI_STS_DEVT_n(n)      ((n) << PCI_STS_DEVT_BIT)
#define PCI_STS_DPD_n(n)       ((n) << PCI_STS_DPD_BIT)
#define PCI_STS_FBC_n(n)       ((n) << PCI_STS_FBC_BIT)
#define PCI_STS_C66_n(n)       ((n) << PCI_STS_C66_BIT)
#define PCI_STS_CL_n(n)        ((n) << PCI_STS_CL_BIT)
#define PCI_STS_IS_n(n)        ((n) << PCI_STS_IS_BIT)

#define PCI_STS_DPE_GET(n)     (((n) >> PCI_STS_DPE_BIT) & PCI_STS_DPE_MASK)
#define PCI_STS_SSE_GET(n)     (((n) >> PCI_STS_SSE_BIT) & PCI_STS_SSE_MASK)
#define PCI_STS_RMA_GET(n)     (((n) >> PCI_STS_RMA_BIT) & PCI_STS_RMA_MASK)
#define PCI_STS_RTA_GET(n)     (((n) >> PCI_STS_RTA_BIT) & PCI_STS_RTA_MASK)
#define PCI_STS_STO_GET(n)     (((n) >> PCI_STS_STO_BIT) & PCI_STS_STO_MASK)
#define PCI_STS_DEVT_GET(n)    (((n) >> PCI_STS_DEVT_BIT) & PCI_STS_DEVT_MASK)
#define PCI_STS_DPD_GET(n)     (((n) >> PCI_STS_DPD_BIT) & PCI_STS_DPD_MASK)
#define PCI_STS_FBC_GET(n)     (((n) >> PCI_STS_FBC_BIT) & PCI_STS_FBC_MASK)
#define PCI_STS_C66_GET(n)     (((n) >> PCI_STS_C66_BIT) & PCI_STS_C66_MASK)
#define PCI_STS_CL_GET(n)      (((n) >> PCI_STS_CL_BIT) & PCI_STS_CL_MASK)
#define PCI_STS_IS_GET(n)      (((n) >> PCI_STS_IS_BIT) & PCI_STS_IS_MASK)

#define PCI_STS_DPE_SET(r,n) \
    ((r) = ((r) & ~PCI_STS_DPE) | PCI_STS_DPE_n((n)))
#define PCI_STS_SSE_SET(r,n) \
    ((r) = ((r) & ~PCI_STS_SSE) | PCI_STS_SSE_n((n)))
#define PCI_STS_RMA_SET(r,n) \
    ((r) = ((r) & ~PCI_STS_RMA) | PCI_STS_RMA_n((n)))
#define PCI_STS_RTA_SET(r,n) \
    ((r) = ((r) & ~PCI_STS_RTA) | PCI_STS_RTA_n((n)))
#define PCI_STS_STO_SET(r,n) \
    ((r) = ((r) & ~PCI_STS_STO) | PCI_STS_STO_n((n)))
#define PCI_STS_DEVT_SET(r,n) \
    ((r) = ((r) & ~PCI_STS_DEVT) | PCI_STS_DEVT_n((n)))
#define PCI_STS_DPD_SET(r,n) \
    ((r) = ((r) & ~PCI_STS_DPD) | PCI_STS_DPD_n((n)))
#define PCI_STS_FBC_SET(r,n) \
    ((r) = ((r) & ~PCI_STS_FBC) | PCI_STS_FBC_n((n)))
#define PCI_STS_C66_SET(r,n) \
    ((r) = ((r) & ~PCI_STS_C66) | PCI_STS_C66_n((n)))
#define PCI_STS_CL_SET(r,n)    ((r) = ((r) & ~PCI_STS_CL) | PCI_STS_CL_n((n)))
#define PCI_STS_IS_SET(r,n)    ((r) = ((r) & ~PCI_STS_IS) | PCI_STS_IS_n((n)))

//
// PCI_RID: Revision ID
#define PCI_RID               0x08

#define PCI_RID_RID_BIT       0

#define PCI_RID_RID_BITS      8
#define PCI_RID_RID_MASK      ((1U << PCI_RID_RID_BITS)-1)

// Revision ID
#define PCI_RID_RID           (PCI_RID_RID_MASK << PCI_RID_RID_BIT)

#define PCI_RID_RID_n(n)      ((n) << PCI_RID_RID_BIT)

#define PCI_RID_RID_GET(n)    (((n) >> PCI_RID_RID_BIT) & PCI_RID_RID_MASK)

#define PCI_RID_RID_SET(r,n)  ((r) = ((r) & ~PCI_RID_RID) | PCI_RID_RID_n((n)))

//
// PCI_CC: Class code
#define PCI_CC               0x09

#define PCI_CC_BCC_BIT       16
#define PCI_CC_SCC_BIT       8
#define PCI_CC_PI_BIT        0

#define PCI_CC_BCC_BITS      8
#define PCI_CC_SCC_BITS      8
#define PCI_CC_PI_BITS       8
#define PCI_CC_BCC_MASK      ((1U << PCI_CC_BCC_BITS)-1)
#define PCI_CC_SCC_MASK      ((1U << PCI_CC_SCC_BITS)-1)
#define PCI_CC_PI_MASK       ((1U << PCI_CC_PI_BITS)-1)

// Base class code
#define PCI_CC_BCC           (PCI_CC_BCC_MASK << PCI_CC_BCC_BIT)

// Sub class code
#define PCI_CC_SCC           (PCI_CC_SCC_MASK << PCI_CC_SCC_BIT)

// Programming interface
#define PCI_CC_PI            (PCI_CC_PI_MASK << PCI_CC_PI_BIT)

#define PCI_CC_BCC_n(n)      ((n) << PCI_CC_BCC_BIT)
#define PCI_CC_SCC_n(n)      ((n) << PCI_CC_SCC_BIT)
#define PCI_CC_PI_n(n)       ((n) << PCI_CC_PI_BIT)

#define PCI_CC_BCC_GET(n)    (((n) >> PCI_CC_BCC_BIT) & PCI_CC_BCC_MASK)
#define PCI_CC_SCC_GET(n)    (((n) >> PCI_CC_SCC_BIT) & PCI_CC_SCC_MASK)
#define PCI_CC_PI_GET(n)     (((n) >> PCI_CC_PI_BIT) & PCI_CC_PI_MASK)

#define PCI_CC_BCC_SET(r,n)  ((r) = ((r) & ~PCI_CC_BCC) | PCI_CC_BCC_n((n)))
#define PCI_CC_SCC_SET(r,n)  ((r) = ((r) & ~PCI_CC_SCC) | PCI_CC_SCC_n((n)))
#define PCI_CC_PI_SET(r,n)   ((r) = ((r) & ~PCI_CC_PI) | PCI_CC_PI_n((n)))

//
// PCI_CLS: Cache line size
#define PCI_CLS               0x0C

#define PCI_CLS_CLS_BIT       0

#define PCI_CLS_CLS_BITS      8
#define PCI_CLS_CLS_MASK      ((1U << PCI_CLS_CLS_BITS)-1)

// Cache line size
#define PCI_CLS_CLS           (PCI_CLS_CLS_MASK << PCI_CLS_CLS_BIT)

#define PCI_CLS_CLS_n(n)      ((n) << PCI_CLS_CLS_BIT)

#define PCI_CLS_CLS_GET(n)    (((n) >> PCI_CLS_CLS_BIT) & PCI_CLS_CLS_MASK)

#define PCI_CLS_CLS_SET(r,n)  ((r) = ((r) & ~PCI_CLS_CLS) | PCI_CLS_CLS_n((n)))

//
// PCI_MLT: Master latency timer
#define PCI_MLT               0x0D

#define PCI_MLT_MLT_BIT       0

#define PCI_MLT_MLT_BITS      8
#define PCI_MLT_MLT_MASK      ((1U << PCI_MLT_MLT_BITS)-1)

// Master latency timer
#define PCI_MLT_MLT           (PCI_MLT_MLT_MASK << PCI_MLT_MLT_BIT)

#define PCI_MLT_MLT_n(n)      ((n) << PCI_MLT_MLT_BIT)

#define PCI_MLT_MLT_GET(n)    (((n) >> PCI_MLT_MLT_BIT) & PCI_MLT_MLT_MASK)

#define PCI_MLT_MLT_SET(r,n)  ((r) = ((r) & ~PCI_MLT_MLT) | PCI_MLT_MLT_n((n)))

//
// PCI_HTYPE: Header type
#define PCI_HTYPE               0x0E

#define PCI_HTYPE_MFD_BIT       7
#define PCI_HTYPE_HL_BIT        0

#define PCI_HTYPE_MFD_BITS      1
#define PCI_HTYPE_HL_BITS       7
#define PCI_HTYPE_MFD_MASK      ((1U << PCI_HTYPE_MFD_BITS)-1)
#define PCI_HTYPE_HL_MASK       ((1U << PCI_HTYPE_HL_BITS)-1)

// Multifunction device
#define PCI_HTYPE_MFD           (PCI_HTYPE_MFD_MASK << PCI_HTYPE_MFD_BIT)

// Header layout
#define PCI_HTYPE_HL            (PCI_HTYPE_HL_MASK << PCI_HTYPE_HL_BIT)

#define PCI_HTYPE_MFD_n(n)      ((n) << PCI_HTYPE_MFD_BIT)
#define PCI_HTYPE_HL_n(n)       ((n) << PCI_HTYPE_HL_BIT)

#define PCI_HTYPE_MFD_GET(n) \
    (((n) >> PCI_HTYPE_MFD_BIT) & PCI_HTYPE_MFD_MASK)
#define PCI_HTYPE_HL_GET(n)     (((n) >> PCI_HTYPE_HL_BIT) & PCI_HTYPE_HL_MASK)

#define PCI_HTYPE_MFD_SET(r,n) \
    ((r) = ((r) & ~PCI_HTYPE_MFD) | PCI_HTYPE_MFD_n((n)))
#define PCI_HTYPE_HL_SET(r,n) \
    ((r) = ((r) & ~PCI_HTYPE_HL) | PCI_HTYPE_HL_n((n)))

//
// PCI_BIST: Built in self test (optional)
#define PCI_BIST              

#define PCI_BIST_BC_BIT       7
#define PCI_BIST_SB_BIT       6
#define PCI_BIST_CC_BIT       0

#define PCI_BIST_BC_BITS      1
#define PCI_BIST_SB_BITS      1
#define PCI_BIST_CC_BITS      4
#define PCI_BIST_BC_MASK      ((1U << PCI_BIST_BC_BITS)-1)
#define PCI_BIST_SB_MASK      ((1U << PCI_BIST_SB_BITS)-1)
#define PCI_BIST_CC_MASK      ((1U << PCI_BIST_CC_BITS)-1)

// Built in self test capable
#define PCI_BIST_BC           (PCI_BIST_BC_MASK << PCI_BIST_BC_BIT)

// Start built in self test
#define PCI_BIST_SB           (PCI_BIST_SB_MASK << PCI_BIST_SB_BIT)

// Completion code
#define PCI_BIST_CC           (PCI_BIST_CC_MASK << PCI_BIST_CC_BIT)

#define PCI_BIST_BC_n(n)      ((n) << PCI_BIST_BC_BIT)
#define PCI_BIST_SB_n(n)      ((n) << PCI_BIST_SB_BIT)
#define PCI_BIST_CC_n(n)      ((n) << PCI_BIST_CC_BIT)

#define PCI_BIST_BC_GET(n)    (((n) >> PCI_BIST_BC_BIT) & PCI_BIST_BC_MASK)
#define PCI_BIST_SB_GET(n)    (((n) >> PCI_BIST_SB_BIT) & PCI_BIST_SB_MASK)
#define PCI_BIST_CC_GET(n)    (((n) >> PCI_BIST_CC_BIT) & PCI_BIST_CC_MASK)

#define PCI_BIST_BC_SET(r,n)  ((r) = ((r) & ~PCI_BIST_BC) | PCI_BIST_BC_n((n)))
#define PCI_BIST_SB_SET(r,n)  ((r) = ((r) & ~PCI_BIST_SB) | PCI_BIST_SB_n((n)))
#define PCI_BIST_CC_SET(r,n)  ((r) = ((r) & ~PCI_BIST_CC) | PCI_BIST_CC_n((n)))

// PCI_BAR

#define PCI_BAR_BA_BIT         4
#define PCI_BAR_PF_BIT         3
#define PCI_BAR_TYPE_BIT       1
#define PCI_BAR_RTE_BIT        0

#define PCI_BAR_BA_BITS        28
#define PCI_BAR_PF_BITS        1
#define PCI_BAR_TYPE_BITS      2
#define PCI_BAR_RTE_BITS       1
#define PCI_BAR_BA_MASK        ((1U << PCI_BAR_BA_BITS)-1)
#define PCI_BAR_PF_MASK        ((1U << PCI_BAR_PF_BITS)-1)
#define PCI_BAR_TYPE_MASK      ((1U << PCI_BAR_TYPE_BITS)-1)
#define PCI_BAR_RTE_MASK       ((1U << PCI_BAR_RTE_BITS)-1)

// Base address
#define PCI_BAR_BA             (PCI_BAR_BA_MASK << PCI_BAR_BA_BIT)

// Prefetchable
#define PCI_BAR_PF             (PCI_BAR_PF_MASK << PCI_BAR_PF_BIT)

// (0=32-bit, 2=64-bit, 1=reserved, 3=reserved)
#define PCI_BAR_TYPE           (PCI_BAR_TYPE_MASK << PCI_BAR_TYPE_BIT)

// Resource type indicator
#define PCI_BAR_RTE            (PCI_BAR_RTE_MASK << PCI_BAR_RTE_BIT)

#define PCI_BAR_BA_n(n)        ((n) << PCI_BAR_BA_BIT)
#define PCI_BAR_PF_n(n)        ((n) << PCI_BAR_PF_BIT)
#define PCI_BAR_TYPE_n(n)      ((n) << PCI_BAR_TYPE_BIT)
#define PCI_BAR_RTE_n(n)       ((n) << PCI_BAR_RTE_BIT)

#define PCI_BAR_BA_GET(n)      (((n) >> PCI_BAR_BA_BIT) & PCI_BAR_BA_MASK)
#define PCI_BAR_PF_GET(n)      (((n) >> PCI_BAR_PF_BIT) & PCI_BAR_PF_MASK)
#define PCI_BAR_TYPE_GET(n)    (((n) >> PCI_BAR_TYPE_BIT) & PCI_BAR_TYPE_MASK)
#define PCI_BAR_RTE_GET(n)     (((n) >> PCI_BAR_RTE_BIT) & PCI_BAR_RTE_MASK)

#define PCI_BAR_BA_SET(r,n)    ((r) = ((r) & ~PCI_BAR_BA) | PCI_BAR_BA_n((n)))
#define PCI_BAR_PF_SET(r,n)    ((r) = ((r) & ~PCI_BAR_PF) | PCI_BAR_PF_n((n)))
#define PCI_BAR_TYPE_SET(r,n) \
    ((r) = ((r) & ~PCI_BAR_TYPE) | PCI_BAR_TYPE_n((n)))
#define PCI_BAR_RTE_SET(r,n) \
    ((r) = ((r) & ~PCI_BAR_RTE) | PCI_BAR_RTE_n((n)))

//
// PCI_SS: Subsystem identifiers

#define PCI_SS_SSID_BIT        16
#define PCI_SS_SSVID_BIT       0

#define PCI_SS_SSID_BITS       16
#define PCI_SS_SSVID_BITS      16
#define PCI_SS_SSID_MASK       ((1U << PCI_SS_SSID_BITS)-1)
#define PCI_SS_SSVID_MASK      ((1U << PCI_SS_SSVID_BITS)-1)

// Subsystem identifier
#define PCI_SS_SSID            (PCI_SS_SSID_MASK << PCI_SS_SSID_BIT)

// Subsystem vendor identifier
#define PCI_SS_SSVID           (PCI_SS_SSVID_MASK << PCI_SS_SSVID_BIT)

#define PCI_SS_SSID_n(n)       ((n) << PCI_SS_SSID_BIT)
#define PCI_SS_SSVID_n(n)      ((n) << PCI_SS_SSVID_BIT)

#define PCI_SS_SSID_GET(n)     (((n) >> PCI_SS_SSID_BIT) & PCI_SS_SSID_MASK)
#define PCI_SS_SSVID_GET(n)    (((n) >> PCI_SS_SSVID_BIT) & PCI_SS_SSVID_MASK)

#define PCI_SS_SSID_SET(r,n) \
    ((r) = ((r) & ~PCI_SS_SSID) | PCI_SS_SSID_n((n)))
#define PCI_SS_SSVID_SET(r,n) \
    ((r) = ((r) & ~PCI_SS_SSVID) | PCI_SS_SSVID_n((n)))

//
// PCI_CAP: Capabilities pointer

#define PCI_CAP_CP_BIT       0

#define PCI_CAP_CP_BITS      8
#define PCI_CAP_CP_MASK      ((1U << PCI_CAP_CP_BITS)-1)

// Capability pointer
#define PCI_CAP_CP           (PCI_CAP_CP_MASK << PCI_CAP_CP_BIT)

#define PCI_CAP_CP_n(n)      ((n) << PCI_CAP_CP_BIT)

#define PCI_CAP_CP_GET(n)    (((n) >> PCI_CAP_CP_BIT) & PCI_CAP_CP_MASK)

#define PCI_CAP_CP_SET(r,n)  ((r) = ((r) & ~PCI_CAP_CP) | PCI_CAP_CP_n((n)))

// PCI_INTR

#define PCI_INTR_IPIN_BIT        8
#define PCI_INTR_ILINE_BIT       0

#define PCI_INTR_IPIN_BITS       8
#define PCI_INTR_ILINE_BITS      8
#define PCI_INTR_IPIN_MASK       ((1U << PCI_INTR_IPIN_BITS)-1)
#define PCI_INTR_ILINE_MASK      ((1U << PCI_INTR_ILINE_BITS)-1)

// Interrupt pin
#define PCI_INTR_IPIN            (PCI_INTR_IPIN_MASK << PCI_INTR_IPIN_BIT)

// Interrupt line
#define PCI_INTR_ILINE           (PCI_INTR_ILINE_MASK << PCI_INTR_ILINE_BIT)

#define PCI_INTR_IPIN_n(n)       ((n) << PCI_INTR_IPIN_BIT)
#define PCI_INTR_ILINE_n(n)      ((n) << PCI_INTR_ILINE_BIT)

#define PCI_INTR_IPIN_GET(n) \
    (((n) >> PCI_INTR_IPIN_BIT) & PCI_INTR_IPIN_MASK)
#define PCI_INTR_ILINE_GET(n) \
    (((n) >> PCI_INTR_ILINE_BIT) & PCI_INTR_ILINE_MASK)

#define PCI_INTR_IPIN_SET(r,n) \
    ((r) = ((r) & ~PCI_INTR_IPIN) | PCI_INTR_IPIN_n((n)))
#define PCI_INTR_ILINE_SET(r,n) \
    ((r) = ((r) & ~PCI_INTR_ILINE) | PCI_INTR_ILINE_n((n)))

//
// PCI_MGNT: Minimum grant

#define PCI_MGNT_GNT_BIT       0

#define PCI_MGNT_GNT_BITS      8
#define PCI_MGNT_GNT_MASK      ((1U << PCI_MGNT_GNT_BITS)-1)

// Minimum grant
#define PCI_MGNT_GNT           (PCI_MGNT_GNT_MASK << PCI_MGNT_GNT_BIT)

#define PCI_MGNT_GNT_n(n)      ((n) << PCI_MGNT_GNT_BIT)

#define PCI_MGNT_GNT_GET(n)    (((n) >> PCI_MGNT_GNT_BIT) & PCI_MGNT_GNT_MASK)

#define PCI_MGNT_GNT_SET(r,n) \
    ((r) = ((r) & ~PCI_MGNT_GNT) | PCI_MGNT_GNT_n((n)))

//
// PCI_MLAT: Maximum latency

#define PCI_MLAT_LAT_BIT       0

#define PCI_MLAT_LAT_BITS      8
#define PCI_MLAT_LAT_MASK      ((1U << PCI_MLAT_LAT_BITS)-1)

// Maximum latency
#define PCI_MLAT_LAT           (PCI_MLAT_LAT_MASK << PCI_MLAT_LAT_BIT)

#define PCI_MLAT_LAT_n(n)      ((n) << PCI_MLAT_LAT_BIT)

#define PCI_MLAT_LAT_GET(n)    (((n) >> PCI_MLAT_LAT_BIT) & PCI_MLAT_LAT_MASK)

#define PCI_MLAT_LAT_SET(r,n) \
    ((r) = ((r) & ~PCI_MLAT_LAT) | PCI_MLAT_LAT_n((n)))

// PCI_MSI_MSG_CTRL

#define PCI_MSI_MSG_CTRL_VMASK_BIT       8
#define PCI_MSI_MSG_CTRL_CAP64_BIT       7
#define PCI_MSI_MSG_CTRL_MME_BIT         4
#define PCI_MSI_MSG_CTRL_MMC_BIT         1
#define PCI_MSI_MSG_CTRL_EN_BIT          0

#define PCI_MSI_MSG_CTRL_VMASK_BITS      1
#define PCI_MSI_MSG_CTRL_CAP64_BITS      1
#define PCI_MSI_MSG_CTRL_MME_BITS        3
#define PCI_MSI_MSG_CTRL_MMC_BITS        3
#define PCI_MSI_MSG_CTRL_EN_BITS         1
#define PCI_MSI_MSG_CTRL_VMASK_MASK \
    ((1U << PCI_MSI_MSG_CTRL_VMASK_BITS)-1)
#define PCI_MSI_MSG_CTRL_CAP64_MASK \
    ((1U << PCI_MSI_MSG_CTRL_CAP64_BITS)-1)
#define PCI_MSI_MSG_CTRL_MME_MASK        ((1U << PCI_MSI_MSG_CTRL_MME_BITS)-1)
#define PCI_MSI_MSG_CTRL_MMC_MASK        ((1U << PCI_MSI_MSG_CTRL_MMC_BITS)-1)
#define PCI_MSI_MSG_CTRL_EN_MASK         ((1U << PCI_MSI_MSG_CTRL_EN_BITS)-1)

// Per-vector mask capable
#define PCI_MSI_MSG_CTRL_VMASK \
    (PCI_MSI_MSG_CTRL_VMASK_MASK << PCI_MSI_MSG_CTRL_VMASK_BIT)

// 64-bit capable
#define PCI_MSI_MSG_CTRL_CAP64 \
    (PCI_MSI_MSG_CTRL_CAP64_MASK << PCI_MSI_MSG_CTRL_CAP64_BIT)

// Multiple message enable
#define PCI_MSI_MSG_CTRL_MME \
    (PCI_MSI_MSG_CTRL_MME_MASK << PCI_MSI_MSG_CTRL_MME_BIT)

// Multiple message capable (log2 N)
#define PCI_MSI_MSG_CTRL_MMC \
    (PCI_MSI_MSG_CTRL_MMC_MASK << PCI_MSI_MSG_CTRL_MMC_BIT)

// Enable
#define PCI_MSI_MSG_CTRL_EN \
    (PCI_MSI_MSG_CTRL_EN_MASK << PCI_MSI_MSG_CTRL_EN_BIT)

#define PCI_MSI_MSG_CTRL_VMASK_n(n)      ((n) << PCI_MSI_MSG_CTRL_VMASK_BIT)
#define PCI_MSI_MSG_CTRL_CAP64_n(n)      ((n) << PCI_MSI_MSG_CTRL_CAP64_BIT)
#define PCI_MSI_MSG_CTRL_MME_n(n)        ((n) << PCI_MSI_MSG_CTRL_MME_BIT)
#define PCI_MSI_MSG_CTRL_MMC_n(n)        ((n) << PCI_MSI_MSG_CTRL_MMC_BIT)
#define PCI_MSI_MSG_CTRL_EN_n(n)         ((n) << PCI_MSI_MSG_CTRL_EN_BIT)

#define PCI_MSI_MSG_CTRL_VMASK_GET(n) \
    (((n) >> PCI_MSI_MSG_CTRL_VMASK_BIT) & PCI_MSI_MSG_CTRL_VMASK_MASK)
#define PCI_MSI_MSG_CTRL_CAP64_GET(n) \
    (((n) >> PCI_MSI_MSG_CTRL_CAP64_BIT) & PCI_MSI_MSG_CTRL_CAP64_MASK)
#define PCI_MSI_MSG_CTRL_MME_GET(n) \
    (((n) >> PCI_MSI_MSG_CTRL_MME_BIT) & PCI_MSI_MSG_CTRL_MME_MASK)
#define PCI_MSI_MSG_CTRL_MMC_GET(n) \
    (((n) >> PCI_MSI_MSG_CTRL_MMC_BIT) & PCI_MSI_MSG_CTRL_MMC_MASK)
#define PCI_MSI_MSG_CTRL_EN_GET(n) \
    (((n) >> PCI_MSI_MSG_CTRL_EN_BIT) & PCI_MSI_MSG_CTRL_EN_MASK)

#define PCI_MSI_MSG_CTRL_VMASK_SET(r,n) \
    ((r) = ((r) & ~PCI_MSI_MSG_CTRL_VMASK) | PCI_MSI_MSG_CTRL_VMASK_n((n)))
#define PCI_MSI_MSG_CTRL_CAP64_SET(r,n) \
    ((r) = ((r) & ~PCI_MSI_MSG_CTRL_CAP64) | PCI_MSI_MSG_CTRL_CAP64_n((n)))
#define PCI_MSI_MSG_CTRL_MME_SET(r,n) \
    ((r) = ((r) & ~PCI_MSI_MSG_CTRL_MME) | PCI_MSI_MSG_CTRL_MME_n((n)))
#define PCI_MSI_MSG_CTRL_MMC_SET(r,n) \
    ((r) = ((r) & ~PCI_MSI_MSG_CTRL_MMC) | PCI_MSI_MSG_CTRL_MMC_n((n)))
#define PCI_MSI_MSG_CTRL_EN_SET(r,n) \
    ((r) = ((r) & ~PCI_MSI_MSG_CTRL_EN) | PCI_MSI_MSG_CTRL_EN_n((n)))

// PCI_MSIX_MSG_CTRL

#define PCI_MSIX_MSG_CTRL_EN_BIT          15
#define PCI_MSIX_MSG_CTRL_MASK_BIT        14
#define PCI_MSIX_MSG_CTRL_TBLSZ_BIT       0

#define PCI_MSIX_MSG_CTRL_EN_BITS         1
#define PCI_MSIX_MSG_CTRL_MASK_BITS       1
#define PCI_MSIX_MSG_CTRL_TBLSZ_BITS      11
#define PCI_MSIX_MSG_CTRL_EN_MASK         ((1U << PCI_MSIX_MSG_CTRL_EN_BITS)-1)
#define PCI_MSIX_MSG_CTRL_MASK_MASK \
    ((1U << PCI_MSIX_MSG_CTRL_MASK_BITS)-1)
#define PCI_MSIX_MSG_CTRL_TBLSZ_MASK \
    ((1U << PCI_MSIX_MSG_CTRL_TBLSZ_BITS)-1)

// Enable
#define PCI_MSIX_MSG_CTRL_EN \
    (PCI_MSIX_MSG_CTRL_EN_MASK << PCI_MSIX_MSG_CTRL_EN_BIT)

// Function mask
#define PCI_MSIX_MSG_CTRL_MASK \
    (PCI_MSIX_MSG_CTRL_MASK_MASK << PCI_MSIX_MSG_CTRL_MASK_BIT)

// Table size
#define PCI_MSIX_MSG_CTRL_TBLSZ \
    (PCI_MSIX_MSG_CTRL_TBLSZ_MASK << PCI_MSIX_MSG_CTRL_TBLSZ_BIT)

#define PCI_MSIX_MSG_CTRL_EN_n(n)         ((n) << PCI_MSIX_MSG_CTRL_EN_BIT)
#define PCI_MSIX_MSG_CTRL_MASK_n(n)       ((n) << PCI_MSIX_MSG_CTRL_MASK_BIT)
#define PCI_MSIX_MSG_CTRL_TBLSZ_n(n)      ((n) << PCI_MSIX_MSG_CTRL_TBLSZ_BIT)

#define PCI_MSIX_MSG_CTRL_EN_GET(n) \
    (((n) >> PCI_MSIX_MSG_CTRL_EN_BIT) & PCI_MSIX_MSG_CTRL_EN_MASK)
#define PCI_MSIX_MSG_CTRL_MASK_GET(n) \
    (((n) >> PCI_MSIX_MSG_CTRL_MASK_BIT) & PCI_MSIX_MSG_CTRL_MASK_MASK)
#define PCI_MSIX_MSG_CTRL_TBLSZ_GET(n) \
    (((n) >> PCI_MSIX_MSG_CTRL_TBLSZ_BIT) & PCI_MSIX_MSG_CTRL_TBLSZ_MASK)

#define PCI_MSIX_MSG_CTRL_EN_SET(r,n) \
    ((r) = ((r) & ~PCI_MSIX_MSG_CTRL_EN) | PCI_MSIX_MSG_CTRL_EN_n((n)))
#define PCI_MSIX_MSG_CTRL_MASK_SET(r,n) \
    ((r) = ((r) & ~PCI_MSIX_MSG_CTRL_MASK) | PCI_MSIX_MSG_CTRL_MASK_n((n)))
#define PCI_MSIX_MSG_CTRL_TBLSZ_SET(r,n) \
    ((r) = ((r) & ~PCI_MSIX_MSG_CTRL_TBLSZ) | PCI_MSIX_MSG_CTRL_TBLSZ_n((n)))

// PCI_MSIX_TBL
#define PCI_MSIX_TBL               4

#define PCI_MSIX_TBL_OFS_BIT       3
#define PCI_MSIX_TBL_BIR_BIT       0

#define PCI_MSIX_TBL_OFS_BITS      29
#define PCI_MSIX_TBL_BIR_BITS      3
#define PCI_MSIX_TBL_OFS_MASK      ((1U << PCI_MSIX_TBL_OFS_BITS)-1)
#define PCI_MSIX_TBL_BIR_MASK      ((1U << PCI_MSIX_TBL_BIR_BITS)-1)

// Table offset
#define PCI_MSIX_TBL_OFS \
    (PCI_MSIX_TBL_OFS_MASK << PCI_MSIX_TBL_OFS_BIT)

// BAR indicator register
#define PCI_MSIX_TBL_BIR \
    (PCI_MSIX_TBL_BIR_MASK << PCI_MSIX_TBL_BIR_BIT)

#define PCI_MSIX_TBL_OFS_n(n)      ((n) << PCI_MSIX_TBL_OFS_BIT)
#define PCI_MSIX_TBL_BIR_n(n)      ((n) << PCI_MSIX_TBL_BIR_BIT)

#define PCI_MSIX_TBL_OFS_GET(n) \
    (((n) >> PCI_MSIX_TBL_OFS_BIT) & PCI_MSIX_TBL_OFS_MASK)
#define PCI_MSIX_TBL_BIR_GET(n) \
    (((n) >> PCI_MSIX_TBL_BIR_BIT) & PCI_MSIX_TBL_BIR_MASK)

#define PCI_MSIX_TBL_OFS_SET(r,n) \
    ((r) = ((r) & ~PCI_MSIX_TBL_OFS) | PCI_MSIX_TBL_OFS_n((n)))
#define PCI_MSIX_TBL_BIR_SET(r,n) \
    ((r) = ((r) & ~PCI_MSIX_TBL_BIR) | PCI_MSIX_TBL_BIR_n((n)))

// PCI_MSIX_PBA
#define PCI_MSIX_PBA               8

#define PCI_MSIX_PBA_OFS_BIT       3
#define PCI_MSIX_PBA_BIR_BIT       0

#define PCI_MSIX_PBA_OFS_BITS      29
#define PCI_MSIX_PBA_BIR_BITS      3
#define PCI_MSIX_PBA_OFS_MASK      ((1U << PCI_MSIX_PBA_OFS_BITS)-1)
#define PCI_MSIX_PBA_BIR_MASK      ((1U << PCI_MSIX_PBA_BIR_BITS)-1)

// Table offset
#define PCI_MSIX_PBA_OFS \
    (PCI_MSIX_PBA_OFS_MASK << PCI_MSIX_PBA_OFS_BIT)

// BAR indicator register
#define PCI_MSIX_PBA_BIR \
    (PCI_MSIX_PBA_BIR_MASK << PCI_MSIX_PBA_BIR_BIT)

#define PCI_MSIX_PBA_OFS_n(n)      ((n) << PCI_MSIX_PBA_OFS_BIT)
#define PCI_MSIX_PBA_BIR_n(n)      ((n) << PCI_MSIX_PBA_BIR_BIT)

#define PCI_MSIX_PBA_OFS_GET(n) \
    (((n) >> PCI_MSIX_PBA_OFS_BIT) & PCI_MSIX_PBA_OFS_MASK)
#define PCI_MSIX_PBA_BIR_GET(n) \
    (((n) >> PCI_MSIX_PBA_BIR_BIT) & PCI_MSIX_PBA_BIR_MASK)

#define PCI_MSIX_PBA_OFS_SET(r,n) \
    ((r) = ((r) & ~PCI_MSIX_PBA_OFS) | PCI_MSIX_PBA_OFS_n((n)))
#define PCI_MSIX_PBA_BIR_SET(r,n) \
    ((r) = ((r) & ~PCI_MSIX_PBA_BIR) | PCI_MSIX_PBA_BIR_n((n)))

// PCI_MSIX_VEC_CTL
#define PCI_MSIX_VEC_CTL                   12

#define PCI_MSIX_VEC_CTL_MASKIRQ_BIT       0

#define PCI_MSIX_VEC_CTL_MASKIRQ_BITS      1
#define PCI_MSIX_VEC_CTL_MASKIRQ_MASK \
    ((1U << PCI_MSIX_VEC_CTL_MASKIRQ_BITS)-1)

// 1=masked
#define PCI_MSIX_VEC_CTL_MASKIRQ \
    (PCI_MSIX_VEC_CTL_MASKIRQ_MASK << PCI_MSIX_VEC_CTL_MASKIRQ_BIT)

#define PCI_MSIX_VEC_CTL_MASKIRQ_n(n) \
    ((n) << PCI_MSIX_VEC_CTL_MASKIRQ_BIT)

#define PCI_MSIX_VEC_CTL_MASKIRQ_GET(n) \
    (((n) >> PCI_MSIX_VEC_CTL_MASKIRQ_BIT) & PCI_MSIX_VEC_CTL_MASKIRQ_MASK)

#define PCI_MSIX_VEC_CTL_MASKIRQ_SET(r,n) \
    ((r) = ((r) & ~PCI_MSIX_VEC_CTL_MASKIRQ) | PCI_MSIX_VEC_CTL_MASKIRQ_n((n)))

// PCI_MSIX_EXTCAP

#define PCI_MSIX_EXTCAP_CAPID_BIT         0
#define PCI_MSIX_EXTCAP_CAPVER_BIT        16
#define PCI_MSIX_EXTCAP_NEXTOFS_BIT       20

#define PCI_MSIX_EXTCAP_CAPID_BITS        16
#define PCI_MSIX_EXTCAP_CAPVER_BITS       4
#define PCI_MSIX_EXTCAP_NEXTOFS_BITS      12
#define PCI_MSIX_EXTCAP_CAPID_MASK \
    ((1U << PCI_MSIX_EXTCAP_CAPID_BITS)-1)
#define PCI_MSIX_EXTCAP_CAPVER_MASK \
    ((1U << PCI_MSIX_EXTCAP_CAPVER_BITS)-1)
#define PCI_MSIX_EXTCAP_NEXTOFS_MASK \
    ((1U << PCI_MSIX_EXTCAP_NEXTOFS_BITS)-1)
#define PCI_MSIX_EXTCAP_CAPID \
    (PCI_MSIX_EXTCAP_CAPID_MASK << PCI_MSIX_EXTCAP_CAPID_BIT)
#define PCI_MSIX_EXTCAP_CAPVER \
    (PCI_MSIX_EXTCAP_CAPVER_MASK << PCI_MSIX_EXTCAP_CAPVER_BIT)
#define PCI_MSIX_EXTCAP_NEXTOFS \
    (PCI_MSIX_EXTCAP_NEXTOFS_MASK << PCI_MSIX_EXTCAP_NEXTOFS_BIT)

#define PCI_MSIX_EXTCAP_CAPID_n(n)        ((n) << PCI_MSIX_EXTCAP_CAPID_BIT)
#define PCI_MSIX_EXTCAP_CAPVER_n(n)       ((n) << PCI_MSIX_EXTCAP_CAPVER_BIT)
#define PCI_MSIX_EXTCAP_NEXTOFS_n(n)      ((n) << PCI_MSIX_EXTCAP_NEXTOFS_BIT)

#define PCI_MSIX_EXTCAP_CAPID_GET(n) \
    (((n) >> PCI_MSIX_EXTCAP_CAPID_BIT) & PCI_MSIX_EXTCAP_CAPID_MASK)
#define PCI_MSIX_EXTCAP_CAPVER_GET(n) \
    (((n) >> PCI_MSIX_EXTCAP_CAPVER_BIT) & PCI_MSIX_EXTCAP_CAPVER_MASK)
#define PCI_MSIX_EXTCAP_NEXTOFS_GET(n) \
    (((n) >> PCI_MSIX_EXTCAP_NEXTOFS_BIT) & PCI_MSIX_EXTCAP_NEXTOFS_MASK)

#define PCI_MSIX_EXTCAP_CAPID_SET(r,n) \
    ((r) = ((r) & ~PCI_MSIX_EXTCAP_CAPID) | PCI_MSIX_EXTCAP_CAPID_n((n)))
#define PCI_MSIX_EXTCAP_CAPVER_SET(r,n) \
    ((r) = ((r) & ~PCI_MSIX_EXTCAP_CAPVER) | PCI_MSIX_EXTCAP_CAPVER_n((n)))
#define PCI_MSIX_EXTCAP_NEXTOFS_SET(r,n) \
    ((r) = ((r) & ~PCI_MSIX_EXTCAP_NEXTOFS) | PCI_MSIX_EXTCAP_NEXTOFS_n((n)))

