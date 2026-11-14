FROM ubuntu:24.04
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    xvfb xauth libgl1-mesa-dri libglx-mesa0 libegl1 libopengl0 libvulkan1 mesa-vulkan-drivers libgtk-3-0 libsecret-1-0 libfuse2 libusb-1.0-0 \
    libpulse0 libbluetooth3 imagemagick x11-utils libxrandr2 libxi6 libxcursor1 libxinerama1 libxkbcommon-x11-0 \
    && rm -rf /var/lib/apt/lists/*
