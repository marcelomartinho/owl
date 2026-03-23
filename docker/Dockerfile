FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=America/Sao_Paulo

# Chromium build dependencies
RUN apt-get update && apt-get install -y \
    git \
    python3 \
    python3-pip \
    curl \
    wget \
    lsb-release \
    sudo \
    build-essential \
    pkg-config \
    ninja-build \
    gperf \
    bison \
    flex \
    libglib2.0-dev \
    libgtk-3-dev \
    libpango1.0-dev \
    libcairo2-dev \
    libasound2-dev \
    libpulse-dev \
    libcups2-dev \
    libdbus-1-dev \
    libnss3-dev \
    libnspr4-dev \
    libxss-dev \
    libxtst-dev \
    libxrandr-dev \
    libxcomposite-dev \
    libxdamage-dev \
    libdrm-dev \
    libgbm-dev \
    libpci-dev \
    libkrb5-dev \
    libudev-dev \
    libsystemd-dev \
    libffi-dev \
    libxkbcommon-dev \
    libva-dev \
    libvulkan-dev \
    xz-utils \
    zip \
    unzip \
    p7zip-full \
    file \
    lsof \
    uuid-dev \
    ccache \
    && rm -rf /var/lib/apt/lists/*

# Create build user (Chromium doesn't like building as root)
RUN useradd -m -s /bin/bash builder && \
    echo "builder ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers

# Install depot_tools
RUN git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git /opt/depot_tools
ENV PATH="/opt/depot_tools:${PATH}"

# Pre-configure git for gclient
RUN git config --global user.email "builder@owl.local" && \
    git config --global user.name "Owl Builder" && \
    git config --global core.preloadindex true && \
    git config --global core.fscache true && \
    git config --global --add safe.directory /opt/depot_tools && \
    git config --global --add safe.directory '*'

# Make depot_tools writable by builder (gclient updates it in-place)
RUN chown -R builder:builder /opt/depot_tools

USER builder

# Configure git for builder user (the user that actually runs builds)
RUN git config --global user.email "builder@owl.local" && \
    git config --global user.name "Owl Builder" && \
    git config --global core.preloadindex true && \
    git config --global core.fscache true && \
    git config --global --add safe.directory /opt/depot_tools && \
    git config --global --add safe.directory '*'

WORKDIR /build

# Volume mount points:
# /build          - Docker volume for Chromium source + build output (~100GB)
# /owl            - Bind mount of Owl repo (read-only)

CMD ["/bin/bash"]
