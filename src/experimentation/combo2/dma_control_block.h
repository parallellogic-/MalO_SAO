#pragma once

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
    virtual int getRequiredDescriptorCount(uint64_t frame_id) = 0;

    /**
     * @brief Populates the shared engine pool with sequential transactions.
     * @param frame_id How many times the out loop (irq) has been called, divided by subframe_max
     * @param pool_start Pointer to the exact slot assigned to this peripheral inside the engine.
     * @param current_index The absolute global index of this assigned slot.
     * @param data_channel The runtime hardware DMA data channel ID.
     * @param ctrl_channel The runtime hardware DMA control channel ID.
     * @param aux0_channel For syncronized operations
     * @param aux1_channel For syncronized operations
     */
    virtual void populateDescriptors(uint64_t frame_id, DmaDescriptor* pool_start, int data_channel, int aux0_channel, int aux1_channel, int ctrl_channel) = 0;

    //virtual void update(); //hook that core1 will call when idle to allow periphreals to perform work as they have it available to do
};

class ScatterGatherEngine : public IMultiDmaTransactionSource{
private:
    int _data_chan=-1;
    int _aux0_chan=-1;
    int _aux1_chan=-1;
    int _ctrl_chan=-1;
    bool _is_data_ready=0;
    
    IMultiDmaTransactionSource* _registrants[MAX_DMA_CONTROL_REGISTRANTS];
    int _registrant_count=0;

    // Master contiguous block pool shared by all peripherals.  +1 to allow for terminating DMA _ctrl command at completion
    DmaDescriptor _global_pool[MAX_DMA_CONTROL_ACTIONS + 1] __attribute__ ((aligned (16)));

    //uint32_t *target=(uint32_t*)&timer_hw->timerawl;
    //TODO: get DMA runtime:
    //write 0xFFFFFF to scratch (make it a negative number by bit inverting it)
    //XOR with timerawl
    //store in local list uint32_t timestamps[2] at index [0]
    //at end of dma operation:
    //push timerawl to index [1]
    //setup sniff (clear to 0x00) for add mode
    //pass 2 element from list through it
    //store output as runtime
    //ignore roll-over adge case since that'd require aux0 and aux1 to kick off a simultaneous read of high and low uint32_t
public:
    ScatterGatherEngine() {}

    void begin(bool is_aux);

    bool registerSource(IMultiDmaTransactionSource* source);

    void compileAndRun(uint64_t frame_id);

    int getRequiredDescriptorCount(uint64_t frame_id) override;
    void populateDescriptors(uint64_t frame_id, DmaDescriptor* pool_start, int data_channel, int aux0_channel, int aux1_channel, int ctrl_channel) override;

    bool is_dma_success(uint64_t frame_id) const;
};

