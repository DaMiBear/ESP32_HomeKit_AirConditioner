set(component_srcs "src/ir_builder_rmt_nec.c"
                   )

idf_component_register(SRCS "${component_srcs}"
                       INCLUDE_DIRS "include" 
                       PRIV_INCLUDE_DIRS ""
                       PRIV_REQUIRES "driver"
                       REQUIRES "main")

