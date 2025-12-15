idf_component_register(
                SRCS 
                    # "cpp_pthread.cpp"
                    "cpp_variant.cpp"
                INCLUDE_DIRS ".")
target_compile_options(${COMPONENT_LIB} PRIVATE -std=gnu++23)