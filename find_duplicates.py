import os
import re
import sys

def map_urls_to_filenames(directory_path):
    """
    Reads the first line of each file in `directory_path`, parses out the URL,
    then stores it in a dictionary mapping that URL to the filename.
    """
    url_file_map = {}

    file_count = 0
    remove_count = 0
    for filename in os.listdir(directory_path):
        if filename == "logs.txt":
            continue
        file_count+=1

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
                    if url not in url_file_map:
                        url_file_map[url] = filename
                    else:
                        os.remove(file_path)
                        remove_count+=1

    return url_file_map, file_count, remove_count

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(f"Usage: python {sys.argv[0]} <directory_path>")
        sys.exit(1)

    # Replace with your directory
    directory_path = sys.argv[1]
    url_map, file_count, remove_count = map_urls_to_filenames(directory_path)

    # Print or further process your mapping
    for url, file in url_map.items():
        print(f"{url}: {file}")

    print(f"Started with {file_count}")
    print(f"Removed {remove_count}")
    print(f"{file_count-remove_count} remaining")

