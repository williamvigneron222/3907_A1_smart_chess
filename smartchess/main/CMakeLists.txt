idf_component_register(
    SRCS
    #    "infrared.c"
    "demofsm.cpp"
    "crypto/keyGen.cpp"
    "crypto/clock.cpp"
    "crypto/ASCON-AEAD128.c"
    "crypto/ASCON-HASH256.c"

    INCLUDE_DIRS
        "."
        "communication"
        "crypto"
        "Audio"
        
)

