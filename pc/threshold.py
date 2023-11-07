from PIL import Image

# Open the PNG image
image = Image.open('eye.png')

# Check if the image is grayscale (L mode)
if image.mode != 'L':
    print("The image is not grayscale (L mode).")
    exit()

# Get the width and height of the image
width, height = image.size

# Iterate through each pixel in the image
lowest = 255
highest = 0
for y in range(height):
    for x in range(width):
        pixel_value = image.getpixel((x, y))
        # print(f"Pixel at ({x}, {y}) has value: {pixel_value}")
        if pixel_value < lowest:
            lowest = pixel_value
        if pixel_value > highest:
            highest = pixel_value

print(f"Lowest pixel value: {lowest}")
print(f"Highest pixel value: {highest}")

thresholdedImage = Image.new('L', (width, height))
thresholdedPix = thresholdedImage.load()
thresholdProportion = 0.1
threshold = thresholdProportion * (highest - lowest) + lowest
print(f"Threshold: {threshold}")
for y in range(height):
    for x in range(width):
        pixel_value = image.getpixel((x, y))
        if pixel_value < threshold:
            thresholdedPix[x, y] = 0
        else:
            thresholdedPix[x, y] = 255

# Close the image file
image.close()
thresholdedImage.save('eye_thresholded.png')
thresholdedImage.close()