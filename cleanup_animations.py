import os

def clean_animations_directory(base_directory):
    # Valid file extensions for animations
    valid_extensions = ('.gif', '.mp4', '.avi', '.mov')

    # Walk through the base directory and all subdirectories
    for root, _, files in os.walk(base_directory):
        for file in files:
            file_path = os.path.join(root, file)
            
            # Remove files starting with `._`
            if file.startswith("._"):
                print(f"Removing hidden metadata file: {file_path}")
                os.remove(file_path)
            
            # Remove files with invalid extensions
            elif not file.lower().endswith(valid_extensions):
                print(f"Removing invalid file: {file_path}")
                os.remove(file_path)

if __name__ == "__main__":
    # Path to the animations directory
    animations_directory = "./animations"
    clean_animations_directory(animations_directory)
    print("Cleanup complete!")
