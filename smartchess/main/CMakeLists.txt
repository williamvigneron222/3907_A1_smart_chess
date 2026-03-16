idf_component_register(SRCS "main.cpp" "crypto/keyGen.cpp" "crypto/clock.cpp" "crypto/ASCON-HASH256.c" "crypto/ASCON-AEAD128.c" 
                    INCLUDE_DIRS "." "crypto" "ir" "ir/esp32-rmt-ir")