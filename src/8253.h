#pragma once

#include <inttypes.h>

#define WRITE_DELAY 2
#define LATCH_DELAY 1
#define READ_DELAY  0

class CounterUnit
{
    int latch_value;
    int write_state;
    int latch_mode;
    int out;
    int value;
    int mode_int;
    int bcd;
    bool armed;

    uint8_t write_lsb;
    uint8_t write_msb;
    uint16_t loadvalue;
    bool load;
    bool enabled;
    int delay;

public:
    CounterUnit() : latch_value(-1), write_state(0), value(0)
    {

    }

    void SetMode(int new_mode, int new_latch_mode, int new_bcd_mode) 
    {
        this->Count(LATCH_DELAY);
        this->delay = LATCH_DELAY;

        this->bcd = new_bcd_mode;
        if ((new_mode & 0x04) == 2) {
            this->mode_int = 2;
        } else if ((new_mode & 0x04) == 3) {
            this->mode_int = 3;
        } else {
            this->mode_int = new_mode;
        }

        switch(this->mode_int) {
            case 0:
                this->out = 0;
                this->armed = true;
                this->enabled = false;
                break;
            case 1:
                this->out = 1;
                this->enabled = false;
                this->armed = true;
                break;
            case 2:
                this->out = 1;
                this->enabled = false;
                break;
            default:
                this->out = 1;
                this->enabled = false;

                break;
        }
        this->load = false;
        this->latch_mode = new_latch_mode;
        this->write_state = 0;
    }

    void Latch(uint8_t w8) {
        this->Count(LATCH_DELAY);
        this->delay = LATCH_DELAY;
        this->latch_value = this->value;
    }

    int Count(int incycles) 
    {
        int cycles = incycles;
        int delay1 = this->delay;
        while (this->delay && cycles) {
            --this->delay;
            --cycles;
        }
        if (!cycles) return this->out;

        int scale = 1;

        switch (this->mode_int) {
            case 0: // Interrupt on terminal count
                if (this->load) {
                    this->value = this->loadvalue;
                    this->enabled = true;
                    this->armed = true;
                    this->load = false;
                    this->out = 0; 
                }
                if (this->enabled) {
                    this->value -= cycles;
                    if (this->value <= 0) {
                        if (this->armed) {
                            this->out = 1;
                            scale = -this->value + 1;
                            this->armed = false;
                        }
                        this->value += this->bcd ? 10000 : 65536;
                    }
                }
                break;
            case 1: // Programmable one-shot
                if (!this->enabled && this->load) {
                    //this->value = this->loadvalue; -- quirk!
                    this->enabled = true;
                }
                this->load = false;
                if (this->enabled && cycles > 0) {
                    this->value -= cycles;
                    if (this->value <= 0) {
                        int reload = this->loadvalue == 0 ? 
                            (this->bcd ? 10000 : 0x10000 ) : (this->loadvalue + 1);
                        this->value += reload;
                        //this->value += this->loadvalue + 1;
                    }
                }
                break;
            case 2: // Rate generator
                if (!this->enabled && this->load) {
                    this->value = this->loadvalue;
                    this->enabled = true;
                }
                this->load = false;
                if (this->enabled && cycles > 0) {
                    this->value -= cycles;
                    if (this->value <= 0) {
                        int reload = this->loadvalue == 0 ? 
                            (this->bcd ? 10000 : 0x10000 ) : this->loadvalue;
                        this->value += reload;
                        //this->value += this->loadvalue;
                    }
                }
                // out will go low for one clock pulse but in our machine it should not be 
                // audible
                break;
            case 3: // Square wave generator
                if (!this->enabled && this->load) {
                    this->value = this->loadvalue;
                    this->enabled = true;
                }
                this->load = false;
                if (this->enabled && cycles > 0) {
                    for (;--cycles >= 0;) {
                        this->value -= 
                            ((this->value == this->loadvalue) && ((this->value&1) == 1)) ? 
                            this->out == 0 ? 3 : 1 : 2; 
                        if (this->value <= 0) {
                            this->out ^= 1;

                            int reload = (this->loadvalue == 0) ? 
                                (this->bcd ? 10000 : 0x10000) : this->loadvalue;
                            this->value += reload;
                            //this->value = this->loadvalue;
                        }
                    }
                }
                break;
            case 4: // Software triggered strobe
                break;
            case 5: // Hardware triggered strobe
                break;
            default:
                break;
        }

        //return this->out * cycles;//this->out * incycles;
        return this->out * scale;
    }

