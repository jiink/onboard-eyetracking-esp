from pythonosc import udp_client
import random
import time
import sys
import math

# Set the IP address and port of your VrChat instance
vrchat_ip = "127.0.0.1"  # Change this to the actual IP address of your VrChat instance
vrchat_port = 9000  # Change this to the actual port of your VrChat instance


# Create an OSC client
client = udp_client.SimpleUDPClient(vrchat_ip, vrchat_port)

def normalized_to_deg(input_tuple):
    # Scale each element in the tuple from -1.0 to 1.0 to -180 to 180
    scaled_values = (
        input_tuple[0] * 180,
        input_tuple[1] * 180,
        input_tuple[0] * 180,
        input_tuple[1] * 180
    )
    return scaled_values

def get_random_tuple():
    # Generate random numbers between -170 and 170 for each element in the tuple
    random_values = (
        random.uniform(-170, 170),
        random.uniform(-170, 170),
        random.uniform(-170, 170),
        random.uniform(-170, 170)
    )
    return random_values

# Function to send random values to the specified OSC address
def send_random_values():
    t = 0
    while True:
        t += 0.1

        # Send the random value to VrChat
        client.send_message("/tracking/eye/EyesClosedAmount", 0.0)

        norms = (
            1,
            -1
            )
        nums = (
            170,
            0,
            0,
            0
            )
        #client.send_message("/tracking/eye/LeftRightPitchYaw", normalized_to_deg(norms))
        client.send_message("/tracking/eye/LeftRightPitchYaw", get_random_tuple())

        # Print the value for reference (optional)
        print(f"{norms} : {normalized_to_deg(norms)}")
        sys.stdout.flush()

        # Sleep for a short duration before sending the next value
        time.sleep(0.1)

# Call the function to start sending random values
send_random_values()
