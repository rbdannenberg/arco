//=======================================================================
/** @file ChordDetector.h
 *  @brief ChordDetector - a class for estimating chord labels from chromagram input
 *  @author Adam Stark
 *  @copyright Copyright (C) 2008-2014  Queen Mary University of London
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
//=======================================================================

#ifndef CHORDDETECT_H
#define CHORDDETECT_H
#define NUM_CHORDS 120
#include <vector>

//=======================================================================
/** A class for estimating chord labels from chromagram input */
class ChordDetector
{
public:
    
    /** An enum describing the chord qualities used in the algorithm */
    enum ChordQuality
    {
        Minor,
        Major,
        Suspended,
        Dominant,
        Dimished5th,
        Augmented5th,
        Half_Dim
    };
    
	/** Constructor */
	ChordDetector();
    
    /** Detects the chord from a chromagram. This is the array interface
     * @param chroma an array of length 12 containing the chromagram
     */
    void detectChord (float* chroma);
	
    /** The root note of the detected chord */
	int rootNote;
    
    /** The quality of the detected chord (Major, Minor, etc) */
	int quality;
    
    /** Any other intervals that describe the chord, e.g. 7th */
	int intervals;
    
    /** Pitches of the detected chord, bit mapped in an integer
     *  For example, A minor is 1<<0 + 1<<4 + 1<<9 = 529 = 0x211
     */
    int pitches;

    /** Our current best attempt at a confidence value of the detected
     * chord, calculated using entropy*/
    double confidence;
    
	
private:
	void makeChordProfiles();
	void classifyChromagram();
	double calculateChordScore (double* chroma, double* chordProfile,
                                    double biasToUse, double N);
	int minimumIndex (double*array, int length);
    
    /** Performs softmax on the chord scores array, then calculates
     *  the entropy of the probabilities. Finally, sets confidence to
     *  1 - (normalized entropy).  This method results in very small
     *  confidence (less than 0.0001) for non-chord audio input and
     *  variable confidence (greater than 0.0005) for chords.  We have
     *  also tried another method of calculating confidence using the
     *  z-score of the predicted chord's chord score. However, there
     *  was not enough variability of confidence between chord and
     *  non-chord input audio.
     */
    void softmaxEntropyConfidence (double* chord, double* confidence);
    void calculateChordScores (double* chord, double* chromagram);
    
    /** Encodes pitches of detected chord into an int by setting corresponding bits*/
    int encodePitches(int p1, int p2, int p3, int p4);
    void minIndexToChord (int chordindex, int* rootNote, int* quality,
                          int* intervals);
    
    /** Compares two harmonic reduction algorithms, a and b, with
     * reduction constants specified by the arguments. */
    void compareAlgos(double a_fifth, double a_third, double b_fifth,
                      double b_third, bool printChromas = false);
    
	double chromagram[12];
	double chordProfiles[NUM_CHORDS][12];
	double chord[NUM_CHORDS];
	double bias;
};

#endif
