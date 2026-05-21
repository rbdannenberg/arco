# Inflection Reflection for trumpet and computer
#
# Roger Dannenberg
# 2025 - 2026

## Recording

To get a multitrack recording, I use multirec.srp. The streams to be recorded are:

  - microphone input (for reference). Source: `input_ugen`
  - flsyn after chorus. Source: `flsyn_chorus_ugen`, but needs 20x gain
  - chords after granular synthesis. Source: `gran_ugen`, but needs 24x gain
  - autofx. Source: `effects`

Note that both chords and flsyn are mixed and routed to zitarev with wet = 0.5, rt60 = 1.0.
Also, trumpet passes through effects based on drygain, which is always 0.7 in states.
The drygain also passes through revbinput_fade to zitarev, rt60 = 2.0 (variable)

We record 1 mono and 3 stereo tracks:
  - "input" (mono)
  - "flsyn" (stereo)
  - "chords" (stereo)
  - "autofx" (stereo)
  

