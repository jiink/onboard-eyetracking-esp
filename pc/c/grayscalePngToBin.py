from PIL import Image

def grayscale_png_to_bin(input_file, output_file):
    # Open the input PNG image
    image = Image.open(input_file).convert('L')

    # Get the image dimensions
    width, height = image.size

    # Create a binary file for writing
    with open(output_file, 'wb') as file:
        # Iterate over each pixel in the image
        for y in range(height):
            for x in range(width):
                # Get the brightness value of the pixel
                brightness = image.getpixel((x, y))

                # Write the brightness value as a byte to the binary file
                file.write(bytes([brightness]))

    print(f"Binary file '{output_file}' created successfully.")

# Usage example
input_file = 'in.png'
output_file = 'buffer.bin'
grayscale_png_to_bin(input_file, output_file)
