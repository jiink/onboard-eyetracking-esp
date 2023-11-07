from PIL import Image

def hex_string_to_image(hex_string, width, height):
    hex_data = bytes.fromhex(hex_string.replace(" ", ""))

    if len(hex_data) != width * height // 8:
        raise ValueError("Hex string length does not match image dimensions")

    image = Image.new('1', (width, height))

    for y in range(height):
        for x in range(width):
            byte_index = (y * width + x) // 8
            bit_index = 7 - (x % 8)
            pixel_value = (hex_data[byte_index] >> bit_index) & 1
            image.putpixel((x, y), pixel_value)

    return image

if __name__ == "__main__":
    hex_string = input("Enter hex string: ")
    width = 96
    height = 96

    image = hex_string_to_image(hex_string, width, height)
    image.save("blackandwhite-image.png")  # Show the image (you can also save it using image.save)
    image.show()