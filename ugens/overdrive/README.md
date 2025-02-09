# Overdrive Unit Generator 

## Overview

**overdrive.ugen** is an overdrive unit generator designed for **Arco**, implemented using **FAUST**. It processes stereo input, applying a high-pass filter, a nonlinear overdrive effect, a tone-adjustable low-pass filter, and volume control to shape the final output.

## Features

-   **High-pass filtering:** Removes unwanted low frequencies below 720 Hz.
-   **Nonlinear overdrive:** Applies cubic nonlinearity distortion.
-   **Tone control:** Adjusts the post-filter low-pass frequency.
-   **Volume control:** Scales the output level based on user-defined gain.

## Parameters

1.  **snd** **(Stereo input signal):** The audio signal to be processed.
2.  **gain** **(0.0 to 1.0):** Controls the intensity of distortion applied to the signal.
3.  **tone** **(0.0 to 1.0):** Adjusts the post-filter low-pass cutoff frequency from **350 Hz** (0.0) to **4500 Hz** (1.0).
4.  **volume** **(0.0 to 1.0):** Controls the output gain, scaling from **-40 dB** (0.0) to **+40 dB** (1.0).

## Signal Processing Chain

1.  **High-pass Filtering:** A **1st-order high-pass filter** at **720 Hz** removes excessive low frequencies.
2.  **Overdrive Effect:** A **cubic nonlinear function** is applied, shaping the sound with harmonic distortion.
3.  **Tone Adjustment:** A **1st-order low-pass filter** dynamically scales from **350 Hz to 4500 Hz** based on `tone`.
4.  **Volume Control:** The final output is **scaled exponentially** between `-40 dB` and `+40 dB`.
