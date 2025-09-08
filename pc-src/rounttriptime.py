import socket

UDP_IP = "0.0.0.0"
UDP_PORT = 8888
REPLY_PORT = 9999  # This must match Pico's localPort

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

print(f"Listening for UDP pings on port {UDP_PORT}...")

while True:
    data, addr = sock.recvfrom(1024)
    print(f"Received {len(data)} bytes from {addr}")

    # Echo back to Pico W
    sock.sendto(data, (addr[0], REPLY_PORT))
