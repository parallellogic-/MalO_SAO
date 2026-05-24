#ifndef DMA_CONTROL_BLOCK_H
#define DMA_CONTROL_BLOCK_H

#define MAX_DMA_CONTROL_REGISTRANTS 8
#define MAX_DMA_CONTROL_ACTIONS 256

// Standard RP2040/RP2350 DMA Descriptors layout (this specific argument order is required by the hardware for alternate_register_mapping_0)
struct DmaDescriptor {
    const void* read_addr;
    void* write_addr;
    uint32_t transfer_count;
    uint32_t config;
};

//the order of arguments is important to writing into the memory map correctly (0x454, 0x458 addresses for ctrl, data)
struct SniffDescriptor {
    uint32_t control;
    uint32_t data;
};

// ==========================================
// 1. MULTI-TRANSACTION INTERFACE
// ==========================================

class IMultiDmaTransactionSource {
public:
    // Returns how many sequential descriptor slots this peripheral requires right now
    virtual int getRequiredDescriptorCount(uint64_t frame_id, uint8_t subframe_id, uint8_t subframe_max) = 0;

    /**
     * @brief Populates the shared engine pool with sequential transactions.
     * @param frame_id How many times the out loop (irq) has been called, divided by subframe_max
     * @param subframe_id A fractional offset within the interrupt routine
     * @param subframe_max Maximum number of fractional interrupts within a frame, ex trigger on ( subframe_id==0 ) || ( subframe_max/2 == subframe_id ), to run twice a frame at even intervals
     * @param pool_start Pointer to the exact slot assigned to this peripheral inside the engine.
     * @param current_index The absolute global index of this assigned slot.
     * @param data_channel The runtime hardware DMA data channel ID.
     * @param ctrl_channel The runtime hardware DMA control channel ID.
     * @param aux0_channel For syncronized operations
     * @param aux1_channel For syncronized operations
     */
    virtual void populateDescriptors(uint64_t frame_id, uint8_t subframe_id, uint8_t subframe_max, DmaDescriptor* pool_start, int data_channel, int aux0_channel, int aux1_channel, int ctrl_channel) = 0;
};


class ScatterGatherEngine {
private:
    int _data_chan;
    int _aux0_chan;
    int _aux1_chan;
    int _ctrl_chan;
    
    IMultiDmaTransactionSource* _registrants[MAX_DMA_CONTROL_REGISTRANTS];
    int _registrant_count;

    // Master contiguous block pool shared by all peripherals.  +1 to allow for terminating DMA _ctrl command at completion
    DmaDescriptor _global_pool[MAX_DMA_CONTROL_ACTIONS + 1] __attribute__ ((aligned (16)));

public:
    ScatterGatherEngine() : _data_chan(-1), _ctrl_chan(-1), _registrant_count(0) {}

    void begin();

    bool registerSource(IMultiDmaTransactionSource* source);

    void compileAndRun(uint64_t frame_id,uint8_t subframe_id,uint8_t subframe_max);
};

#endif