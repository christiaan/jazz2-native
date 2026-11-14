# cemu-xvfb plus xdotool (scripted menu clicks) and glslang/mesa tools (checking Cemu's shader dumps)
FROM cemu-xvfb
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    xdotool glslang-tools mesa-utils \
    && rm -rf /var/lib/apt/lists/*
