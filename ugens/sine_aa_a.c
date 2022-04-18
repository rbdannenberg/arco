/* ------------------------------------------------------------
author: "RBD"
name: "sine"
version: "1.0"
Code generated with Faust 2.37.3 (https://faust.grame.fr)
Compilation options: -lang c -light -es 1 -single -ftz 0
------------------------------------------------------------ */

#ifndef  __Sine_H__
#define  __Sine_H__

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif 


#ifdef __cplusplus
extern "C" {
#endif

#include <math.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
	int iVec0[2];
	int iRec0[2];
} SineSIG0;int getNumInputsSineSIG0(SineSIG0* dsp) {
	return 0;
}
int getNumOutputsSineSIG0(SineSIG0* dsp) {
	return 1;
}

static void instanceInitSineSIG0(SineSIG0* dsp, int sample_rate) {
	/* C99 loop */
	{
		int l0;
		for (l0 = 0; (l0 < 2); l0 = (l0 + 1)) {
			dsp->iVec0[l0] = 0;
		}
	}
	/* C99 loop */
	{
		int l1;
		for (l1 = 0; (l1 < 2); l1 = (l1 + 1)) {
			dsp->iRec0[l1] = 0;
		}
	}
}

static void fillSineSIG0(SineSIG0* dsp, int count, float* table) {
	/* C99 loop */
	{
		int i1;
		for (i1 = 0; (i1 < count); i1 = (i1 + 1)) {
			dsp->iVec0[0] = 1;
			dsp->iRec0[0] = ((dsp->iVec0[1] + dsp->iRec0[1]) % 65536);
			table[i1] = sinf((9.58738019e-05f * (float)dsp->iRec0[0]));
			dsp->iVec0[1] = dsp->iVec0[0];
			dsp->iRec0[1] = dsp->iRec0[0];
		}
	}
}

static float ftbl0SineSIG0[65536];

#ifndef FAUSTCLASS 
#define FAUSTCLASS Sine
#endif
#ifdef __APPLE__ 
#define exp10f __exp10f
#define exp10 __exp10
#endif

#define max(a,b) ((a < b) ? b : a)
#define min(a,b) ((a < b) ? a : b)


typedef struct {
	int fSampleRate;
	float fConst0;
	float fRec1[2];
} Sine;

int getSampleRateSine(Sine* dsp) {
	return dsp->fSampleRate;
}

int getNumInputsSine(Sine* dsp) {
	return 2;
}
int getNumOutputsSine(Sine* dsp) {
	return 1;
}

void classInitSine(int sample_rate) {
	SineSIG0* sig0 = newSineSIG0();
	instanceInitSineSIG0(sig0, sample_rate);
	fillSineSIG0(sig0, 65536, ftbl0SineSIG0);
	deleteSineSIG0(sig0);
}

void instanceResetUserInterfaceSine(Sine* dsp) {
}

void instanceClearSine(Sine* dsp) {
	/* C99 loop */
	{
		int l2;
		for (l2 = 0; (l2 < 2); l2 = (l2 + 1)) {
			dsp->fRec1[l2] = 0.0f;
		}
	}
}

void instanceConstantsSine(Sine* dsp, int sample_rate) {
	dsp->fSampleRate = sample_rate;
	dsp->fConst0 = (1.0f / fminf(192000.0f, fmaxf(1.0f, (float)dsp->fSampleRate)));
}

void instanceInitSine(Sine* dsp, int sample_rate) {
	instanceConstantsSine(dsp, sample_rate);
	instanceResetUserInterfaceSine(dsp);
	instanceClearSine(dsp);
}

void initSine(Sine* dsp, int sample_rate) {
	classInitSine(sample_rate);
	instanceInitSine(dsp, sample_rate);
}

void computeSine(Sine* dsp, int count, FAUSTFLOAT** inputs, FAUSTFLOAT** outputs) {
	FAUSTFLOAT* input0 = inputs[0];
	FAUSTFLOAT* input1 = inputs[1];
	FAUSTFLOAT* output0 = outputs[0];
	/* C99 loop */
	{
		int i0;
		for (i0 = 0; (i0 < count); i0 = (i0 + 1)) {
			float fTemp0 = (dsp->fRec1[1] + (dsp->fConst0 * (float)input0[i0]));
			dsp->fRec1[0] = (fTemp0 - floorf(fTemp0));
			output0[i0] = (FAUSTFLOAT)((float)input1[i0] * ftbl0SineSIG0[(int)(65536.0f * dsp->fRec1[0])]);
			dsp->fRec1[1] = dsp->fRec1[0];
		}
	}
}

#ifdef __cplusplus
}
#endif

#endif
