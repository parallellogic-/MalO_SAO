import malo
import time
import math

class AnimationFade:
    @staticmethod
    def set_leds(frame_id):
        CHARLIPLEX_LED_COUNT = 48
        half_count = CHARLIPLEX_LED_COUNT // 2  # 24
        
        # Configure max effective LED count per channel
        malo.set_effective_led_count(0, half_count)
        malo.set_effective_led_count(1, half_count)

        # 1. Get a smoothly cycling time variable (converts ms to radians)
        # Changing the denominator (400) speeds up or slows down the overall wave
        time_phase = time.ticks_ms() / 400.0

        # Loop through all 48 LEDs
        for iter in range(CHARLIPLEX_LED_COUNT):
            # Select channel 0 for the first half, channel 1 for the second half
            channel = 0 if iter < half_count else 1
            local_iter = iter % half_count

            # 2. Add a unique offset to each LED so they fade at different times
            # Changing 0.3 spreads out or bunches up the LEDs in the pattern
            led_offset = iter * 0.3
            
            # 3. Calculate sine value (-1.0 to 1.0)
            sine_val = math.sin(-time_phase + led_offset)
            
            # 4. Convert -1.0 -> 1.0 into 0 -> 255 for standard brightness
            brightness = int((sine_val + 1.0) * 127.5)

            # Apply the brightness directly
            malo.set_charlieplex_led(0, iter, brightness)
            malo.set_charlieplex_led(1, iter, brightness)



