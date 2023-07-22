/* flsyn.h - fluidsynth interface for Arco
 *
 * Roger B. Dannenberg
 */

#include "fluidsynth.h"

extern const char *Flsyn_name;

class Flsyn : public Ugen {
public:
    fluid_settings_t *settings;
    fluid_synth_t *synth;
    int sfont_id;


    Flsyn(int id, char *path) : Ugen(id, 'a', 2) {
        settings = new_fluid_settings();
        fluid_settings_setint(settings, "synth.polyphony", 32);
        fluid_settings_setnum(settings, "synth.sample-rate", AR);
        synth = new_fluid_synth(settings);
        sfont_id = fluid_synth_sfload(synth, path, true);
    }


    ~Flsyn() {
        if (sfont_id != -1) {
            fluid_synth_sfunload(synth, sfont_id, false);
        }
        delete_fluid_synth(synth);
        delete_fluid_settings(settings);
        settings = NULL;
    }
    
    const char *classname() { return Flsyn_name; }

    void all_off(int chan) {
        fluid_synth_all_notes_off(synth, chan);
    }

    void control_change(int chan, int num, int val) {
        fluid_synth_cc(synth, chan, num, val);
    }

    void channel_pressure(int chan, int val) {
        fluid_synth_channel_pressure(synth, chan, val);
    }

    void key_pressure(int chan, int key, int val) {
        fluid_synth_key_pressure(synth, chan, key, val);
    }

    void noteoff(int chan, int key) {
        fluid_synth_noteoff(synth, chan, key);
    }

    void noteon(int chan, int key, int vel) {
        fluid_synth_noteon(synth, chan, key, vel);
    }

    void pitchbend(int chan, float val) {
        // val is in range [-1, 8192/8193] and will be
        // clipped if out of range
        int bend = int(val * 8192 + 8192.5);
        if (bend < 0) bend = 0;
        else if (bend > 16383) bend = 16383;
        fluid_synth_pitch_bend(synth, chan, bend);
    }

    void pitchsens(int chan, int val) {
        // val is range of pitch in semitones
        fluid_synth_pitch_wheel_sens(synth, chan, val);
    }

    void program_change(int chan, int program) {
        fluid_synth_program_change(synth, chan, program);
    }

    void real_run() {
        fluid_synth_write_float(synth, BL, out_samps, 0, 1, out_samps, BL, 1);
    }
};
