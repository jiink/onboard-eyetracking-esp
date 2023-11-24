from PIL import Image, ImageDraw

def draw_rectangles(image, blob_data):
    draw = ImageDraw.Draw(image)
    
    for line in blob_data:
        parts = line.split(":")
        
        if len(parts) >= 2:
            print(parts[0].strip())
            if parts[0].strip() == "Pupil":
                # Handle the Pupil line separately
                coordinates = tuple(map(int, parts[1].strip()[1:-1].split(",")))
                draw.rectangle([coordinates, coordinates], outline="#00FF00", width=2)
            else:
                # Handle Blob lines
                coordinates = parts[1].split("to")
                if len(coordinates) == 2:
                    start = tuple(map(int, coordinates[0].strip()[1:-1].split(",")))
                    end = tuple(map(int, coordinates[1].strip()[1:-1].split(",")))
                    draw.rectangle([start, end], outline="red")
    
    return image
def main():
    # Load the image
    image = Image.open("in.png").convert("RGB")  # Replace with your image file path

    # Read blob data from the output.txt file
    with open("blobs.txt", "r") as file:
        blob_data = file.readlines()

    # Draw rectangles on the image
    result_image = draw_rectangles(image, blob_data)

    # Save or display the result image
    result_image.save("out.png")  # Replace with your desired output file path
    result_image.show()

if __name__ == "__main__":
    main()
