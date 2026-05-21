#!/usr/bin/env python3
import numpy as np
import os
import sys
from pathlib import Path

order = ["arco2", "arco4", "arco8", "arco16",
         "arco32", "arco64", "arco128", "arco256",
         "block2", "block4", "block8", "block16",
         "block32", "block64", "block128", "block256"]


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
    data_files = Path('data').glob('*.txt')  # Adjust pattern as needed
    
    if not data_files:
        print("No data files found!")
        return
    
    # Process all files
    results = {}
    max_length = 0
    
    print("\nAnalyzing files:")
    print("-" * 50)
    
    data_files = list(data_files)  # from generator to list
    sorted_paths = []
    for filename in order:
        for i in range(len(data_files)):
            if data_files[i].stem == filename:
                sorted_paths.append(data_files[i])
                del data_files[i]
                break
    if len(data_files) > 0:
        print("Unordered files:", data_files)
        sorted_paths += data_files

    headers = []
    for file_path in sorted_paths:
        result = analyze_file(file_path)
        if result:
            filename = file_path.stem  # Get filename without extension
            headers.append(filename)
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
