# idf_component_register(
#     SRCS
#         "main.cpp"
#         "communication/communication.cpp"
#         "communication/crypto_wrap.cpp"
#         "crypto/ASCON-AEAD128.c"
#         "crypto/ASCON-HASH256.c"
#     INCLUDE_DIRS
#         "."
#         "communication"
#         "crypto"
        
# )

idf_component_register(
    SRCS "ir/testIR/testIRsend.cpp"
    INCLUDE_DIRS "." "ir/testIR"
)