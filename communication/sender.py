import socket
sender = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sender.bind(('0.0.0.0', 9999))

sender.listen(3)
while True:
    sender.accept()
    receiver, addr = sender.accept()
    print(receiver.recv(1024).decode())
    sender.send('Hello from Sender'.encode())

