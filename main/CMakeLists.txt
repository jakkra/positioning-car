idf_component_register(
    SRCS "web_server.c" "main.c" "rc_receiver_rmt.c"
    INCLUDE_DIRS ""
    EMBED_FILES "data/index.html" "data/virtualjoystick.js"
)
