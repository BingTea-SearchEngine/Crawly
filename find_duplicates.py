import os
import re
import sys

def map_urls_to_filenames(directory_path):
    """
    Reads the first line of each file in `directory_path`, parses out the URL,
    then stores it in a dictionary mapping that URL to the filename.
    """
    url_file_map = {}

    for filename in os.listdir(directory_path):
        if filename == "logs.txt":
            continue

        file_path = os.path.join(directory_path, filename)
        
        # Only process if it's a file (not a subdirectory)
        if os.path.isfile(file_path):
            with open(file_path, 'r', encoding='utf-8', errors='replace') as f:
                # Read the first line
                first_line = f.readline().strip()
                
                # Use a regex to extract the URL portion
                # This looks for "URL:" followed by any characters (non-greedy)
                # until we reach "Doc number" or end of line
                match = re.search(r'URL:\s*(.*?)\s*Doc number', first_line)
                
                if match:
                    url = match.group(1).strip()
                    url_file_map.setdefault(url, []).append(filename)

    return url_file_map

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(f"Usage: python {sys.argv[0]} <directory_path>")
        sys.exit(1)

    # Replace with your directory
    directory_path = sys.argv[1]
    url_map = map_urls_to_filenames(directory_path)

    # Print or further process your mapping
    for url, files in url_map.items():
        if len(files) > 1:
            print(f"URL: {url}")
            for fname in files:
                print(f"  -> {fname}")
