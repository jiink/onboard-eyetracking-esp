from PIL import Image

def hex_string_to_image(hex_string, width, height):
    # Convert the hex string to bytes
    hex_bytes = bytes.fromhex(hex_string.replace(" ", ""))

    # Create a new grayscale image
    image = Image.new("L", (width, height))

    # Set the image data from the hex bytes
    image.putdata(hex_bytes)

    return image

# Example input
input_file = "in.txt"
with open(input_file, "r") as f:
    hex_string = f.read()

# Specify the width and height of the image
width = 96
height = 96

# Convert the hex string to an image
image = hex_string_to_image(hex_string, width, height)

# Save the image to a file
image.save("grayscale_image.png")
image.show()
