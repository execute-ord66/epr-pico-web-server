import numpy as np

# Generate cosine lookup table for angles 0 to 45
def generate_cosine_lookup_table():
    angles = np.arange(0, 46)  # Create an array of angles from 0 to 45
    radians = np.deg2rad(angles)  # Convert degrees to radians
    cos_values = (np.cos(radians), 10)  # Calculate cosine values and round to 6 decimal places

    return cos_values

# Generate the table
cos_table = generate_cosine_lookup_table()

# Print the table
print(cos_table)
