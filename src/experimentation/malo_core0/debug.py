from machine import mem32

def print_all_dma_status():
    DMA_BASE = 0x50000000
    
    print("\n" + "="*85)
    print(f"{'CH':<3} | {'STATUS':<8} | {'REMAINING':<10} | {'READ ADDR':<10} | {'WRITE ADDR':<10} | {'TREQ (PACING)'}")
    print("="*85)
    
    # The RP2350 houses 16 concurrent DMA channels (0 through 15)
    for ch in range(16):
        # Calculate the 32-bit register address boundaries for each channel layout
        # Channel spacing (stride) is exactly 0x40 bytes apart
        CH_READ_ADDR   = DMA_BASE + (ch * 0x40) + 0x00
        CH_WRITE_ADDR  = DMA_BASE + (ch * 0x40) + 0x04
        CH_TRANS_COUNT = DMA_BASE + (ch * 0x40) + 0x08
        CH_CTRL_TRIG   = DMA_BASE + (ch * 0x40) + 0x0c
        
        # Read raw uint32 values out of the physical silicon registers
        ctrl  = mem32[CH_CTRL_TRIG]
        count = mem32[CH_TRANS_COUNT]
        read_a  = mem32[CH_READ_ADDR]
        write_a = mem32[CH_WRITE_ADDR]
        
        # --- Decode specific configuration bitfields out of the CTRL register ---
        is_enabled = (ctrl >> 0) & 1   # Bit 0: EN (Enable flag)
        is_busy    = (ctrl >> 24) & 1  # Bit 24: BUSY flag (active bus transfers occurring)
        treq_sel   = (ctrl >> 16) & 0x3F # Bits 21:16: TREQ_SEL hardware handshake assignment
        
        # Format the running state visibility text
        if is_busy:
            status_text = "\u001b[32mBUSY\u001b[0m"       # Green text if transferring
        elif is_enabled:
            status_text = "\u001b[33mIDLE/EN\u001b[0m"   # Yellow text if ready but waiting
        else:
            status_text = "OFF"                  # Normal formatting if unassigned
            
        # Standard hardware TREQ/DREQ shorthand decipher mapping
        treq_mapping = {
    # --- PIO0 Blocks ---
    0x00: "PIO0_TX0 Pacing Line",
    0x01: "PIO0_TX1 Pacing Line",
    0x02: "PIO0_TX2 Pacing Line",
    0x03: "PIO0_TX3 Pacing Line",
    0x04: "PIO0_RX0 Pacing Line",
    0x05: "PIO0_RX1 Pacing Line",
    0x06: "PIO0_RX2 Pacing Line",
    0x07: "PIO0_RX3 Pacing Line",

    # --- PIO1 Blocks ---
    0x08: "PIO1_TX0 Pacing Line",
    0x09: "PIO1_TX1 Pacing Line",
    0x0A: "PIO1_TX2 Pacing Line",
    0x0B: "PIO1_TX3 Pacing Line",
    0x0C: "PIO1_RX0 Pacing Line",
    0x0D: "PIO1_RX1 Pacing Line",
    0x0E: "PIO1_RX2 Pacing Line",
    0x0F: "PIO1_RX3 Pacing Line",

    # --- PIO2 Blocks (New on RP2350) ---
    0x10: "PIO2_TX0 Pacing Line",
    0x11: "PIO2_TX1 Pacing Line",
    0x12: "PIO2_TX2 Pacing Line",
    0x13: "PIO2_TX3 Pacing Line",
    0x14: "PIO2_RX0 Pacing Line",
    0x15: "PIO2_RX1 Pacing Line",
    0x16: "PIO2_RX2 Pacing Line",
    0x17: "PIO2_RX3 Pacing Line",

    # --- SPI Controllers ---
    0x18: "SPI0_TX Pacing Line",
    0x19: "SPI0_RX Pacing Line",
    0x1A: "SPI1_TX Pacing Line",
    0x1B: "SPI1_RX Pacing Line",

    # --- UART Controllers ---
    0x1C: "UART0_TX Pacing Line",
    0x1D: "UART0_RX Pacing Line",
    0x1E: "UART1_TX Pacing Line",
    0x1F: "UART1_RX Pacing Line",

    # --- PWM Slices (Expanded to 12 Slices on RP2350) ---
    0x20: "PWM_WRAP0 Pacing Line",
    0x21: "PWM_WRAP1 Pacing Line",
    0x22: "PWM_WRAP2 Pacing Line",
    0x23: "PWM_WRAP3 Pacing Line",
    0x24: "PWM_WRAP4 Pacing Line",
    0x25: "PWM_WRAP5 Pacing Line",
    0x26: "PWM_WRAP6 Pacing Line",
    0x27: "PWM_WRAP7 Pacing Line",
    0x28: "PWM_WRAP8 Pacing Line",
    0x29: "PWM_WRAP9 Pacing Line",
    0x2A: "PWM_WRAP10 Pacing Line",
    0x2B: "PWM_WRAP11 Pacing Line",

    # --- Analog & Storage Components ---
    0x2C: "ADC Pacing Line",
    0x2D: "XIP_STREAM Pacing Line",
    0x2E: "XIP_QMITX Pacing Line",
    0x2F: "XIP_QMIRX Pacing Line",

    # --- High-Speed I/O & Coprocessors (New on RP2350) ---
    0x34: "HSTX (High-Speed TX Serializer) Pacing Line",
    0x35: "CORESIGHT Debug Trace Pacing Line",
    0x36: "SHA256 Hardware Accelerator Pacing Line",

    # --- Inter-Peripheral Internal Timers (DMA Pacing Timers) ---
    0x3B: "DMA_TIMER0 Fractional Pacing Clock",
    0x3C: "DMA_TIMER1 Fractional Pacing Clock",
    0x3D: "DMA_TIMER2 Fractional Pacing Clock",
    0x3E: "DMA_TIMER3 Fractional Pacing Clock",

    # --- System Default Overrides ---
    0x3F: "Permanent On (Unpaced/Instant)"
}

        pacing_info = treq_mapping.get(treq_sel, f"Hardware ID Code: 0x{treq_sel:02X}")

        # Stream the row output down into your Thonny REPL panel terminal window
        print(f"{ch:<3} | {status_text:<14} | {count:<10} | 0x{read_a:08X} | 0x{write_a:08X} | {pacing_info}")
        
    print("="*85 + "\n")

# Run the inspector
print_all_dma_status()


