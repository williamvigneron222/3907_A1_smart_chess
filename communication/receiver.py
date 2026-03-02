
import socket

client = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
client.sendto('Hello From Client'.encode(), ("10.0.1.37", 9999))

data, addr = client.recvfrom(1024)
print(data.decode())






