FROM ubuntu:24.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential bc bison flex \
    libelf-dev libssl-dev \
    cpio gzip xz-utils \
    python3 perl \
    git curl ca-certificates \
    musl-tools \
    rsync \
 && rm -rf /var/lib/apt/lists/*

 WORKDIR /work