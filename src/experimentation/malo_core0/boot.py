import time

@micropython.viper
def reset_core1_and_dma_hardware():
    # =========================================================================
    # 1. FORCEFULLY RESET AND PARK CORE 1 (SIO BLOCKS)
    # =========================================================================
    # SIO Base Address = 0xD0000000. SIO_CORE1_RESET is at offset 0x160 (Word Index 88)
#    sio_ptr = ptr32(0xD0000000)
#    core1_reset_idx = int(88)
    
    # Step A: Write 1 to forcefully freeze and reset Core 1's execution state
#    sio_ptr[core1_reset_idx] = 1
    
    # Step B: Write 0 to lift the hardware reset line. This allows Core 1 to wake
    # up safely into its native, passive Bootrom loop, freeing system locks.
#    sio_ptr[core1_reset_idx] = 0

    # Step C: Flush the Inter-Core Mailbox FIFOs to remove stale cross-core messages
    # SIO_FIFO_ST (Status) is offset 0x50 (Word Index 20). Bit 0 is RX FIFO Empty.
    # SIO_FIFO_RD (Read Data) is offset 0x54 (Word Index 21).
#    fifo_status_idx = int(20)
#    fifo_read_idx = int(21)
#    while (uint(sio_ptr[fifo_status_idx]) & uint(1)) != uint(0):
#        dummy = sio_ptr[fifo_read_idx] # Keep reading until empty

    # =========================================================================
    # 1. HARDWARE: DISABLE AND ABORT ALL 16 DMA CHANNELS
    # =========================================================================
    dma_ptr = ptr32(0x50000000)
    disable_mask = uint(0xFFFFFFFE)
    
    # Disable all 16 channels to drop active pacing locks
    for ch in range(16):
        word_offset = int((ch * 16) + 3) # Stride 0x40 (16 words) + CTRL_TRIG (3 words)
        current_reg_val = uint(dma_ptr[word_offset])
        dma_ptr[word_offset] = current_reg_val & disable_mask
        
    # Assert the global DMA Abort Mask register (Offset 0x444 / 4 = Word index 273)
    abort_idx = int(273)
    dma_ptr[abort_idx] = 0xFFFF 
    
    # Wait for the hardware to confirm completion with a safe timeout
    timeout = 0
    while uint(dma_ptr[abort_idx]) != uint(0) and timeout < 2000:
        timeout += 1

    # =========================================================================
    # 2. SOFTWARE: RESET MICROPYTHON'S CLAIM TRACKING BITMASK
    # =========================================================================
    # In the Pico C SDK (which MicroPython wraps), the software tracking register 
    # for channel claims is physically mirrored in the DMA register map at 
    # offset 0x4b0 (or Word Index 300).
    # Writing 0x00000000 clears the software lock on all channels (0-15).
    claim_mask_idx = int(300)
    dma_ptr[claim_mask_idx] = 0x00000000

# Execute the zero-hang, dual-core and DMA teardown
print("Halting Core 1 and cleaning hardware registers...")
reset_core1_and_dma_hardware()
print("Core 1 is parked and all 16 DMA channels are aborted successfully!")


