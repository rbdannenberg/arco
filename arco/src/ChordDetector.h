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
    
    /** Confidence of the detected chord, calculated using entropy*/
    double confidence;
    
    /** If the detection result has a confidence above a specified threshold*/
    bool shouldDisplay;
	
private:
	void makeChordProfiles();
	void classifyChromagram();
	double calculateChordScore (double* chroma, double* chordProfile, double biasToUse, double N);
	int minimumIndex (double*array, int length);
    void softmaxEntropyConfidence (double* chord, double* confidence);
    void calculateChordScores (double* chord, double* chromagram);
    void minIndexToChord (int chordindex, int* rootNote, int* quality, int* intervals);
    void compareAlgos(double a_fifth, double a_third, double b_fifth, double b_third, bool printChromas = false);
    
	double chromagram[12];
	double chordProfiles[NUM_CHORDS][12];
	double chord[NUM_CHORDS];
	double bias;
};

#endif