    void write_value(uint8_t w8) {
        if (this->latch_mode == 3) {
            // lsb, msb             
            switch (this->write_state) {
                case 0:
                    this->write_lsb = w8;
                    this->write_state = 1;
                    break;
                case 1:
                    this->write_msb = w8;
                    this->write_state = 0;
                    this->loadvalue = ((this->write_msb << 8) & 0xffff) | 
                        (this->write_lsb & 0xff);
                    this->load = true;
                    break;
                default:
                    break;
            }
        } else if (this->latch_mode == 1) {
            // lsb only
            //this->value = (this->value & 0xff00) | w8;
            //this->value = w8;
            //this->value &= 0xffff;
            this->loadvalue = w8;//this->value;
            this->load = true;
        } else if (this->latch_mode == 2) {
            // msb only 
            //this->value = (this->value & 0x00ff) | (w8 << 8);
            this->value = w8 << 8;
            this->value &= 0xffff;
            this->loadvalue = this->value;
            this->load = true;
        }
        if (this->load) {
            if (this->bcd) {
                this->loadvalue = frombcd(this->loadvalue);
            }
            // I'm deeply sorry about the following part
            switch (this->mode_int) {
                case 0:
                    this->delay = 3; break; 
                case 1:
                    if (!this->enabled) {
                        this->delay = 3; 
                    } 
                    break;
                case 2:
                    if (!this->enabled) {
                        this->delay = 3; 
                    }
                    break;
                case 3:
                    if (!this->enabled) {
                        this->delay = 3; 
                    }
                    break;
                default:
                    this->delay = 4; 
                    break;
            }
        }
    }

    static uint16_t tobcd(uint16_t x) {
        int result = 0;
        for (int i = 0; i < 4; ++i) {
            result |= (x % 10) << (i * 4);
            x /= 10;
        }
        return result;
    }

    static uint16_t frombcd(uint16_t x) {
        int result = 0;
        for (int i = 0; i < 4; ++i) {
            int digit = (x & 0xf000) >> 12;
            if (digit > 9) digit = 9;
            result = result * 10 + digit;
            x <<= 4;
        }
        return result;
    }

    int read_value()
    {
        int value;
        switch (this->latch_mode) {
            case 0:
                // impossibru
                break;
            case 1:
                value = this->latch_value != -1 ? this->latch_value : this->value;
                this->latch_value = -1; 
                value = this->bcd ? tobcd(value) : value;
                value &= 0xff;
                break;
            case 2:
                value = this->latch_value != -1 ? this->latch_value : this->value;
                this->latch_value = -1; 
                value = this->bcd ? tobcd(value) : value;
                value = (value >> 8) & 0xff;
                break;
            case 3:
                value = this->latch_value != -1 ? this->latch_value : this->value;
                value = this->bcd ? tobcd(value) : value;
                switch(this->write_state) {
                    case 0:
                        this->write_state = 1;
                        value = value & 0xff;
                        break;
                    case 1:
                        this->latch_value = -1;
                        this->write_state = 0;
                        value = (value >> 8) & 0xff;
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
        return value;
    }
};


class I8253
{
private:
    CounterUnit counters[3];
    uint8_t control_word;

public:
    I8253() : control_word(0) {}

    void write_cw(uint8_t w8) 
    {
        int counter_set = (w8 >> 6) & 3;
        int mode_set = (w8 >> 1) & 3;
        int latch_set = (w8 >> 4) & 3;
        int bcd_set = (w8 & 1);

        CounterUnit & ctr = this->counters[counter_set];
        if (latch_set == 0) {
            ctr.Latch(latch_set);
        } else {
            ctr.SetMode(mode_set, latch_set, bcd_set);
        }
    }

    void write(int addr, uint8_t w8) {
        //console.log("8253 write " + addr + " = " + w8.toString(16));
        switch (addr & 3) {
            case 0x03:
                return this->write_cw(w8);
            default:
                return this->counters[addr & 3].write_value(w8);
        }
    }

    int read(int addr) {
        switch (addr & 3) {
            case 0x03:
                return this->control_word;
            default:
                return this->counters[addr & 3].read_value();
        }
    }

    int Count(int cycles) {
        return this->counters[0].Count(cycles) +
            this->counters[1].Count(cycles) +
            this->counters[2].Count(cycles);
    }
};

class TimerWrapper
{
private:
    I8253 & timer;
    int sound;
    int average_count;
    int last_sound;
public:
    TimerWrapper(I8253 & _timer) : timer(_timer),
        sound(0), average_count(0), last_sound(0)
    {
    }

    int step(int cycles)
    {
        this->last_sound = this->timer.Count(cycles) / cycles;
        return this->last_sound;
    }

    int unload() 
    {
        float result = this->sound / this->average_count;
        this->sound = this->average_count = 0;
        return result - 1.5;
    }
};