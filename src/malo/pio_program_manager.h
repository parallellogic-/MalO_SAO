#ifndef PIO_PROGRAM_MANAGER_H
#define PIO_PROGRAM_MANAGER_H

#include "hardware/pio.h"

class PIOProgramManager {
public:
    /**
     * @brief Safe constructor with zero hardware side effects.
     *        Can be freely copied or moved into structures during aggregate initialization.
     * @param pio_hw    The PIO hardware block instance (pio0, pio1, or pio2 on RP2350).
     * @param program   Pointer to the compiled PIO program structure.
     * @param gpio_base Optional GPIO pin base configuration window (0 or 16 for RP2350).
     */
    PIOProgramManager(PIO pio_hw, const pio_program_t* program, uint gpio_base = 0) 
        : pio(pio_hw), prog(program), offset(-1), 
          allocated_sms{false, false, false, false}, is_initialized(false), target_base(gpio_base) {}

    /**
     * @brief Destructor clears hardware resources if end() wasn't manually executed.
     */
    ~PIOProgramManager() {
        if (is_initialized) {
            end();
        }
    }

    /**
     * @brief Explicitly uploads the program to the PIO hardware instruction memory.
     *        Call this after the manager sits in its final memory location.
     */
    void begin() {
        if (offset == -1) {
            // CRITICAL: Base must be configured BEFORE instructions occupy memory 
            // otherwise the SDK rejects the call with PICO_ERROR_INVALID_STATE
            pio_set_gpio_base(pio, target_base);

            offset = pio_add_program(pio, prog);
            is_initialized = true; // Protects the hardware lifecycle
        }
    }

    /**
     * @brief Claims an available state machine on the assigned PIO hardware block.
     * @return int The assigned SM index (0 to 3), or -1 if no SM is free.
     */
    int allocate_sm() {
        // false = do not panic if the hardware is fully booked; return -1 instead
        int sm = pio_claim_unused_sm(pio, false);
        if (sm >= 0 && sm < 4) {
            allocated_sms[sm] = true;
        }
        return sm;
    }

    /**
     * @brief Fetches the uploaded program hardware address offset.
     * @return int The offset value, or -1 if begin() hasn't been called yet.
     */
    int get_offset() const {
        return offset;
    }

    /**
     * @brief Fetches the underlying raw PIO hardware block instance pointer.
     */
    PIO get_pio() const {
        return pio;
    }

    /**
     * @brief Cleanly unclaims tracked state machines and removes the program from memory.
     */
    void end() {
        for (int sm = 0; sm < 4; sm++) {
            if (allocated_sms[sm]) {
                pio_sm_unclaim(pio, sm);
                allocated_sms[sm] = false;
            }
        }
        if (offset != -1) {
            pio_remove_program(pio, prog, offset);
            offset = -1;
        }
        is_initialized = false;
    }

private:
    PIO pio;
    const pio_program_t* prog;
    int offset;
    bool allocated_sms[4];
    bool is_initialized; // Prevents temporary aggregate copies from wiping hardware
    uint target_base;    // Tracks the targeted pin base shift window (0 or 16)
};

#endif // PIO_PROGRAM_MANAGER_H
