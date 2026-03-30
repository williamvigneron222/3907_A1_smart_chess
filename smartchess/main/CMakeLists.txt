
idf_component_register(
    SRCS
        
        "Audio/firmware_receiver.cpp"
        "communication/communication.cpp"
        "communication/crypto_wrap.cpp"
        "crypto/ASCON-AEAD128.c"
        "crypto/ASCON-HASH256.c"
       
    INCLUDE_DIRS
        "."
        "communication"
        "crypto"
        "Audio"
)
