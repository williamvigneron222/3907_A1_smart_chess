from ctypes import *

# cc -fPIC -shared -o ascon.so ASCON-AEAD128.cs
so_file = "./ascon.so"

ascon = CDLL(so_file)


# UInt64Array5 = c_uint64 * 5
# arr = UInt64Array5(0, 0, 0, 0, 0)

# ascon.Ascon_p(arr, 12)

# for i in range(5):
# 	print(hex(arr[i]))


Uint64Array2 = c_uint64 * 2

key = Uint64Array2(0, 0)
nonce = Uint64Array2(0, 0)
tag = Uint64Array2(0, 0);

def setKey():
	key[0] = 0
	key[1] = 0
	
def setNonce():
	nonce[0] = 0
	nonce[1] = 0
	
# check tags: I think you can '==' 
	
def encrypt():
	# TODO
	print("hello world")
	
def decrypt():
	# TODO
	print("hello world")