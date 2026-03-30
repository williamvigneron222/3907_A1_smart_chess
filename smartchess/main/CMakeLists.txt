
idf_component_register(
    SRCS
        
        "Audio/firmware_sender.cpp"
        "communication/communication.cpp"
        "communication/crypto_wrap.cpp"
        "crypto/ASCON-AEAD128.c"
        "crypto/ASCON-HASH256.c"
        "communication/communication.cpp"
        "Audio/firmware_sender.cpp"
        # "main.cpp"
        # "crypto/keygen.cpp"
        # "crypto/clock.cpp"
    INCLUDE_DIRS
        "."
        "communication"
        "crypto"
        "Audio"
)
