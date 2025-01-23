#!/usr/bin/env python3
import numpy as np
import os
import sys
from pathlib import Path

def analyze_file(filepath):
    """Read and analyze a single file."""
    try:
        with open(filepath, 'r') as f:
            numbers = [float(line.strip()) for line in f if line.strip()]
        return {
            'data': numbers,
            'mean': np.mean(numbers),
            'std': np.std(numbers, ddof=1)  # ddof=1 for sample standard deviation
        }
    except Exception as e:
        print(f"Error processing {filepath}: {str(e)}", file=sys.stderr)
        return None

def main():
    # Get all data files in current directory
    data_files = sorted(Path('data').glob('*.txt'))  # Adjust pattern as needed
    
    if not data_files:
        print("No data files found!")
        return
    
    # Process all files
    results = {}
    max_length = 0
    
    print("\nAnalyzing files:")
    print("-" * 50)
    
    for file_path in data_files:
        result = analyze_file(file_path)
        if result:
            filename = file_path.stem  # Get filename without extension
            results[filename] = result
            max_length = max(max_length, len(result['data']))
            print(f"{filename}:")
            print(f"  Mean: {result['mean']:.4f}")
            print(f"  Std Dev: {result['std']:.4f}")
            print()

    # Write CSV file
    csv_filename = 'analysis_results.csv'
    print(f"Writing results to {csv_filename}")
    
    with open(csv_filename, 'w') as f:
        # Write header
        headers = list(results.keys())
        f.write(','.join(headers) + '\n')
        
        # Write data rows
        for i in range(max_length):
            row = []
            for filename in headers:
                data = results[filename]['data']
                value = str(data[i]) if i < len(data) else ''
                row.append(value)
            f.write(','.join(row) + '\n')
    
    print("\nAnalysis complete!")
    print(f"CSV file created: {csv_filename}")

if __name__ == '__main__':
    main()
