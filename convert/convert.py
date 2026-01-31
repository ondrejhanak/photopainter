import sys
import os.path
from PIL import Image, ImagePalette, ImageOps, ImageEnhance, ImageFilter
import argparse

# Get arguments
parser = argparse.ArgumentParser(description='Process images for e-ink display.')
parser.add_argument('image_file', type=str, help='Input image file')
parser.add_argument('--dir', choices=['landscape', 'portrait'], help='Image direction (landscape or portrait)')
args = parser.parse_args()
input_filename = args.image_file
display_direction = args.dir

# Check whether the input file exists
if not os.path.isfile(input_filename):
    print(f'Error: file {input_filename} does not exist')
    sys.exit(1)

# Read input image
input_image = Image.open(input_filename)

# Get the original image size
width, height = input_image.size

# Specified target size
if display_direction:
    if display_direction == 'landscape':
        target_width, target_height = 800, 480
    else:
        target_width, target_height = 480, 800
else:
    if  width > height:
        target_width, target_height = 800, 480
    else:
        target_width, target_height = 480, 800

# Cover mode: scale to fill entire frame, crop excess from center
scale_ratio = max(target_width / width, target_height / height)

# Calculate the size after scaling
scaled_width = int(width * scale_ratio)
scaled_height = int(height * scale_ratio)

# Resize image to cover
scaled_image = input_image.resize((scaled_width, scaled_height), Image.Resampling.LANCZOS)

# Calculate crop box to center the image
left = (scaled_width - target_width) // 2
top = (scaled_height - target_height) // 2
right = left + target_width
bottom = top + target_height

# Crop to target size from center
resized_image = scaled_image.crop((left, top, right, bottom))

# Enhance image
img = resized_image
img = img.filter(ImageFilter.UnsharpMask(radius=1.0, percent=110, threshold=3))
img = ImageEnhance.Contrast(img).enhance(1.1)
img = ImageEnhance.Color(img).enhance(1.2)

# Prepare output filename
output_filename = os.path.splitext(input_filename)[0] + '_output.bmp'

# Palette approximating how the panel's inks actually look (duller than pure RGB).
# Dithering against it gives more accurate colors than dithering against pure values.
dither_pal = (
    5, 5, 5,        # black
    245, 245, 245,  # white
    65, 140, 30,    # green
    30, 65, 240,    # blue
    190, 0, 0,      # red
    255, 245, 55,   # yellow
    230, 125, 0     # orange
)

# Exact RGB values the firmware recognizes, in the same order as dither_pal.
output_pal = (
    0, 0, 0,        # black
    255, 255, 255,  # white
    0, 255, 0,      # green
    0, 0, 255,      # blue
    255, 0, 0,      # red
    255, 255, 0,    # yellow
    255, 128, 0     # orange
)

# Dither using the realistic palette, then swap in the exact output values (indices stay the same)
pal_img = Image.new("P", (1,1))
pal_img.putpalette(dither_pal)
out_img = img.quantize(dither=Image.FLOYDSTEINBERG, palette=pal_img)
out_img.putpalette(output_pal)
out_img = out_img.convert('RGB')
print(f'Image converted: {input_filename} > {output_filename}')

# Save result
out_img.save(output_filename)
