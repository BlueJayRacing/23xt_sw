idf_component_register( SRCS "example_component.cpp" # list of components
                        INCLUDE_DIRS "include"
                        REQUIRES # list of required esp idf libraries
                            driver
                        )