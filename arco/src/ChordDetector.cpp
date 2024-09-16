//=======================================================================
/** @file ChordDetector.cpp
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

#include "ChordDetector.h"
#include <math.h>
#include <cmath>
#include <algorithm>
#include <numeric>



//=======================================================================
ChordDetector::ChordDetector()
{
	bias = 1.06;
	makeChordProfiles();
}

//=======================================================================
void ChordDetector::detectChord (float* chroma)
{
	for (int i = 0; i < 12; i++)
	{
		chromagram[i] = chroma[i];
	}

	classifyChromagram();
}


//=======================================================================
void ChordDetector::classifyChromagram()
{
	int i;
    int fifth, third;
	int chordindex;
	
    double a_fifth = 0.05; // Original: 0.1
    double a_third = 0; // Original: 0
    
    // To compare two algorithms side by side, uncomment the compareAlgos() below
    // Arguments are the constants to reduce fifths and thirds by for the two algorithms
    // For example, the current code uses 0.05, 0
    // Has optional final argument `bool printChromas` to print chromagrams
    
    // compareAlgos(0.1, 0, 0.1, 0.05, false);
    
	// remove some harmonics from chromagram
    for (i = 0; i < 12; i++)
    {
        fifth = (i + 7) % 12;
        third = (i + 4) % 12;
        
        chromagram[third] = chromagram[third] - (a_third * chromagram[i]);
        chromagram[fifth] = chromagram[fifth] - (a_fifth * chromagram[i]);
        
        if (chromagram[third] < 0)
        {
            chromagram[third] = 0;
        }
        if (chromagram[fifth] < 0)
        {
            chromagram[fifth] = 0;
        }
        
    }
    
    calculateChordScores(chord, chromagram);
    
    chordindex = minimumIndex (chord, NUM_CHORDS);
    
    softmaxEntropyConfidence(chord, &confidence);
    
    minIndexToChord(chordindex, &rootNote, &quality, &intervals, &pitches);
    

}

void ChordDetector::compareAlgos(double a_fifth, double a_third, double b_fifth, double b_third, bool printChromas)
{
    int i;
    int third;
    int fifth;
    int chordindex1, rootNote1, quality1, intervals1;
    double confidence1;
    int chordindex2, rootNote2, quality2, intervals2;
    double confidence2;
    
    double chord1[NUM_CHORDS];
    double chord2[NUM_CHORDS];
    double chromagram1[12];
    double chromagram2[12];
    
    for (i = 0; i < 12; i++)
    {
        chromagram1[i] = chromagram[i];
        chromagram2[i] = chromagram[i];
    }
    
    for (i = 0; i < 12; i++)
    {
        fifth = (i + 7) % 12;
        third = (i + 4) % 12;
        
        chromagram1[third] = chromagram1[third] - (a_third * chromagram1[i]);
        chromagram1[fifth] = chromagram1[fifth] - (a_fifth * chromagram1[i]);
        
        if (chromagram1[third] < 0)
        {
            chromagram1[third] = 0;
        }
        if (chromagram1[fifth] < 0)
        {
            chromagram1[fifth] = 0;
        }
        
    }
    
    calculateChordScores(chord1, chromagram1);
    
    chordindex1 = minimumIndex (chord1, NUM_CHORDS);
    
    softmaxEntropyConfidence(chord1, &confidence1);
    
    minIndexToChord(chordindex1, &rootNote1, &quality1, &intervals1, &pitches);
    
    
    for (i = 0; i < 12; i++)
    {
        fifth = (i + 7) % 12;
        third = (i + 4) % 12;
        
        chromagram2[third] = chromagram2[third] - (b_third * chromagram2[i]);
        chromagram2[fifth] = chromagram2[fifth] - (b_fifth * chromagram2[i]);
        
        if (chromagram2[third] < 0)
        {
            chromagram2[third] = 0;
        }
        if (chromagram2[fifth] < 0)
        {
            chromagram2[fifth] = 0;
        }
    }
    
    if (printChromas) {
        printf("Algo 1 chroma: \n");
        for (i = 0; i < 12; i++)
        {
            printf("%f ", chromagram1[i]);
        }
        printf("\n");
        
        printf("Algo 2 chroma: \n");
        for (i = 0; i < 12; i++)
        {
            printf("%f ", chromagram2[i]);
        }
        printf("\n");
    }
    
    
    calculateChordScores(chord2, chromagram2);
    chordindex2 = minimumIndex(chord2, NUM_CHORDS);
    
    softmaxEntropyConfidence(chord2, &confidence2);
    minIndexToChord(chordindex2, &rootNote2, &quality2, &intervals2, &pitches);
    
    const char* qualities[] = {"Minor", "Major","Suspended", "Dominant","Diminished 5th", "Augmented 5th", "Half-Dim"};
    const char* notes[] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
    
    printf("Algo 1: %s %s %d, Confidence: %f \n", notes[rootNote1], qualities[quality1], intervals1, confidence1);
    printf("Algo 2: %s %s %d, Confidence: %f \n", notes[rootNote2], qualities[quality2], intervals2, confidence2);
}
//=======================================================================
void ChordDetector::calculateChordScores (double chord[NUM_CHORDS], double chromagram[12])
{
    int j;
    // major chords
    for (j = 0; j < 12; j++)
    {
        chord[j] = calculateChordScore (chromagram, chordProfiles[j], bias, 3);
    }
    
    // minor chords
    for (j = 12; j < 24; j++)
    {
        chord[j] = calculateChordScore (chromagram, chordProfiles[j], bias, 3);
    }
    
    // diminished 5th chords
    for (j = 24; j < 36; j++)
    {
        chord[j] = calculateChordScore (chromagram, chordProfiles[j], bias, 3);
    }
    
    // augmented 5th chords
    for (j = 36; j < 48; j++)
    {
        chord[j] = calculateChordScore (chromagram, chordProfiles[j], bias, 3);
    }
    
    // sus2 chords
    for (j = 48; j < 60; j++)
    {
        chord[j] = calculateChordScore (chromagram, chordProfiles[j], 1, 3);
    }
    
    // sus4 chords
    for (j = 60; j < 72; j++)
    {
        chord[j] = calculateChordScore (chromagram, chordProfiles[j], 1, 3);
    }
    
    // major 7th chords
    for (j = 72; j < 84; j++)
    {
        chord[j] = calculateChordScore (chromagram, chordProfiles[j], 1, 4);
    }
    
    // minor 7th chords
    for (j = 84; j < 96; j++)
    {
        chord[j] = calculateChordScore (chromagram, chordProfiles[j], bias, 4);
    }

    // dominant 7th chords
    for (j = 96; j < 108; j++)
    {
        chord[j] = calculateChordScore (chromagram, chordProfiles[j], bias, 4);
    }
    
    // half-diminished chords
    for (j = 108; j < 120; j++)
    {
        chord[j] = calculateChordScore (chromagram, chordProfiles[j], bias, 4);
    }
    
}

//=======================================================================
void ChordDetector::minIndexToChord (int chordindex, int* rootNote, int* quality, int* intervals, int* pitches)
{
    // major
    if (chordindex < 12)
    {
        *rootNote = chordindex;
        *quality = Major;
        *intervals = 0;
        *pitches = encodePitches(*rootNote, (*rootNote + 4) % 12, (*rootNote + 7) % 12, -1);
    }
    
    // minor
    if ((chordindex >= 12) && (chordindex < 24))
    {
        *rootNote = chordindex-12;
        *quality = Minor;
        *intervals = 0;
        *pitches = encodePitches(*rootNote, (*rootNote + 3) % 12, (*rootNote + 7) % 12, -1);
    }
    
    // diminished 5th
    if ((chordindex >= 24) && (chordindex < 36))
    {
        *rootNote = chordindex-24;
        *quality = Dimished5th;
        *intervals = 0;
        *pitches = encodePitches(*rootNote, ((*rootNote) + 3) % 12, (*rootNote + 6) % 12, -1);
    }
    
    // augmented 5th
    if ((chordindex >= 36) && (chordindex < 48))
    {
        *rootNote = chordindex-36;
        *quality = Augmented5th;
        *intervals = 0;
        *pitches = encodePitches(*rootNote, (*rootNote + 4) % 12, (*rootNote + 8) % 12, -1);
    }
    
    // sus2
    if ((chordindex >= 48) && (chordindex < 60))
    {
        *rootNote = chordindex-48;
        *quality = Suspended;
        *intervals = 2;
        *pitches = encodePitches(*rootNote, (*rootNote + 2) % 12, (*rootNote + 7) % 12, -1);
    }
    
    // sus4
    if ((chordindex >= 60) && (chordindex < 72))
    {
        *rootNote = chordindex-60;
        *quality = Suspended;
        *intervals = 4;
        *pitches = encodePitches(*rootNote, (*rootNote + 5) % 12, (*rootNote + 7) % 12, -1);
    }
    
    // major 7th
    if ((chordindex >= 72) && (chordindex < 84))
    {
        *rootNote = chordindex-72;
        *quality = Major;
        *intervals = 7;
        *pitches = encodePitches(*rootNote, (*rootNote + 4) % 12, (*rootNote + 7) % 12, (*rootNote + 11) % 12);
    }
    
    // minor 7th
    if ((chordindex >= 84) && (chordindex < 96))
    {
        *rootNote = chordindex-84;
        *quality = Minor;
        *intervals = 7;
        *pitches = encodePitches(*rootNote, (*rootNote + 3) % 12, (*rootNote + 7) % 12, (*rootNote + 10) % 12);
    }
    
    // dominant 7th
    if ((chordindex >= 96) && (chordindex < 108))
    {
        *rootNote = chordindex-96;
        *quality = Dominant;
        *intervals = 7;
        *pitches = encodePitches(*rootNote, (*rootNote + 4) % 12, (*rootNote + 7) % 12, (*rootNote + 10) % 12);
    }
    
    // half-diminished chords
    if ((chordindex >= 108) && (chordindex < 120))
    {
        *rootNote = chordindex-108;
        *quality = Half_Dim;
        *intervals = 7;
        *pitches = encodePitches(*rootNote, (*rootNote + 3) % 12, (*rootNote + 6) % 12, (*rootNote + 10) % 12);
    }
    
}

//=======================================================================
int ChordDetector::encodePitches(int p1, int p2, int p3, int p4){
    if (p4 < 0) { // If chord has only 3 pitches
        return (1<<p1) + (1<<p2) + (1<<p3);
    }
    else {
        return (1<<p1) + (1<<p2) + (1<<p3) + (1<<p4);
    }
}

//=======================================================================
void ChordDetector::softmaxEntropyConfidence (double chord[NUM_CHORDS], double* confidence)
{
    double probabilities[NUM_CHORDS];
    double newScores[NUM_CHORDS];
    
    // Negate the scores then do softmax, since lower score means closer match to a chord.
    for (int i = 0; i < NUM_CHORDS; i++) {
        newScores[i] = -chord[i];
    }
    
    double max_score = *(std::max_element(newScores, newScores + NUM_CHORDS));
    
    // Softmax
    double sum_exp = 0.0;
    
    for (int i = 0; i < NUM_CHORDS; i++) {
        sum_exp += std::exp(newScores[i] - max_score);
    }
    
    for (int i = 0; i < NUM_CHORDS; i++) {
        probabilities[i] = std::exp(newScores[i] - max_score) / sum_exp;
    }
    
    // Entropy
    double entropy = 0.0;
    
    for (int i = 0; i < NUM_CHORDS; i++) {
        if (probabilities[i] > 0) {
            entropy -= probabilities[i] * std::log(probabilities[i]);
        }
    }
    
    entropy /= std::log(NUM_CHORDS); // Normalize with max entropy
    
    *confidence = 1 - entropy;

}

//=======================================================================
double ChordDetector::calculateChordScore (double* chroma, double* chordProfile, double biasToUse, double N)
{
	double sum = 0;
	double delta;

	for (int i = 0; i < 12; i++)
	{
		sum = sum + ((1 - chordProfile[i]) * (chroma[i] * chroma[i]));
	}

	delta = sqrt (sum) / ((12 - N) * biasToUse);
	
	return delta;
}

//=======================================================================
int ChordDetector::minimumIndex (double* array, int arrayLength)
{
	double minValue = 100000;
	int minIndex = 0;
	
	for (int i = 0;i < arrayLength;i++)
	{
		if (array[i] < minValue)
		{
			minValue = array[i];
			minIndex = i;
		}
	}
	
	return minIndex;
}

//=======================================================================
void ChordDetector::makeChordProfiles()
{
	int i;
	int t;
	int j = 0;
	int root;
	int third;
	int fifth;
	int seventh;
	
	double v1 = 1;
	double v2 = 1;
	double v3 = 1;
	
	// set profiles matrix to all zeros
	for (j = 0; j < NUM_CHORDS; j++)
	{
		for (t = 0;t < 12;t++)
		{
			chordProfiles[j][t] = 0;
		}
	}
	
	// reset j to zero to begin creating profiles
	j = 0;
	
	// major chords
	for (i = 0; i < 12; i++)
	{
		root = i % 12;
		third = (i + 4) % 12;
		fifth = (i + 7) % 12;
		
		chordProfiles[j][root] = v1;
		chordProfiles[j][third] = v2;
		chordProfiles[j][fifth] = v3;
		
		j++;				
	}

	// minor chords
	for (i = 0; i < 12; i++)
	{
		root = i % 12;
		third = (i + 3) % 12;
		fifth = (i + 7) % 12;
		
		chordProfiles[j][root] = v1;
		chordProfiles[j][third] = v2;
		chordProfiles[j][fifth] = v3;
		
		j++;				
	}

	// diminished chords
	for (i = 0; i < 12; i++)
	{
		root = i % 12;
		third = (i + 3) % 12;
		fifth = (i + 6) % 12;
		
		chordProfiles[j][root] = v1;
		chordProfiles[j][third] = v2;
		chordProfiles[j][fifth] = v3;
		
		j++;				
	}	
	
	// augmented chords
	for (i = 0; i < 12; i++)
	{
		root = i % 12;
		third = (i + 4) % 12;
		fifth = (i + 8) % 12;
		
		chordProfiles[j][root] = v1;
		chordProfiles[j][third] = v2;
		chordProfiles[j][fifth] = v3;
		
		j++;				
	}	
	
	// sus2 chords
	for (i = 0; i < 12; i++)
	{
		root = i % 12;
		third = (i + 2) % 12;
		fifth = (i + 7) % 12;
		
		chordProfiles[j][root] = v1;
		chordProfiles[j][third] = v2;
		chordProfiles[j][fifth] = v3;
		
		j++;				
	}
	
	// sus4 chords
	for (i = 0; i < 12; i++)
	{
		root = i % 12;
		third = (i + 5) % 12;
		fifth = (i + 7) % 12;
		
		chordProfiles[j][root] = v1;
		chordProfiles[j][third] = v2;
		chordProfiles[j][fifth] = v3;
		
		j++;				
	}		
	
	// major 7th chords
	for (i = 0; i < 12; i++)
	{
		root = i % 12;
		third = (i + 4) % 12;
		fifth = (i + 7) % 12;
		seventh = (i + 11) % 12;
		
		chordProfiles[j][root] = v1;
		chordProfiles[j][third] = v2;
		chordProfiles[j][fifth] = v3;
		chordProfiles[j][seventh] = v3;
		
		j++;				
	}	
	
	// minor 7th chords
	for (i = 0; i < 12; i++)
	{
		root = i % 12;
		third = (i + 3) % 12;
		fifth = (i + 7) % 12;
		seventh = (i + 10) % 12;
		
		chordProfiles[j][root] = v1;
		chordProfiles[j][third] = v2;
		chordProfiles[j][fifth] = v3;
		chordProfiles[j][seventh] = v3;
		
		j++;				
	}
	
	// dominant 7th chords
	for (i = 0; i < 12; i++)
	{
		root = i % 12;
		third = (i + 4) % 12;
		fifth = (i + 7) % 12;
		seventh = (i + 10) % 12;
		
		chordProfiles[j][root] = v1;
		chordProfiles[j][third] = v2;
		chordProfiles[j][fifth] = v3;
		chordProfiles[j][seventh] = v3;
		
		j++;				
	}
    
    // half-diminished chords (minor 7 flat 5)
    for (i = 0; i < 12; i++)
    {
        root = i % 12;
        third = (i + 3) % 12;
        fifth = (i + 6) % 12;
        seventh = (i + 10) % 12;
        
        chordProfiles[j][root] = v1;
        chordProfiles[j][third] = v2;
        chordProfiles[j][fifth] = v3;
        chordProfiles[j][seventh] = v3;
        
        j++;
    }
    
}
